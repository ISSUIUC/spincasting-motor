#pragma once
#include <utility>

// toolchain doesn't support c++23, so manual implementation
namespace protocol {

template <typename T, typename E>
class Expected {
public:
    constexpr Expected(T value) : has_value_(true), value_(std::move(value)) {}
    constexpr Expected(E error) : has_value_(false), error_(std::move(error)) {}

    constexpr bool has_value() const { return has_value_; }
    constexpr explicit operator bool() const { return has_value_; }

    constexpr const T& operator*() const noexcept { return value_; }
    constexpr T& operator*() noexcept { return value_; }

    constexpr const E& error() const noexcept { return error_; }

private:
    bool has_value_;
    union {
        T value_;
        E error_;
    };
};

} // namespace protocol
