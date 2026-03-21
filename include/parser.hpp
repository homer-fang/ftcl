#pragma once

#include "eval_ptr.hpp"
#include "type.hpp"
#include "util.hpp"

#include <memory>
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
            tokens.push(*nested);
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
            tokens.push(*nested);
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

inline ftcl::expected<Script, Exception> parse_script(std::string_view input) {
    EvalPtr ctx(input);
    return parse_script(ctx);
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

