#pragma once

#include "eval_ptr.hpp"
#include "lexer.hpp"
#include "type.hpp"
#include "util.hpp"

#include <cctype>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ftcl {

enum class WordType {
    Value,
    VarRef,
    ArrayRef,
    Script,
    NumericBracket,
    Tokens,
    Expand,
    String,
};

class Word {
public:
    Word()
        : type_(WordType::Value),
          value_(Value::empty()) {}

    static Word value(Value value) {
        return Word(WordType::Value, std::move(value));
    }

    static Word var_ref(std::string name) {
        return Word(WordType::VarRef, Value(std::move(name)));
    }

    static Word array_ref(std::string name, Word index) {
        Word out(WordType::ArrayRef, Value::empty());
        out.name_ = std::move(name);
        out.child_ = std::make_shared<Word>(std::move(index));
        return out;
    }

    static Word script(std::string script_text) {
        return Word(WordType::Script, Value(std::move(script_text)));
    }

    static Word numeric_bracket(Word inner, std::string script_text) {
        Word out(WordType::NumericBracket, Value(std::move(script_text)));
        out.child_ = std::make_shared<Word>(std::move(inner));
        return out;
    }

    static Word tokens(std::vector<Word> words) {
        Word out(WordType::Tokens, Value::empty());
        out.words_ = std::move(words);
        return out;
    }

    static Word expand(Word word) {
        Word out(WordType::Expand, Value::empty());
        out.child_ = std::make_shared<Word>(std::move(word));
        return out;
    }

    static Word string(std::string text) {
        return Word(WordType::String, Value(std::move(text)));
    }

    Word(WordType type, Value value)
        : type_(type),
          value_(std::move(value)) {}

    WordType type() const {
        return type_;
    }

    const Value& value() const {
        return value_;
    }

    const std::string& name() const {
        return name_;
    }

    const std::vector<Word>& words() const {
        return words_;
    }

    const Word& child() const {
        return *child_;
    }

    bool has_child() const {
        return static_cast<bool>(child_);
    }

private:
    WordType type_;
    Value value_;
    std::string name_;
    std::vector<Word> words_;
    std::shared_ptr<Word> child_;
};

using WordVec = std::vector<Word>;

class Script {
public:
    Script() = default;

    explicit Script(std::vector<WordVec> commands)
        : commands_(std::move(commands)) {}

    const std::vector<WordVec>& commands() const {
        return commands_;
    }

    std::vector<WordVec>& commands_mut() {
        return commands_;
    }

    bool empty() const {
        return commands_.empty();
    }

private:
    std::vector<WordVec> commands_;
};

class Tokens {
public:
    void push(Word word) {
        flush_string();
        words_.push_back(std::move(word));
    }

    void push_str(std::string_view text) {
        string_.append(text.data(), text.size());
        has_string_ = true;
    }

    void push_char(char32_t ch) {
        string_ += char32_to_utf8(ch);
        has_string_ = true;
    }

    Word take() {
        if (has_string_) {
            if (words_.empty()) {
                return Word::value(Value(string_));
            }
            words_.push_back(Word::string(std::move(string_)));
            string_.clear();
            has_string_ = false;
        }

        if (words_.empty()) {
            return Word::value(Value::empty());
        }

        if (words_.size() == 1) {
            return words_.front();
        }

        return Word::tokens(std::move(words_));
    }

private:
    static std::string char32_to_utf8(char32_t ch) {
        std::string out;
        const std::uint32_t cp = static_cast<std::uint32_t>(ch);

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

    void flush_string() {
        if (!has_string_) {
            return;
        }

        words_.push_back(Word::string(std::move(string_)));
        string_.clear();
        has_string_ = false;
    }

    std::vector<Word> words_;
    bool has_string_ = false;
    std::string string_;
};

inline ftcl::expected<Script, Exception> parser_error(const std::string& message) {
    return ftcl::unexpected(Exception::ftcl_err(Value(message)));
}

inline ftcl::expected<Word, Exception> word_error(const std::string& message) {
    return ftcl::unexpected(Exception::ftcl_err(Value(message)));
}

inline ftcl::expected<Word, Exception> parse_next_word(EvalPtr& ctx);
inline ftcl::expected<Word, Exception> parse_bare_word(EvalPtr& ctx, bool index_flag);
inline ftcl::expected<Word, Exception> parse_brackets(EvalPtr& ctx);
inline ftcl::expected<Word, Exception> parse_varname(EvalPtr& ctx);
inline ftcl::expected<bool, Exception> parse_dollar(EvalPtr& ctx, Tokens& tokens);
inline ftcl::expected<Script, Exception> parse_script(EvalPtr& ctx);
inline ftcl::expected<Script, Exception> parse_script(std::string_view input);

enum class ParserBackend {
    Legacy,
    TokenStreamAdapter,
    ShadowCompare,
};

#ifndef FTCL_PARSER_DEFAULT_BACKEND
#define FTCL_PARSER_DEFAULT_BACKEND 0
#endif

class ParserTokenStreamAdapter {
public:
    explicit ParserTokenStreamAdapter(std::string_view input);

    std::string_view source() const;
    const std::vector<LexToken>& tokens() const;

private:
    std::string_view input_;
    std::vector<LexToken> tokens_;
};

class TokenCursor {
public:
    explicit TokenCursor(const std::vector<LexToken>& tokens)
        : tokens_(tokens) {}

    const LexToken* peek(std::size_t lookahead = 0) const {
        const std::size_t idx = index_ + lookahead;
        if (idx >= tokens_.size()) {
            return nullptr;
        }
        return &tokens_[idx];
    }

