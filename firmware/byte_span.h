#pragma once
#include <cstddef>
#include <cstdint>

// toolchain doesn't support c++20, so manual implementation
namespace protocol {

template <typename T>
class Span {
public:
    constexpr Span() : data_(nullptr), size_(0) {}
    constexpr Span(T* data, size_t size) : data_(data), size_(size) {}

    template <size_t N>
    constexpr Span(T (&arr)[N]) : data_(arr), size_(N) {}

    // Allows Span<uint8_t> -> Span<const uint8_t>, matching std::span's
    // implicit const-qualifying conversion.
    template <typename U>
    constexpr Span(const Span<U>& other) : data_(other.data()), size_(other.size()) {}

    constexpr T* data() const { return data_; }
    constexpr size_t size() const { return size_; }
    constexpr bool empty() const { return size_ == 0; }

    constexpr T& operator[](size_t i) const noexcept { return data_[i]; }

    constexpr T* begin() const noexcept { return data_; }
    constexpr T* end() const noexcept { return data_ + size_; }

    constexpr Span subspan(size_t offset) const { return Span(data_ + offset, size_ - offset); }
    constexpr Span subspan(size_t offset, size_t count) const { return Span(data_ + offset, count); }

private:
    T* data_;
    size_t size_;
};

} // namespace protocol
