#pragma once
#include <variant>
#include <string>
#include <utility>

namespace odyssey {

template<typename T, typename E = std::string>
class Result {
public:
    // Success constructor
    static Result ok(T value) { return Result(std::move(value)); }
    // Error constructor
    static Result err(E error) { return Result(error_tag{}, std::move(error)); }

    // NOTE: Use index-based std::get<0>/<1> rather than type-based std::get<T>/<E>.
    // When a caller instantiates Result<std::string> (E defaults to std::string),
    // std::variant<std::string, std::string> has T==E and std::get<T> becomes
    // ambiguous at compile time. Index-based get is always unambiguous.
    bool is_ok() const { return data_.index() == 0; }
    bool is_err() const { return data_.index() == 1; }

    const T& value() const& { return std::get<0>(data_); }
    T&& value() && { return std::get<0>(std::move(data_)); }
    const E& error() const& { return std::get<1>(data_); }

    // Monadic operations
    template<typename F>
    auto map(F&& f) const -> Result<decltype(f(std::declval<T>())), E> {
        using U = decltype(f(std::declval<T>()));
        if (is_ok()) return Result<U, E>::ok(f(value()));
        return Result<U, E>::err(error());
    }

    template<typename F>
    auto and_then(F&& f) const -> decltype(f(std::declval<T>())) {
        if (is_ok()) return f(value());
        return decltype(f(std::declval<T>()))::err(error());
    }

private:
    struct error_tag {};
    // Use std::in_place_index so these constructors remain unambiguous when T==E
    // (e.g. Result<std::string> where E defaults to std::string).
    explicit Result(T value)
        : data_(std::in_place_index<0>, std::move(value)) {}
    explicit Result(error_tag, E error)
        : data_(std::in_place_index<1>, std::move(error)) {}
    std::variant<T, E> data_;
};

} // namespace odyssey