    const LexToken* next() {
        const LexToken* tok = peek();
        if (tok != nullptr && tok->type != LexTokenType::EndOfInput) {
            ++index_;
        }
        return tok;
    }

    bool match(LexTokenType type) {
        const LexToken* tok = peek();
        if (tok == nullptr || tok->type != type) {
            return false;
        }
        next();
        return true;
    }

    void skip_ws() {
        while (true) {
            const LexToken* tok = peek();
            if (tok == nullptr) {
                break;
            }
            if (tok->type != LexTokenType::WhiteSpace && tok->type != LexTokenType::Newline) {
                break;
            }
            next();
        }
    }

    void skip_line_ws() {
        while (match(LexTokenType::WhiteSpace)) {
        }
    }

    void skip_comment() {
        while (match(LexTokenType::Comment)) {
            match(LexTokenType::Newline);
            skip_ws();
        }
    }

    bool at_cmd_end() const {
        const LexToken* tok = peek();
        if (tok == nullptr) {
            return true;
        }
        return tok->type == LexTokenType::Semicolon || tok->type == LexTokenType::Newline ||
               tok->type == LexTokenType::EndOfInput;
    }

    bool at_end() const {
        const LexToken* tok = peek();
        return tok == nullptr || tok->type == LexTokenType::EndOfInput;
    }

    std::size_t position() const {
        return index_;
    }

    void set_position(std::size_t pos) {
        index_ = pos < tokens_.size() ? pos : tokens_.size();
    }

private:
    const std::vector<LexToken>& tokens_;
    std::size_t index_ = 0;
};

inline bool parser_words_equal(const Word& lhs, const Word& rhs);
inline bool parser_commands_equal(const WordVec& lhs, const WordVec& rhs);
inline bool parser_scripts_equal(const Script& lhs, const Script& rhs);
struct ParsedVarRefToken {
    Word word = Word::value(Value::empty());
    std::string trailing_literal;
};
struct TokenBackslashSubstResult {
    char32_t subst = U'\\';
    std::size_t consumed_end = 0;
};
struct TokenBracedEscapeResult {
    std::optional<char32_t> escaped;
    std::size_t consumed_end = 0;
};
inline std::size_t token_cursor_offset(const ParserTokenStreamAdapter& adapter, const TokenCursor& cursor);
inline std::string_view token_source_slice(const ParserTokenStreamAdapter& adapter,
                                           std::size_t start_offset,
                                           std::size_t end_offset);
inline void token_advance_to_offset(TokenCursor& cursor, std::size_t target_offset);
inline void token_flush_literal(const ParserTokenStreamAdapter& adapter,
                                const TokenCursor& cursor,
                                std::size_t literal_start,
                                Tokens& out);
inline std::string token_trailing_after_offset(const ParserTokenStreamAdapter& adapter,
                                               const TokenCursor& cursor,
                                               std::size_t consumed_end);
inline TokenBackslashSubstResult token_backslash_subst_at(std::string_view source, std::size_t slash_offset);
inline TokenBracedEscapeResult token_braced_escape_at(std::string_view source, std::size_t slash_offset);
inline std::size_t token_containing_end_offset(const ParserTokenStreamAdapter& adapter, std::size_t offset);
inline bool source_is_varname_char(std::string_view source, std::size_t offset);
inline ftcl::expected<Word, Exception> parse_braced_word_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                      TokenCursor& cursor);
inline ftcl::expected<Word, Exception> parse_quoted_word_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                      TokenCursor& cursor);
inline ftcl::expected<Word, Exception> parse_bare_word_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                    TokenCursor& cursor,
                                                                    bool index_flag);
inline ftcl::expected<Word, Exception> parse_brackets_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                   TokenCursor& cursor);
inline ftcl::expected<ParsedVarRefToken, Exception> parse_varname_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                                TokenCursor& cursor);
inline ftcl::expected<bool, Exception> parse_dollar_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                 TokenCursor& cursor,
                                                                 Tokens& tokens);
inline ftcl::expected<Word, Exception> parse_next_word_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                    TokenCursor& cursor);
inline ftcl::expected<WordVec, Exception> parse_command_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                     TokenCursor& cursor);
inline ftcl::expected<Script, Exception> parse_script_legacy_input(std::string_view input);
inline ftcl::expected<Script, Exception> parse_script_token_stream(std::string_view input);
inline ftcl::expected<Script, Exception> parse_script_with_backend(std::string_view input, ParserBackend backend);
inline ParserBackend parse_backend_from_string(std::string_view name);
inline ParserBackend parser_compile_time_default_backend();
inline ParserBackend parser_default_backend();

inline ftcl::expected<WordVec, Exception> parse_command(EvalPtr& ctx) {
    WordVec cmd;

    // Skip command separators/comments before the next real command.
    while (!ctx.at_end_of_script()) {
        ctx.skip_block_white();
        if (!ctx.skip_comment()) {
            break;
        }
    }

    while (!ctx.at_end_of_command()) {
        auto word = parse_next_word(ctx);
        if (!word.has_value()) {
            return ftcl::unexpected(word.error());
        }
        cmd.push_back(*word);
        ctx.skip_line_white();
    }

    if (ctx.next_is(U';')) {
        ctx.next();
    }

    return cmd;
}

