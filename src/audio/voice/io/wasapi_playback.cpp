//
// wasapi_playback.cpp — default-render-endpoint WASAPI playback.
//
// Mirrors wasapi_capture.cpp in structure: notification client drives
// hotplug, event-driven wait, shared mode, AUTOCONVERTPCM fallback.
//

#include "audio/voice/io/wasapi_playback.h"

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
constexpr IID   IID_IAudioRenderClient_   = __uuidof(IAudioRenderClient);

template<class T>
void safe_release(T*& p) noexcept { if (p) { p->Release(); p = nullptr; } }

std::string wide_to_utf8(const wchar_t* w) {
    if (!w) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), needed, nullptr, nullptr);
    return out;
}

class HotplugNotifier : public IMMNotificationClient {
public:
    HotplugNotifier() : ref_(1) {}
    std::atomic<bool> render_dirty{false};

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

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
        if (flow == eRender && (role == eConsole || role == eMultimedia))
            render_dirty.store(true, std::memory_order_release);
        return S_OK;
    }
private:
    std::atomic<int> ref_;
};

} // namespace

// ---------------------------------------------------------------------------
struct PlaybackImpl {
    PlaybackConfig cfg;
    FillCallback   cb;

    std::thread        worker;
    std::atomic<bool>  running{false};
    std::atomic<bool>  stop_requested{false};

    mutable std::mutex name_mtx;
    std::string        device_name;
    std::atomic<bool>  recently_switched{false};
};

static PlaybackError worker_loop(PlaybackImpl* impl) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return PlaybackError::CoInitFailed;
    const bool co_initialized_here = (hr == S_OK || hr == S_FALSE);

    DWORD task_index = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

    IMMDeviceEnumerator* enumerator = nullptr;
    HotplugNotifier*     notifier   = nullptr;

    hr = CoCreateInstance(CLSID_MMDeviceEnumerator_, nullptr, CLSCTX_ALL,
                          IID_IMMDeviceEnumerator_, reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) {
        if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
        if (co_initialized_here) CoUninitialize();
        return PlaybackError::EnumeratorCreateFail;
    }

    notifier = new HotplugNotifier();
    enumerator->RegisterEndpointNotificationCallback(notifier);

    PlaybackError last_err = PlaybackError::ClientInitializeFail;

    while (!impl->stop_requested.load(std::memory_order_acquire)) {
        IMMDevice*           device          = nullptr;
        IAudioClient*        client          = nullptr;
        IAudioRenderClient*  render_client   = nullptr;
        HANDLE               event           = nullptr;
        WAVEFORMATEX*        mix_format      = nullptr;

        notifier->render_dirty.store(false, std::memory_order_release);

        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (FAILED(hr) || !device) { last_err = PlaybackError::GetEndpointFail; goto inner_fail; }

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
        if (FAILED(hr) || !client) { last_err = PlaybackError::ActivateClientFail; goto inner_fail; }

        { // scope-isolate variables with initializers so remaining `goto inner_fail`
          // does not cross over them (MSVC C2362).
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
            hr = client->GetMixFormat(&mix_format);
            if (FAILED(hr) || !mix_format) { last_err = PlaybackError::FormatUnsupported; goto inner_fail; }
        }

        WAVEFORMATEX* active_format = use_desired ? reinterpret_cast<WAVEFORMATEX*>(&desired) : mix_format;

        const REFERENCE_TIME buffer_hns = static_cast<REFERENCE_TIME>(impl->cfg.buffer_ms) * 10000;
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK
                                | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
                                | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                buffer_hns, 0, active_format, nullptr);
        if (FAILED(hr)) { last_err = PlaybackError::ClientInitializeFail; goto inner_fail; }

        event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event) { last_err = PlaybackError::SetEventHandleFail; goto inner_fail; }
        hr = client->SetEventHandle(event);
        if (FAILED(hr)) { last_err = PlaybackError::SetEventHandleFail; goto inner_fail; }

        hr = client->GetService(IID_IAudioRenderClient_, reinterpret_cast<void**>(&render_client));
        if (FAILED(hr) || !render_client) { last_err = PlaybackError::GetServiceFail; goto inner_fail; }

        // Pre-roll a frame of silence so the first wake doesn't starve.
        {
            UINT32 buffer_frames = 0;
            client->GetBufferSize(&buffer_frames);
            BYTE* data = nullptr;
            if (SUCCEEDED(render_client->GetBuffer(buffer_frames, &data)) && data) {
                std::memset(data, 0, buffer_frames * active_format->nBlockAlign);
                render_client->ReleaseBuffer(buffer_frames, 0);
            }
        }

        hr = client->Start();
        if (FAILED(hr)) { last_err = PlaybackError::StartStreamFail; goto inner_fail; }

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

            UINT32 buffer_frames = 0;
            client->GetBufferSize(&buffer_frames);

            std::vector<float> scratch;

            while (!impl->stop_requested.load(std::memory_order_acquire)) {
                DWORD w = WaitForSingleObject(event, 100);
                if (w != WAIT_OBJECT_0 && w != WAIT_TIMEOUT) break;

                if (notifier->render_dirty.load(std::memory_order_acquire)) {
                    impl->recently_switched.store(true, std::memory_order_release);
                    break;
                }

                UINT32 padding = 0;
                if (FAILED(client->GetCurrentPadding(&padding))) break;
                UINT32 frames_avail = buffer_frames - padding;
                if (frames_avail == 0) continue;

                BYTE* data = nullptr;
                hr = render_client->GetBuffer(frames_avail, &data);
                if (FAILED(hr) || !data) break;

                scratch.assign(static_cast<size_t>(frames_avail) * impl->cfg.channels, 0.0f);
                if (impl->cb) {
                    impl->cb(std::span<float>(scratch), frames_avail, impl->cfg.channels);
                }

                if (fmt_is_float && fmt_channels == impl->cfg.channels && fmt_sample_rate == impl->cfg.sample_rate_hz) {
                    std::memcpy(data, scratch.data(), scratch.size() * sizeof(float));
                } else if (fmt_is_float) {
                    // Endpoint wants a different channel count at the same rate (rare at
                    // eConsole role). Upmix stereo → N by copying L/R to front pair and
                    // zero elsewhere; downmix N → 1 by averaging. Rate mismatch would
                    // require a resampler on the render side; AUTOCONVERT usually handles
                    // this so we accept a brief pitch distortion if it doesn't.
                    float* dst = reinterpret_cast<float*>(data);
                    const uint32_t src_ch = impl->cfg.channels;
                    for (UINT32 i = 0; i < frames_avail; ++i) {
                        if (fmt_channels == 1) {
                            float acc = 0.0f;
                            for (uint32_t c = 0; c < src_ch; ++c) acc += scratch[i * src_ch + c];
                            dst[i] = acc / static_cast<float>(src_ch);
                        } else {
                            for (uint32_t c = 0; c < fmt_channels; ++c) {
                                dst[i * fmt_channels + c] = (c < src_ch) ? scratch[i * src_ch + c] : 0.0f;
                            }
                        }
                    }
                } else {
                    // Int16 endpoint (rare on modern Windows; AUTOCONVERT handles this
                    // if we pass float anyway, but belt-and-suspenders).
                    int16_t* dst = reinterpret_cast<int16_t*>(data);
                    const uint32_t src_ch = impl->cfg.channels;
                    for (UINT32 i = 0; i < frames_avail; ++i) {
                        for (uint32_t c = 0; c < fmt_channels; ++c) {
                            const float v = (c < src_ch) ? scratch[i * src_ch + c] : 0.0f;
                            const float clamped = std::clamp(v, -1.0f, 1.0f);
                            dst[i * fmt_channels + c] = static_cast<int16_t>(clamped * 32767.0f);
                        }
                    }
                }

                render_client->ReleaseBuffer(frames_avail, 0);
            }

            client->Stop();
        }
        } // end scope-isolation block

    inner_fail:
        safe_release(render_client);
        if (event) { CloseHandle(event); event = nullptr; }
        safe_release(client);
        if (mix_format) { CoTaskMemFree(mix_format); mix_format = nullptr; }
        safe_release(device);

        if (impl->stop_requested.load(std::memory_order_acquire)) break;
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

