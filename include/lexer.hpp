#pragma once

#include "tokenizer.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ftcl {

struct Span {
    std::size_t start_offset = 0;
    std::size_t end_offset = 0;
    std::size_t start_line = 1;
    std::size_t end_line = 1;
    std::size_t start_column = 1;
    std::size_t end_column = 1;
};

enum class LexState {
    Default,
    Quoted,
};

enum class LexTokenType {
    WordText,
    WhiteSpace,
    Newline,
    Semicolon,
    Comment,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    LParen,
    RParen,
    Dollar,
    Quote,
    Backslash,
    EndOfInput,
};

struct LexToken {
    LexTokenType type = LexTokenType::EndOfInput;
    std::string text;
    Span span;
};

class Lexer {
public:
    explicit Lexer(std::string_view input)
        : input_(input),
          tok_(input) {}

    LexState state() const {
        return state_;
    }

    LexToken next_token() {
        if (tok_.at_end()) {
            return make_token(
                LexTokenType::EndOfInput,
                tok_.mark(),
                line_,
                column_,
                tok_.mark(),
                line_,
                column_);
        }

        if (state_ == LexState::Default) {
            if (peek_is_line_white()) {
                return scan_line_white();
            }

            if (peek_is(U'\n')) {
                return scan_newline();
            }

            if (peek_is(U';')) {
                return scan_semicolon();
            }

            if (at_command_start_ && peek_is(U'#')) {
                return scan_comment();
            }

            if (peek_is(U'"')) {
                return scan_quote();
            }

            if (peek_is(U'{')) {
                return scan_single(LexTokenType::LBrace);
            }
            if (peek_is(U'}')) {
                return scan_single(LexTokenType::RBrace);
            }
            if (peek_is(U'[')) {
                return scan_single(LexTokenType::LBracket);
            }
            if (peek_is(U']')) {
                return scan_single(LexTokenType::RBracket);
            }
            if (peek_is(U'(')) {
                return scan_single(LexTokenType::LParen);
            }
            if (peek_is(U')')) {
                return scan_single(LexTokenType::RParen);
            }
            if (peek_is(U'$')) {
                return scan_single(LexTokenType::Dollar);
            }
            if (peek_is(U'\\')) {
                return scan_single(LexTokenType::Backslash);
            }

            return scan_word_text();
        }

        // Quoted state:
        // spaces/newlines/semicolons are plain text here.
        if (peek_is(U'"')) {
            return scan_quote();
        }
        if (peek_is(U'$')) {
            return scan_single(LexTokenType::Dollar);
        }
        if (peek_is(U'[')) {
            return scan_single(LexTokenType::LBracket);
        }
        if (peek_is(U']')) {
            return scan_single(LexTokenType::RBracket);
        }
        if (peek_is(U'\\')) {
            return scan_single(LexTokenType::Backslash);
        }

        return scan_word_text();
    }

    std::vector<LexToken> tokenize_all() {
        std::vector<LexToken> out;
        while (true) {
            LexToken tok = next_token();
            out.push_back(tok);
            if (tok.type == LexTokenType::EndOfInput) {
                break;
            }
        }
        return out;
    }

private:
    bool peek_is(char32_t ch) const {
        return tok_.is(ch);
    }

    bool peek_is_line_white() const {
        auto ch = tok_.peek();
        return ch.has_value() && (*ch == U' ' || *ch == U'\t' || *ch == U'\r' || *ch == U'\f' || *ch == U'\v');
    }

    bool is_default_break_char(char32_t ch) const {
        if (ch == U'\n' || ch == U';' || ch == U'"' || ch == U'{' || ch == U'}' || ch == U'[' || ch == U']' ||
            ch == U'(' || ch == U')' || ch == U'$' || ch == U'\\') {
            return true;
        }
        if (ch == U' ' || ch == U'\t' || ch == U'\r' || ch == U'\f' || ch == U'\v') {
            return true;
        }
        if (at_command_start_ && ch == U'#') {
            return true;
        }
        return false;
    }

