#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

class Tokenizer {
public:
    explicit Tokenizer(std::string_view input)
        : input_(input), index_(0) {}

    // 杩斿洖鏁翠釜杈撳叆
    std::string_view input() const {
        return input_;
    }

    // 杩斿洖褰撳墠鍓╀綑杈撳叆
    std::string_view as_str() const {
        return input_.substr(index_);
    }

    // 褰撳墠浣嶇疆锛堝瓧鑺備笅鏍囷級浣滀负 mark
    std::size_t mark() const {
        return index_;
    }

    // 浠庢寚瀹?mark 鍒版湯灏?
    std::string_view tail(std::size_t mark) const {
        assert(mark <= input_.size());
        return input_.substr(mark);
    }

    // 璇诲彇涓嬩竴涓?Unicode 鐮佺偣锛屽苟鎺ㄨ繘 index
    std::optional<char32_t> next() {
        auto d = decode_utf8_at(input_, index_);
        if (!d.has_value()) {
            return std::nullopt;
        }
        index_ += d->len;
        return d->ch;
    }

    // 鏌ョ湅涓嬩竴涓?Unicode 鐮佺偣锛屼笉鎺ㄨ繘 index
    std::optional<char32_t> peek() const {
        auto d = decode_utf8_at(input_, index_);
        if (!d.has_value()) {
            return std::nullopt;
        }
        return d->ch;
    }

    // 鍙?[mark, index_) 杩欐 token
    std::string_view token(std::size_t mark) const {
        assert(mark <= index_ && "mark follows index");
        return input_.substr(mark, index_ - mark);
    }

    // 鍙?[mark, index) 杩欐 token
    std::string_view token2(std::size_t mark, std::size_t index) const {
        assert(mark <= index && "mark follows index");
        assert(index <= input_.size());
        return input_.substr(mark, index - mark);
    }

    // 涓嬩竴涓瓧绗︽槸鍚︾瓑浜?ch
    bool is(char32_t ch) const {
        auto p = peek();
        return p.has_value() && *p == ch;
    }

    // 涓嬩竴涓瓧绗︽槸鍚︽弧瓒宠皳璇?
    template <class Pred>
    bool has(Pred pred) const {
        auto p = peek();
        return p.has_value() && pred(*p);
    }

    // 鏄惁鍒版湯灏?
    bool at_end() const {
        return !peek().has_value();
    }

    // 璺宠繃涓嬩竴涓瓧绗?
    void skip() {
        next();
    }

    // 璺宠繃鎸囧畾瀛楃锛屼笉鍖归厤灏辨柇瑷€澶辫触
    void skip_char(char32_t ch) {
        assert(is(ch));
        next();
    }

    // 璺宠繃 num_chars 涓瓧绗︼紙鎸?UTF-8 鐮佺偣鏁帮紝涓嶆槸瀛楄妭鏁帮級
    void skip_over(std::size_t num_chars) {
        for (std::size_t i = 0; i < num_chars; ++i) {
            if (!next().has_value()) {
                break;
            }
        }
    }

    // 褰撹皳璇嶄负鐪熸椂鎸佺画璺宠繃
    template <class Pred>
    void skip_while(Pred pred) {
        while (true) {
            auto p = peek();
            if (!p.has_value() || !pred(*p)) {
                break;
            }
            next();
        }
    }

    // 瑙ｆ瀽涓€涓弽鏂滄潬杞箟锛岃繑鍥炴浛鎹㈠悗鐨勫瓧绗?
    // 璋冪敤鍓嶅綋鍓嶅瓧绗﹀簲褰撴槸 '\'
    char32_t backslash_subst() {
        // 鍏堣烦杩?'\'
        skip_char(U'\\');

        // 璁颁綇鍙嶆枩鏉犲悗鐨勮捣濮嬩綅缃?
        std::size_t start = mark();

        auto oc = next();
        if (!oc.has_value()) {
            // 杈撳叆鍙湁鍙嶆枩鏉?
            return U'\\';
        }

        char32_t c = *oc;

        switch (c) {
            // 鍗曞瓧绗﹁浆涔?
            case U'a': return U'\x07';
            case U'b': return U'\x08';
            case U'f': return U'\x0c';
            case U'n': return U'\n';
            case U'r': return U'\r';
            case U't': return U'\t';
            case U'v': return U'\x0b';

            // 鍏繘鍒?1~3 浣?
            case U'0': case U'1': case U'2': case U'3':
            case U'4': case U'5': case U'6': case U'7': {
                // 杩欓噷鍜?Rust 涓€鏍凤紝渚濊禆鍏繘鍒舵暟瀛楁槸 ASCII 鍗曞瓧鑺?
                while (has([](char32_t ch) { return is_digit_base(ch, 8); }) &&
                       index_ - start < 3) {
                    next();
                }

                auto oct = input_.substr(start, index_ - start);
                unsigned long val = std::stoul(std::string(oct), nullptr, 8);
                return static_cast<char32_t>(val);
            }

            // \xhh, \uhhhh, \Uhhhhhhhh
            case U'x':
            case U'u':
            case U'U': {
                std::size_t m = mark();

                std::size_t max = 0;
                if (c == U'x') max = 2;
                else if (c == U'u') max = 4;
                else max = 8;

                // 杩欓噷鍜?Rust 涓€鏍凤紝渚濊禆鍗佸叚杩涘埗鏁板瓧鏄?ASCII 鍗曞瓧鑺?
                while (has([](char32_t ch) { return is_digit_base(ch, 16); }) &&
                       index_ - m < max) {
                    next();
                }

                // 涓€涓崄鍏繘鍒舵暟瀛楅兘娌℃湁鏃讹紝閫€鍖栨垚瀛楅潰瀛楃 x/u/U
                if (index_ == m) {
                    return c;
                }

                auto hex = input_.substr(m, index_ - m);
                unsigned long val = std::stoul(std::string(hex), nullptr, 16);

                if (is_valid_scalar(static_cast<uint32_t>(val))) {
                    return static_cast<char32_t>(val);
                } else {
                    // 闈炴硶 Unicode 鏍囬噺鍊兼椂锛屽洖閫€骞惰繑鍥?x/u/U
                    reset_to(m);
                    return c;
                }
            }

            // 鍏朵粬浠绘剰瀛楃锛氬幓鎺夊弽鏂滄潬锛屼繚鐣欏畠
            default:
                return c;
        }
    }

private:
    struct Decoded {
        char32_t ch;
        std::size_t len;
    };