WasapiPlayback::WasapiPlayback() : impl_(std::make_unique<PlaybackImpl>()) {}

WasapiPlayback::~WasapiPlayback() { (void)stop(); }

Result<std::monostate, PlaybackError>
WasapiPlayback::start(const PlaybackConfig& cfg, FillCallback cb) {
    if (!cb) return Result<std::monostate, PlaybackError>::err(PlaybackError::NullCallback);
    if (impl_->running.load(std::memory_order_acquire))
        return Result<std::monostate, PlaybackError>::err(PlaybackError::ThreadAlreadyRunning);

    impl_->cfg = cfg;
    impl_->cb  = std::move(cb);
    impl_->stop_requested.store(false, std::memory_order_release);
    impl_->running.store(true, std::memory_order_release);
    impl_->worker = std::thread([this]{ (void)worker_loop(impl_.get()); });
    return Result<std::monostate, PlaybackError>::ok(std::monostate{});
}

Result<std::monostate, PlaybackError> WasapiPlayback::stop() {
    if (!impl_->worker.joinable()) {
        if (impl_->running.load(std::memory_order_acquire))
            return Result<std::monostate, PlaybackError>::err(PlaybackError::ThreadNotRunning);
        return Result<std::monostate, PlaybackError>::ok(std::monostate{});
    }
    impl_->stop_requested.store(true, std::memory_order_release);
    impl_->worker.join();
    return Result<std::monostate, PlaybackError>::ok(std::monostate{});
}

bool WasapiPlayback::is_running() const noexcept {
    return impl_->running.load(std::memory_order_acquire);
}

std::string WasapiPlayback::current_device_name() const {
    std::lock_guard<std::mutex> lk(impl_->name_mtx);
    return impl_->device_name;
}

bool WasapiPlayback::was_device_recently_switched() const noexcept {
    return impl_->recently_switched.load(std::memory_order_acquire);
}

} // namespace odyssey::audio::voice::io
