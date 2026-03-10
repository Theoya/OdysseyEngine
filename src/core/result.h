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

    bool is_ok() const { return std::holds_alternative<T>(data_); }
    bool is_err() const { return !is_ok(); }

    const T& value() const& { return std::get<T>(data_); }
    T&& value() && { return std::get<T>(std::move(data_)); }
    const E& error() const& { return std::get<E>(data_); }

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
    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(error_tag, E error) : data_(std::move(error)) {}
    std::variant<T, E> data_;
};

} // namespace odyssey