inline ftcl::expected<Word, Exception> parse_braced_word(EvalPtr& ctx) {
    ctx.skip_char(U'{');
    int depth = 1;
    std::string text;
    std::size_t start = ctx.mark();

    auto push_view = [&](std::string_view sv) {
        text.append(sv.data(), sv.size());
    };

    while (!ctx.at_end()) {
        if (ctx.next_is(U'{')) {
            ++depth;
            ctx.skip();
            continue;
        }

        if (ctx.next_is(U'}')) {
            --depth;
            if (depth > 0) {
                ctx.skip();
                continue;
            }

            push_view(ctx.token(start));
            ctx.skip_char(U'}');

            if (ctx.at_end_of_command() || ctx.next_is_line_white()) {
                return Word::value(Value(std::move(text)));
            }
            return word_error("extra characters after close-brace");
        }

        if (ctx.next_is(U'\\')) {
            push_view(ctx.token(start));
            ctx.skip_char(U'\\');

            auto ch = ctx.next();
            if (!ch.has_value()) {
                break;
            }

            if (*ch == U'\n') {
                text.push_back(' ');
            } else {
                text.push_back('\\');
                Tokens tmp;
                tmp.push_char(*ch);
                text += tmp.take().value().as_string();
            }

            start = ctx.mark();
            continue;
        }

        ctx.skip();
    }

    return word_error("missing close-brace");
}

inline bool is_numeric_bracket_candidate_inner_word(const Word& word) {
    switch (word.type()) {
        case WordType::Value:
        case WordType::String:
        case WordType::VarRef:
        case WordType::ArrayRef:
            return true;
        default:
            return false;
    }
}

inline std::optional<Word> parse_numeric_bracket_candidate_word(std::string_view script_text) {
    EvalPtr inner(script_text);
    inner.skip_line_white();
    if (inner.at_end()) {
        return std::nullopt;
    }

    auto inner_word = parse_next_word(inner);
    if (!inner_word.has_value()) {
        return std::nullopt;
    }

    inner.skip_line_white();
    if (!inner.at_end()) {
        return std::nullopt;
    }

    if (!is_numeric_bracket_candidate_inner_word(*inner_word)) {
        return std::nullopt;
    }

    return Word::numeric_bracket(*inner_word, std::string(script_text));
}

inline ftcl::expected<Word, Exception> parse_quoted_word(EvalPtr& ctx) {
    ctx.skip_char(U'"');

    Tokens tokens;
    std::size_t start = ctx.mark();

    auto flush_literal = [&]() {
        if (start != ctx.mark()) {
            tokens.push_str(ctx.token(start));
        }
    };

    while (!ctx.at_end()) {
        if (ctx.next_is(U'[')) {
            flush_literal();
            auto nested = parse_brackets(ctx);
            if (!nested.has_value()) {
                return ftcl::unexpected(nested.error());
            }
            if (auto bracket_literal = parse_numeric_bracket_candidate_word(nested->value().as_string());
                bracket_literal.has_value()) {
                tokens.push(*bracket_literal);
            } else {
                tokens.push(*nested);
            }
            start = ctx.mark();
            continue;
        }

        if (ctx.next_is(U'$')) {
            flush_literal();
            auto dollar = parse_dollar(ctx, tokens);
            if (!dollar.has_value()) {
                return ftcl::unexpected(dollar.error());
            }
            start = ctx.mark();
            continue;
        }

        if (ctx.next_is(U'\\')) {
            flush_literal();
            tokens.push_char(ctx.backslash_subst());
            start = ctx.mark();
            continue;
        }

        if (ctx.next_is(U'"')) {
            flush_literal();
            ctx.skip_char(U'"');

            if (ctx.at_end_of_command() || ctx.next_is_line_white()) {
                return tokens.take();
            }

            return word_error("extra characters after close-quote");
        }

        ctx.skip();
    }

    return word_error("missing \"");
}

inline ftcl::expected<Word, Exception> parse_bare_word(EvalPtr& ctx, bool index_flag) {
    Tokens tokens;
    std::size_t start = ctx.mark();

    auto flush_literal = [&]() {
        if (start != ctx.mark()) {
            tokens.push_str(ctx.token(start));
        }
    };

    while (!ctx.at_end_of_command() && !ctx.next_is_line_white()) {
        if (index_flag && ctx.next_is(U')')) {
            break;
        }

        if (ctx.next_is(U'[')) {
            flush_literal();
            auto nested = parse_brackets(ctx);
            if (!nested.has_value()) {
                return ftcl::unexpected(nested.error());
            }
            if (auto bracket_literal = parse_numeric_bracket_candidate_word(nested->value().as_string());
                bracket_literal.has_value()) {
                tokens.push(*bracket_literal);
            } else {
                tokens.push(*nested);
            }
            start = ctx.mark();
            continue;
        }

        if (ctx.next_is(U'$')) {
            flush_literal();
            auto dollar = parse_dollar(ctx, tokens);
            if (!dollar.has_value()) {
                return ftcl::unexpected(dollar.error());
            }
            start = ctx.mark();
            continue;
        }

        if (ctx.next_is(U'\\')) {
            flush_literal();
            tokens.push_char(ctx.backslash_subst());
            start = ctx.mark();
            continue;
        }

        ctx.skip();
    }

    flush_literal();
    return tokens.take();
}

inline ftcl::expected<Word, Exception> parse_brackets(EvalPtr& ctx) {
    ctx.skip_char(U'[');
    const std::size_t script_start = ctx.mark();

    const bool old = ctx.is_bracket_term();
    ctx.set_bracket_term(true);
    auto nested = parse_script(ctx);
    ctx.set_bracket_term(old);
    if (!nested.has_value()) {
        return ftcl::unexpected(nested.error());
    }

    if (!ctx.next_is(U']')) {
        return word_error("missing close-bracket");
    }

    std::string script_text(ctx.token(script_start));
    ctx.skip_char(U']');
    return Word::script(std::move(script_text));
}

