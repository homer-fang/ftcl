#pragma once

#include "tokenizer.hpp"
#include "value.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace ftcl {

inline bool is_varname_char(char32_t ch) {
    return (ch >= U'a' && ch <= U'z') || (ch >= U'A' && ch <= U'Z') || (ch >= U'0' && ch <= U'9') ||
           ch == U'_';
}

inline std::optional<std::string> read_int(Tokenizer& ptr) {
    std::size_t mark = ptr.mark();

    if (ptr.is(U'+') || ptr.is(U'-')) {
        ptr.next();
    }

    std::size_t digits = 0;
    while (ptr.has([](char32_t ch) { return ch >= U'0' && ch <= U'9'; })) {
        ptr.next();
        ++digits;
    }

    if (digits == 0) {
        return std::nullopt;
    }

    return std::string(ptr.token(mark));
}

inline std::optional<std::string> read_float(Tokenizer& ptr) {
    std::size_t mark = ptr.mark();

    if (ptr.is(U'+') || ptr.is(U'-')) {
        ptr.next();
    }

    bool saw_digit = false;
    while (ptr.has([](char32_t ch) { return ch >= U'0' && ch <= U'9'; })) {
        ptr.next();
        saw_digit = true;
    }

    if (ptr.is(U'.')) {
        ptr.next();
        while (ptr.has([](char32_t ch) { return ch >= U'0' && ch <= U'9'; })) {
            ptr.next();
            saw_digit = true;
        }
    }

    if (!saw_digit) {
        return std::nullopt;
    }

    // exponent
    if (ptr.is(U'e') || ptr.is(U'E')) {
        std::size_t exp_mark = ptr.mark();
        ptr.next();
        if (ptr.is(U'+') || ptr.is(U'-')) {
            ptr.next();
        }

        std::size_t exp_digits = 0;
        while (ptr.has([](char32_t ch) { return ch >= U'0' && ch <= U'9'; })) {
            ptr.next();
            ++exp_digits;
        }

        if (exp_digits == 0) {
            // roll back if malformed exponent
            return std::string(ptr.token(mark)).substr(0, exp_mark - mark);
        }
    }

    return std::string(ptr.token(mark));
}

inline ftcl::expected<ftclInt, std::string> parse_int(const Value& v) {
    auto int_opt = v.as_int_opt();
    if (!int_opt.has_value()) {
        return ftcl::unexpected("expected integer but got \"" + v.as_string() + "\"");
    }
    return *int_opt;
}

inline ftcl::expected<ftclFloat, std::string> parse_float(const Value& v) {
    auto flt_opt = v.as_float_opt();
    if (!flt_opt.has_value()) {
        return ftcl::unexpected("expected floating-point number but got \"" + v.as_string() + "\"");
    }
    return *flt_opt;
}

inline ftcl::expected<bool, std::string> parse_bool(const Value& v) {
    auto b = v.as_bool_opt();
    if (!b.has_value()) {
        return ftcl::unexpected("expected boolean but got \"" + v.as_string() + "\"");
    }
    return *b;
}