    std::string_view input_;
    std::size_t index_;

    // 鍥為€€鍒版煇涓?mark
    void reset_to(std::size_t mark) {
        assert(mark <= input_.size());
        index_ = mark;
    }

    static bool is_cont(unsigned char b) {
        return (b & 0xC0u) == 0x80u;
    }

    static std::optional<Decoded> decode_utf8_at(std::string_view s, std::size_t pos) {
        if (pos >= s.size()) {
            return std::nullopt;
        }

        const unsigned char b0 = static_cast<unsigned char>(s[pos]);

        // 1-byte
        if (b0 < 0x80u) {
            return Decoded{static_cast<char32_t>(b0), 1};
        }

        // 2-byte
        if ((b0 >> 5) == 0x6) {
            if (pos + 1 >= s.size()) return std::nullopt;
            const unsigned char b1 = static_cast<unsigned char>(s[pos + 1]);
            if (!is_cont(b1)) return std::nullopt;

            char32_t cp =
                ((b0 & 0x1Fu) << 6) |
                (b1 & 0x3Fu);

            return Decoded{cp, 2};
        }

        // 3-byte
        if ((b0 >> 4) == 0xE) {
            if (pos + 2 >= s.size()) return std::nullopt;
            const unsigned char b1 = static_cast<unsigned char>(s[pos + 1]);
            const unsigned char b2 = static_cast<unsigned char>(s[pos + 2]);
            if (!is_cont(b1) || !is_cont(b2)) return std::nullopt;

            char32_t cp =
                ((b0 & 0x0Fu) << 12) |
                ((b1 & 0x3Fu) << 6) |
                (b2 & 0x3Fu);

            return Decoded{cp, 3};
        }

        // 4-byte
        if ((b0 >> 3) == 0x1E) {
            if (pos + 3 >= s.size()) return std::nullopt;
            const unsigned char b1 = static_cast<unsigned char>(s[pos + 1]);
            const unsigned char b2 = static_cast<unsigned char>(s[pos + 2]);
            const unsigned char b3 = static_cast<unsigned char>(s[pos + 3]);
            if (!is_cont(b1) || !is_cont(b2) || !is_cont(b3)) return std::nullopt;

            char32_t cp =
                ((b0 & 0x07u) << 18) |
                ((b1 & 0x3Fu) << 12) |
                ((b2 & 0x3Fu) << 6) |
                (b3 & 0x3Fu);

            return Decoded{cp, 4};
        }

        return std::nullopt;
    }

    static bool is_digit_base(char32_t ch, int base) {
        if (base == 8) {
            return ch >= U'0' && ch <= U'7';
        }
        if (base == 10) {
            return ch >= U'0' && ch <= U'9';
        }
        if (base == 16) {
            return (ch >= U'0' && ch <= U'9') ||
                   (ch >= U'a' && ch <= U'f') ||
                   (ch >= U'A' && ch <= U'F');
        }
        return false;
    }

    static bool is_valid_scalar(uint32_t cp) {
        // Unicode scalar value: <= 0x10FFFF 涓斾笉鏄?surrogate
        return cp <= 0x10FFFFu && !(cp >= 0xD800u && cp <= 0xDFFFu);
    }
};

// 浠呯敤浜庢紨绀鸿緭鍑猴紝鎶?char32_t 杞垚 UTF-8 string
[[maybe_unused]] static std::string to_utf8(char32_t ch) {
    std::string out;

    uint32_t cp = static_cast<uint32_t>(ch);
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }

    return out;
}