inline ftcl::expected<Word, Exception> parse_varname(EvalPtr& ctx) {
    if (ctx.next_is(U'{')) {
        ctx.skip_char(U'{');
        const std::size_t start = ctx.mark();
        ctx.skip_while([](char32_t ch) { return ch != U'}'; });

        if (ctx.at_end()) {
            return word_error("missing close-brace for variable name");
        }

        VarName var_name(std::string(ctx.token(start)));
        ctx.skip_char(U'}');

        if (var_name.index().has_value()) {
            return Word::array_ref(var_name.name(), Word::string(*var_name.index()));
        }

        return Word::var_ref(var_name.name());
    }

    const std::size_t start = ctx.mark();
    ctx.skip_while([](char32_t ch) { return is_varname_char(ch); });
    const std::string name(ctx.token(start));

    if (!ctx.next_is(U'(')) {
        return Word::var_ref(name);
    }

    ctx.skip_char(U'(');
    auto index_word = parse_bare_word(ctx, true);
    if (!index_word.has_value()) {
        return ftcl::unexpected(index_word.error());
    }

    if (!ctx.next_is(U')')) {
        return word_error("missing close-paren for array index");
    }

    ctx.skip_char(U')');
    return Word::array_ref(name, *index_word);
}

inline ftcl::expected<bool, Exception> parse_dollar(EvalPtr& ctx, Tokens& tokens) {
    ctx.skip_char(U'$');

    if (!ctx.next_is_varname_char() && !ctx.next_is(U'{')) {
        tokens.push_char(U'$');
        return true;
    }

    auto word = parse_varname(ctx);
    if (!word.has_value()) {
        return ftcl::unexpected(word.error());
    }
    tokens.push(*word);
    return true;
}

inline ftcl::expected<Word, Exception> parse_next_word(EvalPtr& ctx) {
    if (ctx.next_is(U'{')) {
        if (ctx.tok().as_str().starts_with("{*}")) {
            ctx.skip();
            ctx.skip();
            ctx.skip();

            if (ctx.at_end() || ctx.next_is_block_white()) {
                return Word::value(Value("*"));
            }

            auto rest = parse_next_word(ctx);
            if (!rest.has_value()) {
                return ftcl::unexpected(rest.error());
            }
            return Word::expand(*rest);
        }

        return parse_braced_word(ctx);
    }

    if (ctx.next_is(U'"')) {
        return parse_quoted_word(ctx);
    }

    return parse_bare_word(ctx, false);
}

inline ftcl::expected<Script, Exception> parse_script(EvalPtr& ctx) {
    std::vector<WordVec> commands;

    while (!ctx.at_end_of_script()) {
        auto command = parse_command(ctx);
        if (!command.has_value()) {
            return ftcl::unexpected(command.error());
        }

        if (!command->empty()) {
            commands.push_back(*command);
        }
    }

    return Script(std::move(commands));
}

inline ftcl::expected<Script, Exception> parse_script_legacy_input(std::string_view input) {
    EvalPtr ctx(input);
    return parse_script(ctx);
}

inline ftcl::expected<Script, Exception> parse_script(std::string_view input) {
    return parse_script_with_backend(input, parser_default_backend());
}

inline ParserTokenStreamAdapter::ParserTokenStreamAdapter(std::string_view input)
    : input_(input),
      tokens_(Lexer(input).tokenize_all()) {}

inline std::string_view ParserTokenStreamAdapter::source() const {
    return input_;
}

inline const std::vector<LexToken>& ParserTokenStreamAdapter::tokens() const {
    return tokens_;
}

inline bool parser_words_equal(const Word& lhs, const Word& rhs) {
    if (lhs.type() != rhs.type()) {
        return false;
    }

    switch (lhs.type()) {
        case WordType::Value:
        case WordType::VarRef:
        case WordType::Script:
        case WordType::String:
            return lhs.value().as_string() == rhs.value().as_string();

        case WordType::ArrayRef:
            if (lhs.name() != rhs.name() || lhs.has_child() != rhs.has_child()) {
                return false;
            }
            return !lhs.has_child() || parser_words_equal(lhs.child(), rhs.child());

        case WordType::NumericBracket:
            if (lhs.value().as_string() != rhs.value().as_string() || lhs.has_child() != rhs.has_child()) {
                return false;
            }
            return !lhs.has_child() || parser_words_equal(lhs.child(), rhs.child());

        case WordType::Tokens: {
            const auto& lw = lhs.words();
            const auto& rw = rhs.words();
            if (lw.size() != rw.size()) {
                return false;
            }
            for (std::size_t i = 0; i < lw.size(); ++i) {
                if (!parser_words_equal(lw[i], rw[i])) {
                    return false;
                }
            }
            return true;
        }

        case WordType::Expand:
            if (lhs.has_child() != rhs.has_child()) {
                return false;
            }
            return !lhs.has_child() || parser_words_equal(lhs.child(), rhs.child());
    }

    return false;
}

inline bool parser_commands_equal(const WordVec& lhs, const WordVec& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (!parser_words_equal(lhs[i], rhs[i])) {
            return false;
        }
    }

    return true;
}

inline bool parser_scripts_equal(const Script& lhs, const Script& rhs) {
    const auto& lc = lhs.commands();
    const auto& rc = rhs.commands();
    if (lc.size() != rc.size()) {
        return false;
    }

    for (std::size_t i = 0; i < lc.size(); ++i) {
        if (!parser_commands_equal(lc[i], rc[i])) {
            return false;
        }
    }

    return true;
}

inline std::size_t token_cursor_offset(const ParserTokenStreamAdapter& adapter, const TokenCursor& cursor) {
    const auto* tok = cursor.peek();
    if (tok == nullptr) {
        return adapter.source().size();
    }
    return tok->span.start_offset;
}

