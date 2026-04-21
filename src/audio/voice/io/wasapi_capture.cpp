//
// wasapi_capture.cpp — WASAPI shared-mode, communications-role mic capture.
//
// This is a pure I/O wrapper: all DSP lives upstream of the callback. The
// only interesting logic here is (1) IAudioClient lifecycle, (2) re-chunking
// WASAPI's variable-size reads into fixed 960-sample frames, and (3) device
// hotplug / invalidation recovery.
//
// References:
//   - MSDN "Capturing a Stream"
//     https://learn.microsoft.com/en-us/windows/win32/coreaudio/capturing-a-stream
//   - MSDN IMMNotificationClient
//   - Windows SDK headers: Audioclient.h, Mmdeviceapi.h, Functiondiscoverykeys_devpkey.h
//

#include "audio/voice/io/wasapi_capture.h"
#include "audio/voice/io/resample.h"

// Windows / WASAPI headers — order matters on MSVC.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <avrt.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "avrt.lib")

namespace odyssey::audio::voice::io {

namespace {

constexpr CLSID CLSID_MMDeviceEnumerator_ = __uuidof(MMDeviceEnumerator);
constexpr IID   IID_IMMDeviceEnumerator_  = __uuidof(IMMDeviceEnumerator);
constexpr IID   IID_IAudioClient_         = __uuidof(IAudioClient);
constexpr IID   IID_IAudioCaptureClient_  = __uuidof(IAudioCaptureClient);

template<class T>
void safe_release(T*& p) noexcept { if (p) { p->Release(); p = nullptr; } }

// Convert a widechar endpoint name to utf-8 for logging. Best-effort; we
// never return this back up the stack in a form that affects logic.
std::string wide_to_utf8(const wchar_t* w) {
    if (!w) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), needed, nullptr, nullptr);
    return out;
}

// ---------------------------------------------------------------------------
// HotplugNotifier — IMMNotificationClient that flips an atomic flag when the
// default communications-role capture endpoint changes. The worker thread
// polls this flag on each wake and reinitializes if set.
// ---------------------------------------------------------------------------
class HotplugNotifier : public IMMNotificationClient {
public:
    HotplugNotifier() : ref_(1) {}
    virtual ~HotplugNotifier() = default;

    std::atomic<bool> capture_dirty{false};

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) {
            *out = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(++ref_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG c = static_cast<ULONG>(--ref_);
        if (c == 0) delete this;
        return c;
    }

    // IMMNotificationClient
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR /*devId*/) override {
        if (flow == eCapture && role == eCommunications) capture_dirty.store(true, std::memory_order_release);
        return S_OK;
    }

private:
    std::atomic<int> ref_;
};

} // namespace

// ---------------------------------------------------------------------------
// CaptureImpl — private state forward-declared in the header.
// ---------------------------------------------------------------------------
struct CaptureImpl {
    // Config
    CaptureConfig   cfg;
    FrameCallback   cb;

    // Thread
    std::thread     worker;
    std::atomic<bool> running{false};
    std::atomic<bool> stop_requested{false};

    // Device name / hotplug observability
    mutable std::mutex   name_mtx;
    std::string          device_name;
    std::atomic<bool>    recently_switched{false};

    // Rechunk buffer — accumulates odd-sized WASAPI packets into 960-sample frames.
    std::vector<float>   rechunk;

    // Resampler state (only used if endpoint delivers non-48k float32).
    LinearResamplerState resampler{};
};