    bool is_quoted_break_char(char32_t ch) const {
        return ch == U'"' || ch == U'$' || ch == U'[' || ch == U']' || ch == U'\\';
    }

    std::optional<char32_t> consume_one() {
        auto ch = tok_.next();
        if (!ch.has_value()) {
            return std::nullopt;
        }

        if (*ch == U'\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }

        return ch;
    }

    LexToken make_token(LexTokenType type,
                        std::size_t start_offset,
                        std::size_t start_line,
                        std::size_t start_column,
                        std::size_t end_offset,
                        std::size_t end_line,
                        std::size_t end_column) const {
        LexToken tok;
        tok.type = type;
        tok.text = std::string(input_.substr(start_offset, end_offset - start_offset));
        tok.span = Span{
            start_offset,
            end_offset,
            start_line,
            end_line,
            start_column,
            end_column,
        };
        return tok;
    }

    LexToken scan_line_white() {
        const std::size_t start_offset = tok_.mark();
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;

        while (peek_is_line_white()) {
            consume_one();
        }

        return make_token(
            LexTokenType::WhiteSpace,
            start_offset,
            start_line,
            start_column,
            tok_.mark(),
            line_,
            column_);
    }

    LexToken scan_newline() {
        const std::size_t start_offset = tok_.mark();
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;

        consume_one();
        at_command_start_ = true;

        return make_token(
            LexTokenType::Newline,
            start_offset,
            start_line,
            start_column,
            tok_.mark(),
            line_,
            column_);
    }

    LexToken scan_semicolon() {
        const std::size_t start_offset = tok_.mark();
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;

        consume_one();
        at_command_start_ = true;

        return make_token(
            LexTokenType::Semicolon,
            start_offset,
            start_line,
            start_column,
            tok_.mark(),
            line_,
            column_);
    }

    LexToken scan_comment() {
        const std::size_t start_offset = tok_.mark();
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;

        while (!tok_.at_end() && !peek_is(U'\n')) {
            consume_one();
        }

        // A comment sits where a command could start; keep this true.
        at_command_start_ = true;

        return make_token(
            LexTokenType::Comment,
            start_offset,
            start_line,
            start_column,
            tok_.mark(),
            line_,
            column_);
    }

    LexToken scan_quote() {
        const std::size_t start_offset = tok_.mark();
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;

        consume_one();

        if (state_ == LexState::Default) {
            state_ = LexState::Quoted;
        } else {
            state_ = LexState::Default;
        }

        at_command_start_ = false;

        return make_token(
            LexTokenType::Quote,
            start_offset,
            start_line,
            start_column,
            tok_.mark(),
            line_,
            column_);
    }

    LexToken scan_single(LexTokenType type) {
        const std::size_t start_offset = tok_.mark();
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;

        consume_one();
        at_command_start_ = false;

        return make_token(
            type,
            start_offset,
            start_line,
            start_column,
            tok_.mark(),
            line_,
            column_);
    }

    LexToken scan_word_text() {
        const std::size_t start_offset = tok_.mark();
        const std::size_t start_line = line_;
        const std::size_t start_column = column_;

        while (!tok_.at_end()) {
            auto ch = tok_.peek();
            if (!ch.has_value()) {
                break;
            }

            if (state_ == LexState::Default) {
                if (is_default_break_char(*ch)) {
                    break;
                }
            } else if (is_quoted_break_char(*ch)) {
                break;
            }

            consume_one();
        }

        at_command_start_ = false;

        return make_token(
            LexTokenType::WordText,
            start_offset,
            start_line,
            start_column,
            tok_.mark(),
            line_,
            column_);
    }

    std::string_view input_;
    Tokenizer tok_;
    LexState state_ = LexState::Default;
    bool at_command_start_ = true;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

}  // namespace ftcl