inline std::string_view token_source_slice(const ParserTokenStreamAdapter& adapter,
                                           std::size_t start_offset,
                                           std::size_t end_offset) {
    const std::size_t source_size = adapter.source().size();
    if (start_offset > end_offset || end_offset > source_size) {
        return std::string_view{};
    }
    return adapter.source().substr(start_offset, end_offset - start_offset);
}

inline void token_advance_to_offset(TokenCursor& cursor, std::size_t target_offset) {
    while (true) {
        const auto* tok = cursor.peek();
        if (tok == nullptr || tok->type == LexTokenType::EndOfInput || tok->span.start_offset >= target_offset) {
            return;
        }
        cursor.next();
    }
}

inline void token_flush_literal(const ParserTokenStreamAdapter& adapter,
                                const TokenCursor& cursor,
                                std::size_t literal_start,
                                Tokens& out) {
    const std::size_t current = token_cursor_offset(adapter, cursor);
    if (literal_start >= current) {
        return;
    }
    out.push_str(token_source_slice(adapter, literal_start, current));
}

inline std::string token_trailing_after_offset(const ParserTokenStreamAdapter& adapter,
                                               const TokenCursor& cursor,
                                               std::size_t consumed_end) {
    const auto* next_tok = cursor.peek(1);
    if (next_tok == nullptr || next_tok->type == LexTokenType::EndOfInput) {
        return "";
    }
    if (next_tok->type != LexTokenType::WordText) {
        return "";
    }

    if (consumed_end <= next_tok->span.start_offset || consumed_end >= next_tok->span.end_offset) {
        return "";
    }

    return std::string(token_source_slice(adapter, consumed_end, next_tok->span.end_offset));
}

inline TokenBackslashSubstResult token_backslash_subst_at(std::string_view source, std::size_t slash_offset) {
    TokenBackslashSubstResult out;
    out.consumed_end = slash_offset < source.size() ? slash_offset + 1 : source.size();

    if (slash_offset >= source.size() || source[slash_offset] != '\\') {
        return out;
    }

    Tokenizer tok(source.substr(slash_offset));
    out.subst = tok.backslash_subst();
    out.consumed_end = slash_offset + tok.mark();
    return out;
}

inline TokenBracedEscapeResult token_braced_escape_at(std::string_view source, std::size_t slash_offset) {
    TokenBracedEscapeResult out;
    out.consumed_end = slash_offset < source.size() ? slash_offset + 1 : source.size();

    if (slash_offset >= source.size() || source[slash_offset] != '\\') {
        return out;
    }

    std::size_t i = slash_offset + 1;
    if (i >= source.size()) {
        return out;
    }

    char32_t ch = 0;
    if (!utf8_decode_one(source, i, ch)) {
        return out;
    }

    out.escaped = ch;
    out.consumed_end = i;
    return out;
}

inline std::size_t token_containing_end_offset(const ParserTokenStreamAdapter& adapter, std::size_t offset) {
    for (const auto& tok : adapter.tokens()) {
        if (tok.type == LexTokenType::EndOfInput) {
            break;
        }
        if (tok.span.start_offset < offset && offset < tok.span.end_offset) {
            return tok.span.end_offset;
        }
    }
    return offset;
}

inline bool source_is_varname_char(std::string_view source, std::size_t offset) {
    if (offset >= source.size()) {
        return false;
    }
    const unsigned char ch = static_cast<unsigned char>(source[offset]);
    return (ch >= static_cast<unsigned char>('a') && ch <= static_cast<unsigned char>('z')) ||
           (ch >= static_cast<unsigned char>('A') && ch <= static_cast<unsigned char>('Z')) ||
           (ch >= static_cast<unsigned char>('0') && ch <= static_cast<unsigned char>('9')) || ch == '_';
}

inline ftcl::expected<Word, Exception> parse_braced_word_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                      TokenCursor& cursor) {
    if (!cursor.match(LexTokenType::LBrace)) {
        return word_error("missing open-brace");
    }

    int depth = 1;
    std::string text;
    std::size_t start = token_cursor_offset(adapter, cursor);

    auto push_view = [&](std::size_t end_offset) {
        if (start >= end_offset) {
            return;
        }
        text.append(token_source_slice(adapter, start, end_offset));
    };

    while (!cursor.at_end()) {
        const auto* tok = cursor.peek();
        if (tok == nullptr || tok->type == LexTokenType::EndOfInput) {
            break;
        }

        if (tok->type == LexTokenType::LBrace) {
            ++depth;
            cursor.next();
            continue;
        }

        if (tok->type == LexTokenType::RBrace) {
            --depth;
            if (depth > 0) {
                cursor.next();
                continue;
            }

            push_view(tok->span.start_offset);
            cursor.next();

            const auto* after = cursor.peek();
            if (cursor.at_cmd_end() || (after != nullptr && after->type == LexTokenType::WhiteSpace)) {
                return Word::value(Value(std::move(text)));
            }
            return word_error("extra characters after close-brace");
        }

        if (tok->type == LexTokenType::Backslash) {
            push_view(tok->span.start_offset);

            const std::size_t slash_offset = tok->span.start_offset;
            const auto escape = token_braced_escape_at(adapter.source(), slash_offset);
            const std::size_t consumed_end = escape.consumed_end;
            const std::string trailing = token_trailing_after_offset(adapter, cursor, consumed_end);
            token_advance_to_offset(cursor, consumed_end);

            if (!escape.escaped.has_value()) {
                break;
            }

            if (*escape.escaped == U'\n') {
                text.push_back(' ');
            } else {
                text.push_back('\\');
                utf8_append(text, *escape.escaped);
            }
            text += trailing;

            start = token_cursor_offset(adapter, cursor);
            continue;
        }

        cursor.next();
    }

    return word_error("missing close-brace");
}