// ---------------------------------------------------------------------------
// worker_loop — capture thread body. One per WasapiCapture instance.
//
// Responsibilities:
//   1. CoInitialize this thread.
//   2. Enumerate default-communications capture endpoint.
//   3. Activate IAudioClient, negotiate format (request float32 48k mono;
//      fall back to endpoint default + AUTOCONVERT + our linear resampler).
//   4. Initialize in shared mode with event callback, set event handle,
//      register the notification client.
//   5. Loop waiting on the event; on wake, read all available packets from
//      IAudioCaptureClient, re-chunk to 960-sample frames, invoke cb.
//   6. On hotplug or AUDCLNT_E_DEVICE_INVALIDATED, tear down and restart.
// ---------------------------------------------------------------------------
static CaptureError worker_loop(CaptureImpl* impl) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return CaptureError::CoInitFailed;
    }
    const bool co_initialized_here = (hr == S_OK || hr == S_FALSE);

    // MMCSS bump so Windows schedules this thread at "Pro Audio" latency.
    DWORD task_index = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

    IMMDeviceEnumerator* enumerator = nullptr;
    HotplugNotifier*     notifier   = nullptr;

    hr = CoCreateInstance(CLSID_MMDeviceEnumerator_, nullptr, CLSCTX_ALL,
                          IID_IMMDeviceEnumerator_, reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) {
        if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
        if (co_initialized_here) CoUninitialize();
        return CaptureError::EnumeratorCreateFail;
    }

    notifier = new HotplugNotifier();
    enumerator->RegisterEndpointNotificationCallback(notifier);

    CaptureError last_err = CaptureError::ClientInitializeFail;

    // Outer loop re-enters on device change / invalidation.
    while (!impl->stop_requested.load(std::memory_order_acquire)) {
        IMMDevice*           device          = nullptr;
        IAudioClient*        client          = nullptr;
        IAudioCaptureClient* capture_client  = nullptr;
        HANDLE               event           = nullptr;
        WAVEFORMATEX*        mix_format      = nullptr;

        notifier->capture_dirty.store(false, std::memory_order_release);

        hr = enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &device);
        if (FAILED(hr) || !device) { last_err = CaptureError::GetEndpointFail; goto inner_fail; }

        // Pull friendly name for diagnostics.
        {
            IPropertyStore* props = nullptr;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props)) && props) {
                PROPVARIANT name; PropVariantInit(&name);
                if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &name)) && name.vt == VT_LPWSTR) {
                    std::lock_guard<std::mutex> lk(impl->name_mtx);
                    impl->device_name = wide_to_utf8(name.pwszVal);
                }
                PropVariantClear(&name);
                props->Release();
            }
        }

        hr = device->Activate(IID_IAudioClient_, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client));
        if (FAILED(hr) || !client) { last_err = CaptureError::ActivateClientFail; goto inner_fail; }

        { // scope-isolate variables with initializers so remaining `goto inner_fail`
          // does not cross over them (C++ forbids goto past a declaration with
          // a non-trivial initializer when the declaration is in scope at the label).
        // Request native float32 mono 48k. If the endpoint says no, fall
        // back to its mix format + AUTOCONVERT + our resampler.
        WAVEFORMATEXTENSIBLE desired{};
        desired.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
        desired.Format.nChannels            = static_cast<WORD>(impl->cfg.channels);
        desired.Format.nSamplesPerSec       = impl->cfg.sample_rate_hz;
        desired.Format.wBitsPerSample       = 32;
        desired.Format.nBlockAlign          = static_cast<WORD>(desired.Format.nChannels * 4);
        desired.Format.nAvgBytesPerSec      = desired.Format.nSamplesPerSec * desired.Format.nBlockAlign;
        desired.Format.cbSize               = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        desired.Samples.wValidBitsPerSample = 32;
        desired.dwChannelMask               = (impl->cfg.channels == 1) ? SPEAKER_FRONT_CENTER
                                                                         : (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT);
        desired.SubFormat                   = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

        WAVEFORMATEX* closest = nullptr;
        hr = client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
                                       reinterpret_cast<WAVEFORMATEX*>(&desired), &closest);
        bool use_desired = SUCCEEDED(hr) && hr != S_FALSE;

        if (!use_desired) {
            // Fall back to mix format; we'll convert downstream.
            hr = client->GetMixFormat(&mix_format);
            if (FAILED(hr) || !mix_format) { last_err = CaptureError::FormatUnsupported; goto inner_fail; }
        }

        WAVEFORMATEX* active_format = use_desired ? reinterpret_cast<WAVEFORMATEX*>(&desired) : mix_format;

        // 200 ms buffer, event-driven. AUTOCONVERTPCM + SRC_DEFAULT_QUALITY
        // let WASAPI reformat silently if the endpoint disagrees with
        // active_format; we still prefer native 48k float32 when available.
        const REFERENCE_TIME buffer_hns = static_cast<REFERENCE_TIME>(impl->cfg.buffer_ms) * 10000;
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK
                                | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
                                | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                buffer_hns, 0, active_format, nullptr);
        if (FAILED(hr)) { last_err = CaptureError::ClientInitializeFail; goto inner_fail; }

        event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event) { last_err = CaptureError::SetEventHandleFail; goto inner_fail; }

        hr = client->SetEventHandle(event);
        if (FAILED(hr)) { last_err = CaptureError::SetEventHandleFail; goto inner_fail; }

        hr = client->GetService(IID_IAudioCaptureClient_, reinterpret_cast<void**>(&capture_client));
        if (FAILED(hr) || !capture_client) { last_err = CaptureError::GetServiceFail; goto inner_fail; }

        hr = client->Start();
        if (FAILED(hr)) { last_err = CaptureError::StartStreamFail; goto inner_fail; }

        // Inner capture loop — wait-on-event, read-all-packets, re-chunk.
        {
            const uint32_t fmt_channels    = active_format->nChannels;
            const uint32_t fmt_sample_rate = active_format->nSamplesPerSec;
            const bool     fmt_is_float    = [&]{
                if (active_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
                if (active_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
                    const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(active_format);
                    return ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
                }
                return false;
            }();

            std::vector<float> mono_scratch;
            mono_scratch.reserve(impl->cfg.frame_samples * 8);

            while (!impl->stop_requested.load(std::memory_order_acquire)) {
                // Wait for WASAPI to signal new data. 100 ms timeout so we
                // re-check stop_requested / hotplug reasonably promptly.
                DWORD w = WaitForSingleObject(event, 100);
                if (w != WAIT_OBJECT_0 && w != WAIT_TIMEOUT) break;

                if (notifier->capture_dirty.load(std::memory_order_acquire)) {
                    impl->recently_switched.store(true, std::memory_order_release);
                    break; // bounce to outer loop to reinitialize
                }

                // Drain all packets available.
                UINT32 packet_frames = 0;
                while (SUCCEEDED(capture_client->GetNextPacketSize(&packet_frames)) && packet_frames > 0) {
                    BYTE* data = nullptr;
                    UINT32 frames = 0;
                    DWORD flags = 0;
                    UINT64 qpc = 0;
                    hr = capture_client->GetBuffer(&data, &frames, &flags, nullptr, &qpc);
                    if (FAILED(hr)) break;

                    mono_scratch.clear();
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        mono_scratch.resize(frames, 0.0f);
                    } else if (fmt_is_float) {
                        const float* src = reinterpret_cast<const float*>(data);
                        if (fmt_channels == 1) {
                            mono_scratch.assign(src, src + frames);
                        } else {
                            // Downmix to mono: average front pair. Simple and
                            // mandate-friendly (no third-party downmix matrix).
                            mono_scratch.resize(frames);
                            for (UINT32 i = 0; i < frames; ++i) {
                                float acc = 0.0f;
                                for (UINT32 c = 0; c < fmt_channels; ++c) acc += src[i * fmt_channels + c];
                                mono_scratch[i] = acc / static_cast<float>(fmt_channels);
                            }
                        }
                    } else {
                        // Int16 is the only other realistic format AUTOCONVERT hands us;
                        // handle it rather than crash.
                        const int16_t* src = reinterpret_cast<const int16_t*>(data);
                        mono_scratch.resize(frames);
                        const float inv = 1.0f / 32768.0f;
                        for (UINT32 i = 0; i < frames; ++i) {
                            float acc = 0.0f;
                            for (UINT32 c = 0; c < fmt_channels; ++c) acc += static_cast<float>(src[i * fmt_channels + c]) * inv;
                            mono_scratch[i] = acc / static_cast<float>(fmt_channels);
                        }
                    }

                    capture_client->ReleaseBuffer(frames);

                    // Resample if endpoint is not at our target rate.
                    if (fmt_sample_rate != impl->cfg.sample_rate_hz) {
                        std::vector<float> converted;
                        converted.reserve(static_cast<size_t>(frames) *
                                          impl->cfg.sample_rate_hz / fmt_sample_rate + 8);
                        (void)linear_resample_mono(mono_scratch, fmt_sample_rate,
                                                   impl->cfg.sample_rate_hz, impl->resampler, converted);
                        mono_scratch.swap(converted);
                    }

                    // Re-chunk into fixed frame_samples-sized frames.
                    impl->rechunk.insert(impl->rechunk.end(), mono_scratch.begin(), mono_scratch.end());
                    while (impl->rechunk.size() >= impl->cfg.frame_samples) {
                        if (impl->cb) {
                            std::span<const float> frame(impl->rechunk.data(), impl->cfg.frame_samples);
                            impl->cb(frame, qpc);
                        }
                        impl->rechunk.erase(impl->rechunk.begin(),
                                            impl->rechunk.begin() + impl->cfg.frame_samples);
                    }
                }
            }

            client->Stop();
        }
        } // end scope-isolation block

    inner_fail:
        safe_release(capture_client);
        if (event) { CloseHandle(event); event = nullptr; }
        safe_release(client);
        if (mix_format) { CoTaskMemFree(mix_format); mix_format = nullptr; }
        safe_release(device);

        if (impl->stop_requested.load(std::memory_order_acquire)) break;
        // Backoff before retry to avoid spinning on a persistent device error.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (enumerator && notifier) enumerator->UnregisterEndpointNotificationCallback(notifier);
    if (notifier) notifier->Release();
    safe_release(enumerator);

    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    if (co_initialized_here) CoUninitialize();

    impl->running.store(false, std::memory_order_release);
    return last_err;
}

