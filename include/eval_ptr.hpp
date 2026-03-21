#pragma once

#include "tokenizer.hpp"

#include <cstddef>
#include <cwctype>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace ftcl {

// Parsing context: wraps Tokenizer plus parser flags.
class EvalPtr {
public:
    explicit EvalPtr(std::string_view input)
        : tok_(input),
          bracket_term_(false),
          term_char_(std::nullopt),
          no_eval_(false) {}

    explicit EvalPtr(const Tokenizer& tok)
        : tok_(tok),
          bracket_term_(false),
          term_char_(std::nullopt),
          no_eval_(false) {}

    // Returns a mutable reference to the inner tokenizer.
    Tokenizer& tok() {
        return tok_;
    }

    // Temporary compatibility helper, matching the Rust version.
    static EvalPtr from_tokenizer(const Tokenizer& ptr) {
        return EvalPtr(ptr);
    }

    // Temporary compatibility helper, matching the Rust version.
    Tokenizer to_tokenizer() const {
        return tok_;
    }

    //-----------------------------------------------------------------------
    // Configuration

    // If true, script ends at ']'; otherwise it ends at input end.
    void set_bracket_term(bool flag) {
        bracket_term_ = flag;
        term_char_ = flag ? std::optional<char32_t>(U']') : std::nullopt;
    }


    // Returns whether the input ends with ']', i.e. interpolated script mode.
    bool is_bracket_term() const {
        return bracket_term_;
    }

    // Enables/disables "no eval" mode: only scans for structural completeness.
    void set_no_eval(bool flag) {
        no_eval_ = flag;
    }

    // Returns whether we are scanning for completeness only.
    bool is_no_eval() const {
        return no_eval_;
    }

    //-----------------------------------------------------------------------
    // Tokenizer methods

    // Gets the next character.
    std::optional<char32_t> next() {
        return tok_.next();
    }

    // Checks whether the next character is ch.
    bool next_is(char32_t ch) const {
        return tok_.is(ch);
    }

    // We are at input end when there are no more characters.
    bool at_end() const {
        return tok_.at_end();
    }

    // Skips one character.
    void skip() {
        tok_.skip();
    }

    // Skips a specific character.
    void skip_char(char32_t ch) {
        tok_.skip_char(ch);
    }

    // Skips while predicate is true.
    template <class Pred>
    void skip_while(Pred&& predicate) {
        tok_.skip_while(std::forward<Pred>(predicate));
    }

    // Returns the current index as a mark.
    std::size_t mark() const {
        return tok_.mark();
    }

    // Returns token between mark and current index.
    std::string_view token(std::size_t mark) const {
        return tok_.token(mark);
    }

    // Parses a backslash escape and returns the substituted char.
    char32_t backslash_subst() {
        return tok_.backslash_subst();
    }

    //-----------------------------------------------------------------------
    // Parsing helpers

    // End of script: hit term char or input end.
    bool at_end_of_script() const {
        return tok_.at_end() || tok_.peek() == term_char_;
    }

    // End of command: newline, semicolon, or end of script.
    bool at_end_of_command() const {
        return next_is(U'\n') || next_is(U';') || at_end_of_script();
    }

    // Is current char a whitespace char, including newlines?
    bool next_is_block_white() const {
        auto c = tok_.peek();
        return c.has_value() && is_space(*c);
    }

    // Is current char a whitespace char, excluding newlines?
    bool next_is_line_white() const {
        auto c = tok_.peek();
        return c.has_value() && is_space(*c) && *c != U'\n';
    }

    // Is current char a variable-name char?
    bool next_is_varname_char() const {
        auto c = tok_.peek();
        return c.has_value() && (is_alnum(*c) || *c == U'_');
    }

    // Skips block whitespace (including newlines).
    void skip_block_white() {
        while (!at_end() && next_is_block_white()) {
            tok_.next();
        }
    }

    // Skips line whitespace (excluding newlines).
    void skip_line_white() {
        while (!at_end() && next_is_line_white()) {
            tok_.next();
        }
    }

    // Skips a comment (starting with '#'), including terminating newline.
    // Returns true if a comment was skipped.
    bool skip_comment() {
        if (!next_is(U'#')) {
            return false;
        }

        while (!at_end()) {
            auto c = tok_.next();
            if (c == std::optional<char32_t>(U'\n')) {
                break;
            }
            if (c == std::optional<char32_t>(U'\\')) {
                // Skip next char as well; intent is to skip backslashed newlines.
                tok_.next();
            }
        }

        return true;
    }

private:
    static bool is_space(char32_t ch) {
        if (ch <= static_cast<char32_t>(std::numeric_limits<wint_t>::max())) {
            return std::iswspace(static_cast<wint_t>(ch)) != 0;
        }
        return false;
    }

    static bool is_alnum(char32_t ch) {
        if (ch <= static_cast<char32_t>(std::numeric_limits<wint_t>::max())) {
            return std::iswalnum(static_cast<wint_t>(ch)) != 0;
        }
        return false;
    }

    // Input iterator.
    Tokenizer tok_;

    // Whether we're looking for a bracket terminator.
    bool bracket_term_;

    // Script terminator: none or ']'.
    std::optional<char32_t> term_char_;

    // Whether we're checking completeness without evaluation.
    bool no_eval_;
};

}  // namespace ftcl