inline ftcl::expected<Word, Exception> parse_brackets_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                   TokenCursor& cursor) {
    if (!cursor.match(LexTokenType::LBracket)) {
        return word_error("missing open-bracket");
    }

    const std::size_t script_start = token_cursor_offset(adapter, cursor);
    std::size_t script_end = script_start;
    int depth = 1;
    int brace_depth = 0;
    bool quoted = false;
    bool escaped = false;

    while (!cursor.at_end()) {
        const auto* tok = cursor.peek();
        if (tok == nullptr || tok->type == LexTokenType::EndOfInput) {
            break;
        }

        if (escaped) {
            escaped = false;
            cursor.next();
            continue;
        }

        if (tok->type == LexTokenType::Backslash) {
            escaped = true;
            cursor.next();
            continue;
        }

        if (quoted) {
            if (tok->type == LexTokenType::Quote) {
                quoted = false;
            }
            cursor.next();
            continue;
        }

        if (tok->type == LexTokenType::Quote && brace_depth == 0) {
            quoted = true;
            cursor.next();
            continue;
        }

        if (brace_depth > 0) {
            if (tok->type == LexTokenType::LBrace) {
                ++brace_depth;
            } else if (tok->type == LexTokenType::RBrace) {
                --brace_depth;
            }
            cursor.next();
            continue;
        }

        if (tok->type == LexTokenType::LBrace) {
            ++brace_depth;
            cursor.next();
            continue;
        }

        if (tok->type == LexTokenType::LBracket) {
            ++depth;
            cursor.next();
            continue;
        }

        if (tok->type == LexTokenType::RBracket) {
            --depth;
            if (depth == 0) {
                script_end = tok->span.start_offset;
                cursor.next();
                break;
            }
            cursor.next();
            continue;
        }

        cursor.next();
    }

    if (depth != 0) {
        return word_error("missing close-bracket");
    }

    std::string script_text(token_source_slice(adapter, script_start, script_end));
    auto nested = parse_script_token_stream(script_text);
    if (!nested.has_value()) {
        return ftcl::unexpected(nested.error());
    }
    return Word::script(std::move(script_text));
}

inline ftcl::expected<ParsedVarRefToken, Exception> parse_varname_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                                TokenCursor& cursor) {
    ParsedVarRefToken out;
    const std::string_view source = adapter.source();
    const std::size_t start_offset = token_cursor_offset(adapter, cursor);

    if (start_offset < source.size() && source[start_offset] == '{') {
        std::size_t close = start_offset + 1;
        while (close < source.size() && source[close] != '}') {
            ++close;
        }
        if (close >= source.size()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("missing close-brace for variable name")));
        }

        VarName var_name(std::string(token_source_slice(adapter, start_offset + 1, close)));
        const std::size_t consumed_end = close + 1;
        const std::size_t trailing_end = token_containing_end_offset(adapter, consumed_end);
        if (trailing_end > consumed_end) {
            out.trailing_literal = std::string(token_source_slice(adapter, consumed_end, trailing_end));
        }
        token_advance_to_offset(cursor, consumed_end);

        if (var_name.index().has_value()) {
            out.word = Word::array_ref(var_name.name(), Word::string(*var_name.index()));
        } else {
            out.word = Word::var_ref(var_name.name());
        }
        return out;
    }

    const auto* first = cursor.peek();
    if (first == nullptr || first->type == LexTokenType::EndOfInput) {
        return ftcl::unexpected(Exception::ftcl_err(Value("missing variable name")));
    }

    const std::size_t name_start = first->span.start_offset;
    std::size_t name_end = name_start;
    while (name_end < source.size() && source_is_varname_char(source, name_end)) {
        ++name_end;
    }

    if (name_end == name_start) {
        return ftcl::unexpected(Exception::ftcl_err(Value("missing variable name")));
    }

    std::string name(token_source_slice(adapter, name_start, name_end));
    if (name_end < source.size() && source[name_end] == '(') {
        ParserTokenStreamAdapter index_adapter(source.substr(name_end + 1));
        TokenCursor index_cursor(index_adapter.tokens());
        auto index_word = parse_bare_word_token_stream(index_adapter, index_cursor, true);
        if (!index_word.has_value()) {
            return ftcl::unexpected(index_word.error());
        }

        if (!index_cursor.match(LexTokenType::RParen)) {
            return ftcl::unexpected(Exception::ftcl_err(Value("missing close-paren for array index")));
        }

        out.word = Word::array_ref(name, *index_word);
        const std::size_t consumed_end = name_end + 1 + token_cursor_offset(index_adapter, index_cursor);
        const std::size_t trailing_end = token_containing_end_offset(adapter, consumed_end);
        if (trailing_end > consumed_end) {
            out.trailing_literal = std::string(token_source_slice(adapter, consumed_end, trailing_end));
        }
        token_advance_to_offset(cursor, consumed_end);
        return out;
    }

    if (name_end < first->span.end_offset) {
        out.word = Word::var_ref(name);
        out.trailing_literal = std::string(token_source_slice(adapter, name_end, first->span.end_offset));
        cursor.next();
        return out;
    }

    token_advance_to_offset(cursor, name_end);
    out.word = Word::var_ref(name);
    return out;
}