// ---------------------------------------------------------------------------
// Public class methods
// ---------------------------------------------------------------------------
WasapiCapture::WasapiCapture() : impl_(std::make_unique<CaptureImpl>()) {}

WasapiCapture::~WasapiCapture() {
    // Best-effort stop in destructor — tests / games that forget to call
    // stop() still get a clean join.
    (void)stop();
}

Result<std::monostate, CaptureError>
WasapiCapture::start(const CaptureConfig& cfg, FrameCallback cb) {
    if (!cb) return Result<std::monostate, CaptureError>::err(CaptureError::NullCallback);
    if (impl_->running.load(std::memory_order_acquire))
        return Result<std::monostate, CaptureError>::err(CaptureError::ThreadAlreadyRunning);

    impl_->cfg = cfg;
    impl_->cb  = std::move(cb);
    impl_->rechunk.clear();
    impl_->resampler = {};
    impl_->stop_requested.store(false, std::memory_order_release);
    impl_->running.store(true, std::memory_order_release);

    impl_->worker = std::thread([this]{ (void)worker_loop(impl_.get()); });
    return Result<std::monostate, CaptureError>::ok(std::monostate{});
}

Result<std::monostate, CaptureError> WasapiCapture::stop() {
    if (!impl_->worker.joinable()) {
        if (impl_->running.load(std::memory_order_acquire)) {
            return Result<std::monostate, CaptureError>::err(CaptureError::ThreadNotRunning);
        }
        return Result<std::monostate, CaptureError>::ok(std::monostate{});
    }
    impl_->stop_requested.store(true, std::memory_order_release);
    impl_->worker.join();
    return Result<std::monostate, CaptureError>::ok(std::monostate{});
}

bool WasapiCapture::is_running() const noexcept {
    return impl_->running.load(std::memory_order_acquire);
}

std::string WasapiCapture::current_device_name() const {
    std::lock_guard<std::mutex> lk(impl_->name_mtx);
    return impl_->device_name;
}

bool WasapiCapture::was_device_recently_switched() const noexcept {
    return impl_->recently_switched.load(std::memory_order_acquire);
}

} // namespace odyssey::audio::voice::io
