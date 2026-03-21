#pragma once

#include <type_traits>
#include <utility>

#if defined(__has_include)
#if __has_include(<expected>)
#include <expected>
#define FTCL_HAS_STD_EXPECTED 1
#endif
#endif

#ifndef FTCL_HAS_STD_EXPECTED
#include <variant>
#endif

namespace ftcl {

#ifdef FTCL_HAS_STD_EXPECTED

template <class T, class E>
using expected = std::expected<T, E>;

template <class E>
using unexpected_holder = std::unexpected<E>;

template <class E>
inline auto unexpected(E&& error) -> std::unexpected<std::decay_t<E>> {
    return std::unexpected<std::decay_t<E>>(std::forward<E>(error));
}

#else

template <class E>
class unexpected_holder {
public:
    explicit unexpected_holder(const E& error)
        : error_(error) {}

    explicit unexpected_holder(E&& error)
        : error_(std::move(error)) {}

    const E& error() const& {
        return error_;
    }

    E& error() & {
        return error_;
    }

    E&& error() && {
        return std::move(error_);
    }

private:
    E error_;
};

template <class E>
inline auto unexpected(E&& error) -> unexpected_holder<std::decay_t<E>> {
    return unexpected_holder<std::decay_t<E>>(std::forward<E>(error));
}

template <class T, class E>
class expected {
public:
    expected()
        requires std::is_default_constructible_v<T>
        : storage_(T{}) {}

    expected(const T& value)
        : storage_(value) {}

    expected(T&& value)
        : storage_(std::move(value)) {}

    template <class U>
    expected(const unexpected_holder<U>& error)
        requires std::is_constructible_v<E, const U&>
        : storage_(E(error.error())) {}

    template <class U>
    expected(unexpected_holder<U>&& error)
        requires std::is_constructible_v<E, U&&>
        : storage_(E(std::move(error).error())) {}

    bool has_value() const {
        return std::holds_alternative<T>(storage_);
    }

    explicit operator bool() const {
        return has_value();
    }

    T& value() & {
        return std::get<T>(storage_);
    }

    const T& value() const& {
        return std::get<T>(storage_);
    }

    T&& value() && {
        return std::move(std::get<T>(storage_));
    }

    E& error() & {
        return std::get<E>(storage_);
    }

    const E& error() const& {
        return std::get<E>(storage_);
    }

    E&& error() && {
        return std::move(std::get<E>(storage_));
    }

    T& operator*() & {
        return value();
    }

    const T& operator*() const& {
        return value();
    }

    T* operator->() {
        return &value();
    }

    const T* operator->() const {
        return &value();
    }

private:
    std::variant<T, E> storage_;
};

#endif

}  // namespace ftcl