inline ftcl::expected<bool, Exception> parse_dollar_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                 TokenCursor& cursor,
                                                                 Tokens& tokens) {
    if (!cursor.match(LexTokenType::Dollar)) {
        return ftcl::unexpected(Exception::ftcl_err(Value("missing dollar")));
    }

    const auto* next_tok = cursor.peek();
    if (next_tok == nullptr || next_tok->type == LexTokenType::EndOfInput) {
        tokens.push_char(U'$');
        return true;
    }

    const std::string_view source = adapter.source();
    const bool starts_braced_var =
        next_tok->span.start_offset < source.size() && source[next_tok->span.start_offset] == '{';
    const bool starts_varname = starts_braced_var || source_is_varname_char(source, next_tok->span.start_offset);
    if (!starts_varname) {
        tokens.push_char(U'$');
        return true;
    }

    auto parsed = parse_varname_token_stream(adapter, cursor);
    if (!parsed.has_value()) {
        return ftcl::unexpected(parsed.error());
    }

    tokens.push(parsed->word);
    if (!parsed->trailing_literal.empty()) {
        tokens.push_str(parsed->trailing_literal);
    }
    return true;
}

inline ftcl::expected<Word, Exception> parse_quoted_word_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                      TokenCursor& cursor) {
    if (!cursor.match(LexTokenType::Quote)) {
        return word_error("missing open-quote");
    }

    Tokens tokens;
    std::size_t start = token_cursor_offset(adapter, cursor);

    while (!cursor.at_end()) {
        const auto* tok = cursor.peek();
        if (tok == nullptr) {
            break;
        }

        if (tok->type == LexTokenType::LBracket) {
            token_flush_literal(adapter, cursor, start, tokens);
            auto nested = parse_brackets_token_stream(adapter, cursor);
            if (!nested.has_value()) {
                return ftcl::unexpected(nested.error());
            }
            if (auto bracket_literal = parse_numeric_bracket_candidate_word(nested->value().as_string());
                bracket_literal.has_value()) {
                tokens.push(*bracket_literal);
            } else {
                tokens.push(*nested);
            }
            start = token_cursor_offset(adapter, cursor);
            continue;
        }

        if (tok->type == LexTokenType::Dollar) {
            token_flush_literal(adapter, cursor, start, tokens);
            auto dollar = parse_dollar_token_stream(adapter, cursor, tokens);
            if (!dollar.has_value()) {
                return ftcl::unexpected(dollar.error());
            }
            start = token_cursor_offset(adapter, cursor);
            continue;
        }

        if (tok->type == LexTokenType::Backslash) {
            token_flush_literal(adapter, cursor, start, tokens);

            const std::size_t slash_offset = tok->span.start_offset;
            const auto subst = token_backslash_subst_at(adapter.source(), slash_offset);
            const std::string trailing = token_trailing_after_offset(adapter, cursor, subst.consumed_end);
            tokens.push_char(subst.subst);
            token_advance_to_offset(cursor, subst.consumed_end);
            if (!trailing.empty()) {
                tokens.push_str(trailing);
            }

            start = token_cursor_offset(adapter, cursor);
            continue;
        }

        if (tok->type == LexTokenType::Quote) {
            token_flush_literal(adapter, cursor, start, tokens);
            cursor.next();

            const auto* after = cursor.peek();
            if (cursor.at_cmd_end() || (after != nullptr && after->type == LexTokenType::WhiteSpace)) {
                return tokens.take();
            }
            return word_error("extra characters after close-quote");
        }

        cursor.next();
    }

    return word_error("missing \"");
}

inline ftcl::expected<Word, Exception> parse_bare_word_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                    TokenCursor& cursor,
                                                                    bool index_flag) {
    Tokens tokens;
    std::size_t start = token_cursor_offset(adapter, cursor);

    while (!cursor.at_end() && !cursor.at_cmd_end()) {
        const auto* tok = cursor.peek();
        if (tok == nullptr || tok->type == LexTokenType::EndOfInput || tok->type == LexTokenType::WhiteSpace) {
            break;
        }

        if (index_flag && tok->type == LexTokenType::RParen) {
            break;
        }

        if (tok->type == LexTokenType::LBracket) {
            token_flush_literal(adapter, cursor, start, tokens);
            auto nested = parse_brackets_token_stream(adapter, cursor);
            if (!nested.has_value()) {
                return ftcl::unexpected(nested.error());
            }
            if (auto bracket_literal = parse_numeric_bracket_candidate_word(nested->value().as_string());
                bracket_literal.has_value()) {
                tokens.push(*bracket_literal);
            } else {
                tokens.push(*nested);
            }
            start = token_cursor_offset(adapter, cursor);
            continue;
        }

        if (tok->type == LexTokenType::Dollar) {
            token_flush_literal(adapter, cursor, start, tokens);
            auto dollar = parse_dollar_token_stream(adapter, cursor, tokens);
            if (!dollar.has_value()) {
                return ftcl::unexpected(dollar.error());
            }
            start = token_cursor_offset(adapter, cursor);
            continue;
        }

        if (tok->type == LexTokenType::Backslash) {
            token_flush_literal(adapter, cursor, start, tokens);

            const std::size_t slash_offset = tok->span.start_offset;
            const auto subst = token_backslash_subst_at(adapter.source(), slash_offset);
            const std::string trailing = token_trailing_after_offset(adapter, cursor, subst.consumed_end);
            tokens.push_char(subst.subst);
            token_advance_to_offset(cursor, subst.consumed_end);
            if (!trailing.empty()) {
                tokens.push_str(trailing);
            }

            start = token_cursor_offset(adapter, cursor);
            continue;
        }

        cursor.next();
    }

    token_flush_literal(adapter, cursor, start, tokens);
    return tokens.take();
}

