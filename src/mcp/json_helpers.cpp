#include "mcp/json_helpers.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace odyssey::mcp::json {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

// Skip whitespace starting at pos, return new position.
size_t skip_ws(std::string_view s, size_t pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
        ++pos;
    }
    return pos;
}

// Find the position of a key "key" inside a JSON object string.
// Returns the position right after the colon following the key,
// or std::string_view::npos if not found.
size_t find_key(std::string_view json, std::string_view key) {
    // Build the search pattern: "key"
    std::string pattern;
    pattern.reserve(key.size() + 2);
    pattern += '"';
    pattern += key;
    pattern += '"';

    size_t pos = 0;
    while (pos < json.size()) {
        size_t found = json.find(pattern, pos);
        if (found == std::string_view::npos) return std::string_view::npos;

        // Advance past the key string
        size_t after_key = found + pattern.size();
        after_key = skip_ws(json, after_key);

        // Expect a colon
        if (after_key < json.size() && json[after_key] == ':') {
            return skip_ws(json, after_key + 1);
        }

        // Not a real key:value — maybe it was inside a string value.
        pos = found + 1;
    }
    return std::string_view::npos;
}

// Extract a JSON string value starting at pos (pos should point to the
// opening quote). Returns the unescaped string content.
std::optional<std::string> extract_string_at(std::string_view json, size_t pos) {
    if (pos >= json.size() || json[pos] != '"') return std::nullopt;

    std::string result;
    size_t i = pos + 1;
    while (i < json.size()) {
        char c = json[i];
        if (c == '\\' && i + 1 < json.size()) {
            char next = json[i + 1];
            switch (next) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += '\\'; result += next; break;
            }
            i += 2;
        } else if (c == '"') {
            return result;
        } else {
            result += c;
            ++i;
        }
    }
    return std::nullopt; // unterminated string
}

// Extract a raw token (number, bool, null) starting at pos up to the next
// delimiter (, } ] whitespace).
std::string_view extract_token(std::string_view json, size_t pos) {
    size_t end = pos;
    while (end < json.size()) {
        char c = json[end];
        if (c == ',' || c == '}' || c == ']' ||
            std::isspace(static_cast<unsigned char>(c))) {
            break;
        }
        ++end;
    }
    return json.substr(pos, end - pos);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public extraction functions
// ---------------------------------------------------------------------------

std::optional<std::string> get_string(std::string_view json, std::string_view key) {
    size_t pos = find_key(json, key);
    if (pos == std::string_view::npos) return std::nullopt;
    return extract_string_at(json, pos);
}

std::optional<double> get_number(std::string_view json, std::string_view key) {
    size_t pos = find_key(json, key);
    if (pos == std::string_view::npos) return std::nullopt;

    auto token = extract_token(json, pos);
    if (token.empty()) return std::nullopt;

    // Use strtod for parsing
    char* end = nullptr;
    std::string tok_str(token);
    double val = std::strtod(tok_str.c_str(), &end);
    if (end == tok_str.c_str()) return std::nullopt;
    return val;
}

std::optional<int64_t> get_int(std::string_view json, std::string_view key) {
    size_t pos = find_key(json, key);
    if (pos == std::string_view::npos) return std::nullopt;

    auto token = extract_token(json, pos);
    if (token.empty()) return std::nullopt;

    char* end = nullptr;
    std::string tok_str(token);
    int64_t val = std::strtoll(tok_str.c_str(), &end, 10);
    if (end == tok_str.c_str()) return std::nullopt;
    return val;
}

std::optional<bool> get_bool(std::string_view json, std::string_view key) {
    size_t pos = find_key(json, key);
    if (pos == std::string_view::npos) return std::nullopt;

    auto token = extract_token(json, pos);
    if (token == "true") return true;
    if (token == "false") return false;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// escape
// ---------------------------------------------------------------------------

std::string escape(std::string_view raw) {
    std::string out;
    out.reserve(raw.size() + 8);
    for (char c : raw) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Control character — emit as \u00XX
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// JsonBuilder
// ---------------------------------------------------------------------------

void JsonBuilder::maybe_comma() {
    if (needs_comma_) {
        ss_ << ',';
    }
    needs_comma_ = false;
}

JsonBuilder& JsonBuilder::begin_object() {
    maybe_comma();
    ss_ << '{';
    needs_comma_ = false;
    ++depth_;
    return *this;
}

JsonBuilder& JsonBuilder::end_object() {
    ss_ << '}';
    needs_comma_ = true;
    --depth_;
    return *this;
}

JsonBuilder& JsonBuilder::begin_array() {
    maybe_comma();
    ss_ << '[';
    needs_comma_ = false;
    ++depth_;
    return *this;
}

JsonBuilder& JsonBuilder::end_array() {
    ss_ << ']';
    needs_comma_ = true;
    --depth_;
    return *this;
}

JsonBuilder& JsonBuilder::key(std::string_view k) {
    maybe_comma();
    ss_ << '"' << escape(k) << "\":";
    needs_comma_ = false;
    return *this;
}

JsonBuilder& JsonBuilder::value(std::string_view v) {
    maybe_comma();
    ss_ << '"' << escape(v) << '"';
    needs_comma_ = true;
    return *this;
}

JsonBuilder& JsonBuilder::value(const std::string& v) {
    return value(std::string_view(v));
}

JsonBuilder& JsonBuilder::value(const char* v) {
    return value(std::string_view(v));
}

JsonBuilder& JsonBuilder::value(int64_t v) {
    maybe_comma();
    ss_ << v;
    needs_comma_ = true;
    return *this;
}

JsonBuilder& JsonBuilder::value(uint32_t v) {
    maybe_comma();
    ss_ << v;
    needs_comma_ = true;
    return *this;
}

JsonBuilder& JsonBuilder::value(double v) {
    maybe_comma();
    ss_ << v;
    needs_comma_ = true;
    return *this;
}

JsonBuilder& JsonBuilder::value(bool v) {
    maybe_comma();
    ss_ << (v ? "true" : "false");
    needs_comma_ = true;
    return *this;
}

JsonBuilder& JsonBuilder::null_value() {
    maybe_comma();
    ss_ << "null";
    needs_comma_ = true;
    return *this;
}

JsonBuilder& JsonBuilder::raw(std::string_view json_fragment) {
    maybe_comma();
    ss_ << json_fragment;
    needs_comma_ = true;
    return *this;
}

std::string JsonBuilder::build() const {
    return ss_.str();
}

// ---------------------------------------------------------------------------
// Convenience wrappers
// ---------------------------------------------------------------------------

std::string make_result(std::string_view json_content) {
    std::string out;
    out.reserve(json_content.size() + 64);
    out += R"({"jsonrpc":"2.0","result":)";
    out += json_content;
    out += '}';
    return out;
}

std::string make_error(int code, std::string_view message) {
    JsonBuilder b;
    b.begin_object()
        .key("jsonrpc").value("2.0")
        .key("error").begin_object()
            .key("code").value(static_cast<int64_t>(code))
            .key("message").value(message)
        .end_object()
    .end_object();
    return b.build();
}

} // namespace odyssey::mcp::json
