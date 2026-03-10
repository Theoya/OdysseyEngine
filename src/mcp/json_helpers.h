#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <sstream>
#include <cstdint>

namespace odyssey::mcp::json {

// Extract a string value from a JSON object string by key
std::optional<std::string> get_string(std::string_view json, std::string_view key);

// Extract a number value
std::optional<double> get_number(std::string_view json, std::string_view key);

// Extract an integer value
std::optional<int64_t> get_int(std::string_view json, std::string_view key);

// Extract a boolean value
std::optional<bool> get_bool(std::string_view json, std::string_view key);

// Escape a string for JSON output (handles quotes, backslashes, control chars)
std::string escape(std::string_view raw);

// Simple JSON builder
class JsonBuilder {
public:
    JsonBuilder& begin_object();
    JsonBuilder& end_object();
    JsonBuilder& begin_array();
    JsonBuilder& end_array();
    JsonBuilder& key(std::string_view k);
    JsonBuilder& value(std::string_view v);
    JsonBuilder& value(const std::string& v);
    JsonBuilder& value(const char* v);
    JsonBuilder& value(int64_t v);
    JsonBuilder& value(uint32_t v);
    JsonBuilder& value(double v);
    JsonBuilder& value(bool v);
    JsonBuilder& null_value();
    // Insert raw JSON (already formatted)
    JsonBuilder& raw(std::string_view json_fragment);

    std::string build() const;

private:
    std::ostringstream ss_;
    bool needs_comma_ = false;
    int depth_ = 0;

    void maybe_comma();
};

// Convenience: build a simple JSON-RPC result wrapper
std::string make_result(std::string_view json_content);

// Convenience: build a JSON-RPC error wrapper
std::string make_error(int code, std::string_view message);

} // namespace odyssey::mcp::json