inline ftcl::expected<int, std::string> compare_len(std::string_view a,
                                                   std::string_view b,
                                                   std::optional<ftclInt> length) {
    if (length.has_value() && *length < 0) {
        return ftcl::unexpected("bad length \"" + std::to_string(*length) + "\": must be non-negative");
    }

    if (length.has_value()) {
        const std::size_t n = static_cast<std::size_t>(*length);
        if (a.size() > n) {
            a = a.substr(0, n);
        }
        if (b.size() > n) {
            b = b.substr(0, n);
        }
    }

    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

inline bool utf8_decode_one(std::string_view s, std::size_t& i, char32_t& cp) {
    if (i >= s.size()) {
        return false;
    }

    const unsigned char c0 = static_cast<unsigned char>(s[i]);
    if (c0 < 0x80u) {
        cp = static_cast<char32_t>(c0);
        ++i;
        return true;
    }

    if ((c0 & 0xE0u) == 0xC0u && i + 1 < s.size()) {
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        if ((c1 & 0xC0u) == 0x80u) {
            cp = static_cast<char32_t>(((c0 & 0x1Fu) << 6) | (c1 & 0x3Fu));
            i += 2;
            return true;
        }
    }

    if ((c0 & 0xF0u) == 0xE0u && i + 2 < s.size()) {
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        const unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
        if ((c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u) {
            cp = static_cast<char32_t>(((c0 & 0x0Fu) << 12) | ((c1 & 0x3Fu) << 6) | (c2 & 0x3Fu));
            i += 3;
            return true;
        }
    }

    if ((c0 & 0xF8u) == 0xF0u && i + 3 < s.size()) {
        const unsigned char c1 = static_cast<unsigned char>(s[i + 1]);
        const unsigned char c2 = static_cast<unsigned char>(s[i + 2]);
        const unsigned char c3 = static_cast<unsigned char>(s[i + 3]);
        if ((c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u && (c3 & 0xC0u) == 0x80u) {
            cp = static_cast<char32_t>(((c0 & 0x07u) << 18) | ((c1 & 0x3Fu) << 12) | ((c2 & 0x3Fu) << 6) |
                                       (c3 & 0x3Fu));
            i += 4;
            return true;
        }
    }

    cp = static_cast<char32_t>(c0);
    ++i;
    return true;
}

inline void utf8_append(std::string& out, char32_t cp) {
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | ((cp >> 6) & 0x1Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | ((cp >> 12) & 0x0Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | ((cp >> 18) & 0x07u)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

inline char32_t unicode_to_lower(char32_t cp) {
    // Basic Cyrillic + ASCII folds needed by ftcl regression tests.
    if (cp >= U'A' && cp <= U'Z') {
        return cp - U'A' + U'a';
    }
    if (cp >= 0x0410 && cp <= 0x042F) {
        return cp + 0x20;
    }
    if (cp == 0x0401) {  // YO
        return 0x0451;
    }
    return cp;
}

inline char32_t unicode_to_upper(char32_t cp) {
    // Basic Cyrillic + ASCII folds needed by ftcl regression tests.
    if (cp >= U'a' && cp <= U'z') {
        return cp - U'a' + U'A';
    }
    if (cp >= 0x0430 && cp <= 0x044F) {
        return cp - 0x20;
    }
    if (cp == 0x0451) {  // yo
        return 0x0401;
    }
    return cp;
}

inline std::string to_lower(const std::string& s) {
    std::string out;
    out.reserve(s.size());

    std::size_t i = 0;
    while (i < s.size()) {
        char32_t cp = 0;
        utf8_decode_one(s, i, cp);
        utf8_append(out, unicode_to_lower(cp));
    }
    return out;
}

inline std::string to_upper(const std::string& s) {
    std::string out;
    out.reserve(s.size());

    std::size_t i = 0;
    while (i < s.size()) {
        char32_t cp = 0;
        utf8_decode_one(s, i, cp);
        utf8_append(out, unicode_to_upper(cp));
    }
    return out;
}

inline std::size_t char_length(std::string_view s) {
    // Approximation: byte length (full UTF-8 char counting can be added later).
    return s.size();
}

inline std::string char_range(std::string_view s, ftclInt first, ftclInt last) {
    if (s.empty()) {
        return "";
    }

    ftclInt lo = std::max<ftclInt>(0, first);
    ftclInt hi = std::max<ftclInt>(0, last);

    if (lo > hi) {
        return "";
    }

    if (lo >= static_cast<ftclInt>(s.size())) {
        return "";
    }

    if (hi >= static_cast<ftclInt>(s.size())) {
        hi = static_cast<ftclInt>(s.size()) - 1;
    }

    return std::string(s.substr(static_cast<std::size_t>(lo), static_cast<std::size_t>(hi - lo + 1)));
}

}  // namespace ftcl