inline ftcl::expected<Word, Exception> parse_next_word_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                    TokenCursor& cursor) {
    if (cursor.at_end() || cursor.at_cmd_end()) {
        return word_error("unexpected end of command while parsing word");
    }

    const auto* tok = cursor.peek();
    if (tok == nullptr) {
        return word_error("missing token at word start");
    }

    if (tok->type == LexTokenType::LBrace) {
        const auto* t1 = cursor.peek(1);
        const auto* t2 = cursor.peek(2);
        if (t1 != nullptr && t2 != nullptr && t1->type == LexTokenType::WordText && t1->text == "*" &&
            t2->type == LexTokenType::RBrace) {
            cursor.next();
            cursor.next();
            cursor.next();

            const auto* after = cursor.peek();
            if (cursor.at_cmd_end() || (after != nullptr && after->type == LexTokenType::WhiteSpace)) {
                return Word::value(Value("*"));
            }

            auto rest = parse_next_word_token_stream(adapter, cursor);
            if (!rest.has_value()) {
                return ftcl::unexpected(rest.error());
            }
            return Word::expand(*rest);
        }

        return parse_braced_word_token_stream(adapter, cursor);
    }

    if (tok->type == LexTokenType::Quote) {
        return parse_quoted_word_token_stream(adapter, cursor);
    }

    return parse_bare_word_token_stream(adapter, cursor, false);
}

inline ftcl::expected<WordVec, Exception> parse_command_token_stream(const ParserTokenStreamAdapter& adapter,
                                                                     TokenCursor& cursor) {
    while (true) {
        const std::size_t before = cursor.position();
        cursor.skip_ws();
        cursor.skip_comment();
        if (cursor.position() == before) {
            break;
        }
    }

    if (cursor.at_end()) {
        return WordVec{};
    }

    WordVec cmd;
    while (!cursor.at_end() && !cursor.at_cmd_end()) {
        auto word = parse_next_word_token_stream(adapter, cursor);
        if (!word.has_value()) {
            return ftcl::unexpected(word.error());
        }
        cmd.push_back(*word);
        cursor.skip_line_ws();
    }

    cursor.match(LexTokenType::Semicolon);
    cursor.match(LexTokenType::Newline);
    return cmd;
}

inline ftcl::expected<Script, Exception> parse_script_token_stream(std::string_view input) {
    ParserTokenStreamAdapter adapter(input);
    if (adapter.tokens().empty() || adapter.tokens().back().type != LexTokenType::EndOfInput) {
        return parser_error("token stream adapter produced invalid token stream");
    }

    TokenCursor cursor(adapter.tokens());
    std::vector<WordVec> commands;

    while (!cursor.at_end()) {
        const std::size_t before = cursor.position();
        auto command = parse_command_token_stream(adapter, cursor);
        if (!command.has_value()) {
            return ftcl::unexpected(command.error());
        }
        if (!command->empty()) {
            commands.push_back(*command);
        }

        if (cursor.position() == before && !cursor.at_end()) {
            return parser_error("token-stream parser made no progress");
        }
    }

    return Script(std::move(commands));
}

inline ParserBackend parse_backend_from_string(std::string_view name) {
    std::string lower;
    lower.reserve(name.size());
    for (char ch : name) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    if (lower == "legacy") {
        return ParserBackend::Legacy;
    }
    if (lower == "token_stream" || lower == "tokenstream" || lower == "token-stream") {
        return ParserBackend::TokenStreamAdapter;
    }
    if (lower == "shadow" || lower == "shadow_compare" || lower == "shadow-compare") {
        return ParserBackend::ShadowCompare;
    }

    return parser_compile_time_default_backend();
}

inline ParserBackend parser_compile_time_default_backend() {
#if FTCL_PARSER_DEFAULT_BACKEND == 1
    return ParserBackend::TokenStreamAdapter;
#elif FTCL_PARSER_DEFAULT_BACKEND == 2
    return ParserBackend::ShadowCompare;
#else
    return ParserBackend::Legacy;
#endif
}

inline ParserBackend parser_default_backend() {
    const char* env = std::getenv("FTCL_PARSER_BACKEND");
    if (env != nullptr && *env != '\0') {
        return parse_backend_from_string(env);
    }
    return parser_compile_time_default_backend();
}

inline ftcl::expected<Script, Exception> parse_script_with_backend(std::string_view input, ParserBackend backend) {
    switch (backend) {
        case ParserBackend::Legacy:
            return parse_script_legacy_input(input);

        case ParserBackend::TokenStreamAdapter:
            return parse_script_token_stream(input);

        case ParserBackend::ShadowCompare: {
            auto legacy = parse_script_legacy_input(input);
            if (!legacy.has_value()) {
                return ftcl::unexpected(legacy.error());
            }

            auto token_stream = parse_script_token_stream(input);
            if (!token_stream.has_value()) {
                return ftcl::unexpected(token_stream.error());
            }

            if (!parser_scripts_equal(*legacy, *token_stream)) {
                return parser_error("parser backend mismatch between legacy and token-stream adapter");
            }

            return *legacy;
        }
    }

    return parse_script_legacy_input(input);
}

inline ftclResult cmd_parse(Interp* /*interp*/, ContextID /*context_id*/, const std::vector<Value>& argv) {
    if (argv.size() != 2) {
        return ftcl_err("wrong # args: should be \"parse script\"");
    }

    auto parsed = parse_script(argv[1].as_string());
    if (!parsed.has_value()) {
        return ftcl::unexpected(parsed.error());
    }

    return ftcl_ok(static_cast<ftclInt>(parsed->commands().size()));
}

}  // namespace ftcl
