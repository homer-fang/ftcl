#pragma once

#include "type.hpp"
#include "util.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace ftcl {

ftcl::expected<Value, Exception> expr_resolve_var(Interp* interp, const std::string& var_name);
ftcl::expected<Value, Exception> expr_eval_script(Interp* interp, const std::string& script);

enum class DatumType {
    Int,
    Float,
    String,
};

struct Datum {
    DatumType type = DatumType::String;
    ftclInt int_value = 0;
    ftclFloat float_value = 0.0;
    std::string string_value;

    static Datum from_int(ftclInt v) {
        Datum d;
        d.type = DatumType::Int;
        d.int_value = v;
        d.float_value = static_cast<ftclFloat>(v);
        return d;
    }

    static Datum from_float(ftclFloat v) {
        Datum d;
        d.type = DatumType::Float;
        d.int_value = static_cast<ftclInt>(v);
        d.float_value = v;
        return d;
    }

    static Datum from_string(std::string s) {
        Datum d;
        d.type = DatumType::String;
        d.string_value = std::move(s);
        return d;
    }

    bool is_true() const {
        switch (type) {
            case DatumType::Int:
                return int_value != 0;
            case DatumType::Float:
                return float_value != 0.0;
            case DatumType::String:
                return !string_value.empty() && string_value != "0" && string_value != "false";
        }
        return false;
    }

    ftclFloat as_float() const {
        if (type == DatumType::Float) {
            return float_value;
        }
        if (type == DatumType::Int) {
            return static_cast<ftclFloat>(int_value);
        }
        auto parsed = Value(string_value).as_float_opt();
        return parsed.value_or(0.0);
    }

    ftclInt as_int() const {
        if (type == DatumType::Int) {
            return int_value;
        }
        if (type == DatumType::Float) {
            return static_cast<ftclInt>(float_value);
        }
        auto parsed = Value(string_value).as_int_opt();
        return parsed.value_or(0);
    }

    std::string as_string() const {
        if (type == DatumType::String) {
            return string_value;
        }
        if (type == DatumType::Int) {
            return std::to_string(int_value);
        }
        return Value(as_float()).as_string();
    }
};

class ExprParser {
public:
    ExprParser(Interp* interp, std::string_view input)
        : interp_(interp),
          input_(input) {}

    ftcl::expected<Datum, Exception> parse() {
        auto value = parse_conditional();
        if (!value.has_value()) {
            return value;
        }

        skip_white();
        if (!at_end()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("syntax error in expression near \"" + remaining() + "\"")));
        }

        return value;
    }

private:
    struct EvalModeGuard {
        ExprParser* parser = nullptr;
        bool old = true;

        EvalModeGuard(ExprParser* p, bool eval)
            : parser(p),
              old(p->evaluate_) {
            p->evaluate_ = eval;
        }

        ~EvalModeGuard() {
            parser->evaluate_ = old;
        }
    };

    Interp* interp_;
    std::string_view input_;
    std::size_t pos_ = 0;
    bool evaluate_ = true;

    static Datum datum_from_value(const Value& value) {
        auto i = value.as_int_opt();
        if (i.has_value()) {
            return Datum::from_int(*i);
        }

        auto f = value.as_float_opt();
        if (f.has_value()) {
            return Datum::from_float(*f);
        }

        return Datum::from_string(value.as_string());
    }

    static int compare_for_relational(const Datum& lhs, const Datum& rhs) {
        if (lhs.type == DatumType::String || rhs.type == DatumType::String) {
            const std::string ls = lhs.as_string();
            const std::string rs = rhs.as_string();
            if (ls < rs) {
                return -1;
            }
            if (ls > rs) {
                return 1;
            }
            return 0;
        }

        if (lhs.type == DatumType::Float || rhs.type == DatumType::Float) {
            const ftclFloat lf = lhs.as_float();
            const ftclFloat rf = rhs.as_float();
            if (lf < rf) {
                return -1;
            }
            if (lf > rf) {
                return 1;
            }
            return 0;
        }

        const ftclInt li = lhs.as_int();
        const ftclInt ri = rhs.as_int();
        if (li < ri) {
            return -1;
        }
        if (li > ri) {
            return 1;
        }
        return 0;
    }

    static bool is_var_char(char ch) {
        return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
    }

    bool at_end() const {
        return pos_ >= input_.size();
    }

    char peek() const {
        return at_end() ? '\0' : input_[pos_];
    }

    bool match(char ch) {
        skip_white();
        if (!at_end() && input_[pos_] == ch) {
            ++pos_;
            return true;
        }
        return false;
    }

    bool match2(char a, char b) {
        skip_white();
        if (pos_ + 1 < input_.size() && input_[pos_] == a && input_[pos_ + 1] == b) {
            pos_ += 2;
            return true;
        }
        return false;
    }

    bool match_keyword(std::string_view keyword) {
        skip_white();

        if (pos_ + keyword.size() > input_.size()) {
            return false;
        }

        if (input_.substr(pos_, keyword.size()) != keyword) {
            return false;
        }

        const std::size_t end = pos_ + keyword.size();
        if (end < input_.size() && is_var_char(input_[end])) {
            return false;
        }

        pos_ = end;
        return true;
    }

    std::string remaining() const {
        return std::string(input_.substr(pos_));
    }

    void skip_white() {
        while (!at_end() && std::isspace(static_cast<unsigned char>(input_[pos_])) != 0) {
            ++pos_;
        }
    }

    ftcl::expected<std::string, Exception> parse_variable_name() {
        std::string name;

        if (!at_end() && peek() == '{') {
            ++pos_;
            while (!at_end() && peek() != '}') {
                name.push_back(peek());
                ++pos_;
            }
            if (at_end()) {
                return ftcl::unexpected(Exception::ftcl_err(Value("missing close brace for variable name")));
            }
            ++pos_;
        } else {
            while (!at_end()) {
                char ch = peek();
                if (!is_var_char(ch)) {
                    break;
                }
                name.push_back(ch);
                ++pos_;
            }

            if (!at_end() && peek() == '(') {
                name.push_back('(');
                ++pos_;
                int depth = 1;

                while (!at_end() && depth > 0) {
                    char ch = peek();
                    name.push_back(ch);
                    ++pos_;

                    if (ch == '(') {
                        ++depth;
                    } else if (ch == ')') {
                        --depth;
                    }
                }

                if (depth != 0) {
                    return ftcl::unexpected(Exception::ftcl_err(Value("missing close parenthesis in variable reference")));
                }
            }
        }

        if (name.empty()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("bad variable reference in expression")));
        }

        return name;
    }

    ftcl::expected<Datum, Exception> parse_variable_reference() {
        ++pos_;  // consume '$'

        auto name = parse_variable_name();
        if (!name.has_value()) {
            return ftcl::unexpected(name.error());
        }

        if (!evaluate_) {
            return Datum::from_int(0);
        }

        if (interp_ == nullptr) {
            return Datum::from_string("$" + *name);
        }

        auto resolved = expr_resolve_var(interp_, *name);
        if (!resolved.has_value()) {
            return ftcl::unexpected(resolved.error());
        }

        return datum_from_value(*resolved);
    }

    ftcl::expected<Datum, Exception> parse_command_substitution() {
        ++pos_;  // consume '['
        int depth = 1;
        std::string script;

        while (!at_end() && depth > 0) {
            const char ch = peek();
            ++pos_;

            if (ch == '[') {
                ++depth;
                script.push_back(ch);
                continue;
            }

            if (ch == ']') {
                --depth;
                if (depth > 0) {
                    script.push_back(ch);
                }
                continue;
            }

            script.push_back(ch);
        }

        if (depth != 0) {
            return ftcl::unexpected(Exception::ftcl_err(Value("missing close bracket in expression")));
        }

        if (!evaluate_) {
            return Datum::from_int(0);
        }

        if (interp_ == nullptr) {
            return ftcl::unexpected(Exception::ftcl_err(Value("command substitution requires an interpreter")));
        }

        auto value = expr_eval_script(interp_, script);
        if (!value.has_value()) {
            return ftcl::unexpected(value.error());
        }

        return datum_from_value(*value);
    }

    ftcl::expected<std::string, Exception> parse_quoted_string() {
        ++pos_;  // consume '"'
        std::string out;

        while (!at_end() && peek() != '"') {
            if (peek() == '\\' && pos_ + 1 < input_.size()) {
                out.push_back(input_[pos_ + 1]);
                pos_ += 2;
                continue;
            }

            if (peek() == '$') {
                auto value = parse_variable_reference();
                if (!value.has_value()) {
                    return ftcl::unexpected(value.error());
                }
                if (evaluate_) {
                    out += value->as_string();
                }
                continue;
            }

            if (peek() == '[') {
                auto value = parse_command_substitution();
                if (!value.has_value()) {
                    return ftcl::unexpected(value.error());
                }
                if (evaluate_) {
                    out += value->as_string();
                }
                continue;
            }

            out.push_back(input_[pos_++]);
        }

        if (at_end()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("unterminated quoted string in expression")));
        }

        ++pos_;  // consume '"'
        return out;
    }

    ftcl::expected<std::string, Exception> parse_braced_string() {
        ++pos_;  // consume '{'
        int depth = 1;
        std::string out;

        while (!at_end() && depth > 0) {
            const char ch = peek();
            ++pos_;

            if (ch == '{') {
                ++depth;
                out.push_back(ch);
                continue;
            }

            if (ch == '}') {
                --depth;
                if (depth > 0) {
                    out.push_back(ch);
                }
                continue;
            }

            out.push_back(ch);
        }

        if (depth != 0) {
            return ftcl::unexpected(Exception::ftcl_err(Value("missing close brace in expression")));
        }

        return out;
    }

    static ftcl::expected<ftclInt, Exception> int_for_bitwise(const Datum& value, const char* op) {
        if (value.type == DatumType::Float) {
            return ftcl::unexpected(
                Exception::ftcl_err(Value("can't use floating-point value as operand of \"" + std::string(op) + "\"")));
        }

        if (value.type == DatumType::Int) {
            return value.int_value;
        }

        auto parsed = Value(value.as_string()).as_int_opt();
        if (!parsed.has_value()) {
            return ftcl::unexpected(
                Exception::ftcl_err(Value("expected integer but got \"" + value.as_string() + "\"")));
        }
        return *parsed;
    }

    static ftcl::expected<ftclInt, Exception> checked_add(ftclInt a, ftclInt b) {
#if defined(__SIZEOF_INT128__)
        const __int128 sum = static_cast<__int128>(a) + static_cast<__int128>(b);
        if (sum > static_cast<__int128>(std::numeric_limits<ftclInt>::max()) ||
            sum < static_cast<__int128>(std::numeric_limits<ftclInt>::min())) {
            return ftcl::unexpected(Exception::ftcl_err(Value("integer overflow")));
        }
        return static_cast<ftclInt>(sum);
#else
        if ((b > 0 && a > std::numeric_limits<ftclInt>::max() - b) ||
            (b < 0 && a < std::numeric_limits<ftclInt>::min() - b)) {
            return ftcl::unexpected(Exception::ftcl_err(Value("integer overflow")));
        }
        return a + b;
#endif
    }

    static ftcl::expected<ftclInt, Exception> checked_sub(ftclInt a, ftclInt b) {
#if defined(__SIZEOF_INT128__)
        const __int128 diff = static_cast<__int128>(a) - static_cast<__int128>(b);
        if (diff > static_cast<__int128>(std::numeric_limits<ftclInt>::max()) ||
            diff < static_cast<__int128>(std::numeric_limits<ftclInt>::min())) {
            return ftcl::unexpected(Exception::ftcl_err(Value("integer overflow")));
        }
        return static_cast<ftclInt>(diff);
#else
        if ((b < 0 && a > std::numeric_limits<ftclInt>::max() + b) ||
            (b > 0 && a < std::numeric_limits<ftclInt>::min() + b)) {
            return ftcl::unexpected(Exception::ftcl_err(Value("integer overflow")));
        }
        return a - b;
#endif
    }

    static ftcl::expected<ftclInt, Exception> checked_mul(ftclInt a, ftclInt b) {
#if defined(__SIZEOF_INT128__)
        const __int128 prod = static_cast<__int128>(a) * static_cast<__int128>(b);
        if (prod > static_cast<__int128>(std::numeric_limits<ftclInt>::max()) ||
            prod < static_cast<__int128>(std::numeric_limits<ftclInt>::min())) {
            return ftcl::unexpected(Exception::ftcl_err(Value("integer overflow")));
        }
        return static_cast<ftclInt>(prod);
#else
        if (a == 0 || b == 0) {
            return static_cast<ftclInt>(0);
        }
        if (a == -1 && b == std::numeric_limits<ftclInt>::min()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("integer overflow")));
        }
        if (b == -1 && a == std::numeric_limits<ftclInt>::min()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("integer overflow")));
        }
        const ftclInt r = a * b;
        if (r / b != a) {
            return ftcl::unexpected(Exception::ftcl_err(Value("integer overflow")));
        }
        return r;
#endif
    }

    static ftcl::expected<ftclInt, Exception> checked_div(ftclInt a, ftclInt b) {
        if (b == 0) {
            return ftcl::unexpected(Exception::ftcl_err(Value("divide by zero")));
        }
        if (a == std::numeric_limits<ftclInt>::min() && b == -1) {
            return ftcl::unexpected(Exception::ftcl_err(Value("integer overflow")));
        }
        return a / b;
    }

    static ftcl::expected<ftclInt, Exception> checked_mod(ftclInt a, ftclInt b) {
        if (b == 0) {
            return ftcl::unexpected(Exception::ftcl_err(Value("divide by zero")));
        }
        if (a == std::numeric_limits<ftclInt>::min() && b == -1) {
            return static_cast<ftclInt>(0);
        }
        return a % b;
    }

    static ftcl::expected<ftclInt, Exception> checked_lshift(ftclInt value, ftclInt shift) {
        if (shift < 0) {
            return ftcl::unexpected(Exception::ftcl_err(Value("negative shift argument")));
        }

        using UInt = std::make_unsigned_t<ftclInt>;
        constexpr ftclInt bit_width = static_cast<ftclInt>(std::numeric_limits<UInt>::digits);
        if (shift >= bit_width) {
            if (value == 0) {
                return static_cast<ftclInt>(0);
            }
            return ftcl::unexpected(Exception::ftcl_err(Value("integer overflow")));
        }

#if defined(__SIZEOF_INT128__)
        const __int128 shifted = static_cast<__int128>(value) << static_cast<unsigned int>(shift);
        if (shifted > static_cast<__int128>(std::numeric_limits<ftclInt>::max()) ||
            shifted < static_cast<__int128>(std::numeric_limits<ftclInt>::min())) {
            return ftcl::unexpected(Exception::ftcl_err(Value("integer overflow")));
        }
        return static_cast<ftclInt>(shifted);
#else
        ftclInt out = value;
        for (ftclInt i = 0; i < shift; ++i) {
            auto doubled = checked_mul(out, 2);
            if (!doubled.has_value()) {
                return ftcl::unexpected(doubled.error());
            }
            out = *doubled;
        }
        return out;
#endif
    }

    static ftcl::expected<ftclInt, Exception> checked_rshift(ftclInt value, ftclInt shift) {
        if (shift < 0) {
            return ftcl::unexpected(Exception::ftcl_err(Value("negative shift argument")));
        }

        using UInt = std::make_unsigned_t<ftclInt>;
        constexpr ftclInt bit_width = static_cast<ftclInt>(std::numeric_limits<UInt>::digits);
        if (shift >= bit_width) {
            return value < 0 ? static_cast<ftclInt>(-1) : static_cast<ftclInt>(0);
        }

        UInt bits = static_cast<UInt>(value);
        bits >>= static_cast<unsigned int>(shift);
        if (value < 0 && shift > 0) {
            const UInt sign_fill = (~UInt{0}) << static_cast<unsigned int>(bit_width - shift);
            bits |= sign_fill;
        }

        return static_cast<ftclInt>(bits);
    }

    ftcl::expected<Datum, Exception> parse_conditional() {
        auto cond = parse_logical_or();
        if (!cond.has_value()) {
            return cond;
        }

        if (!match('?')) {
            return cond;
        }

        const bool take_true = evaluate_ && cond->is_true();
        Datum true_value = Datum::from_int(0);

        {
            EvalModeGuard guard(this, evaluate_ && take_true);
            auto rhs_true = parse_logical_or();
            if (!rhs_true.has_value()) {
                return rhs_true;
            }
            if (evaluate_ && take_true) {
                true_value = *rhs_true;
            }
        }

        if (!match(':')) {
            return ftcl::unexpected(Exception::ftcl_err(Value("syntax error in expression near \"" + remaining() + "\"")));
        }

        Datum false_value = Datum::from_int(0);
        {
            EvalModeGuard guard(this, evaluate_ && !take_true);
            auto rhs_false = parse_conditional();
            if (!rhs_false.has_value()) {
                return rhs_false;
            }
            if (evaluate_ && !take_true) {
                false_value = *rhs_false;
            }
        }

        if (!evaluate_) {
            return Datum::from_int(0);
        }
        return take_true ? true_value : false_value;
    }

    ftcl::expected<Datum, Exception> parse_logical_or() {
        auto lhs = parse_logical_and();
        if (!lhs.has_value()) {
            return lhs;
        }

        while (match2('|', '|')) {
            if (evaluate_ && lhs->is_true()) {
                EvalModeGuard guard(this, false);
                auto skipped = parse_logical_and();
                if (!skipped.has_value()) {
                    return skipped;
                }
                lhs = Datum::from_int(1);
                continue;
            }

            auto rhs = parse_logical_and();
            if (!rhs.has_value()) {
                return rhs;
            }

            if (!evaluate_) {
                lhs = Datum::from_int(0);
            } else {
                lhs = Datum::from_int(lhs->is_true() || rhs->is_true() ? 1 : 0);
            }
        }

        return lhs;
    }

    ftcl::expected<Datum, Exception> parse_logical_and() {
        auto lhs = parse_bitwise_or();
        if (!lhs.has_value()) {
            return lhs;
        }

        while (match2('&', '&')) {
            if (evaluate_ && !lhs->is_true()) {
                EvalModeGuard guard(this, false);
                auto skipped = parse_bitwise_or();
                if (!skipped.has_value()) {
                    return skipped;
                }
                lhs = Datum::from_int(0);
                continue;
            }

            auto rhs = parse_bitwise_or();
            if (!rhs.has_value()) {
                return rhs;
            }

            if (!evaluate_) {
                lhs = Datum::from_int(0);
            } else {
                lhs = Datum::from_int(lhs->is_true() && rhs->is_true() ? 1 : 0);
            }
        }

        return lhs;
    }

    ftcl::expected<Datum, Exception> parse_bitwise_or() {
        auto lhs = parse_bitwise_xor();
        if (!lhs.has_value()) {
            return lhs;
        }

        while (match('|')) {
            if (match('|')) {
                --pos_;
                --pos_;
                break;
            }

            auto rhs = parse_bitwise_xor();
            if (!rhs.has_value()) {
                return rhs;
            }

            if (!evaluate_) {
                lhs = Datum::from_int(0);
                continue;
            }

            auto li = int_for_bitwise(*lhs, "|");
            if (!li.has_value()) {
                return ftcl::unexpected(li.error());
            }
            auto ri = int_for_bitwise(*rhs, "|");
            if (!ri.has_value()) {
                return ftcl::unexpected(ri.error());
            }

            lhs = Datum::from_int((*li) | (*ri));
        }

        return lhs;
    }

    ftcl::expected<Datum, Exception> parse_bitwise_xor() {
        auto lhs = parse_bitwise_and();
        if (!lhs.has_value()) {
            return lhs;
        }

        while (match('^')) {
            auto rhs = parse_bitwise_and();
            if (!rhs.has_value()) {
                return rhs;
            }

            if (!evaluate_) {
                lhs = Datum::from_int(0);
                continue;
            }

            auto li = int_for_bitwise(*lhs, "^");
            if (!li.has_value()) {
                return ftcl::unexpected(li.error());
            }
            auto ri = int_for_bitwise(*rhs, "^");
            if (!ri.has_value()) {
                return ftcl::unexpected(ri.error());
            }

            lhs = Datum::from_int((*li) ^ (*ri));
        }

        return lhs;
    }

    ftcl::expected<Datum, Exception> parse_bitwise_and() {
        auto lhs = parse_equality();
        if (!lhs.has_value()) {
            return lhs;
        }

        while (match('&')) {
            if (match('&')) {
                --pos_;
                --pos_;
                break;
            }

            auto rhs = parse_equality();
            if (!rhs.has_value()) {
                return rhs;
            }

            if (!evaluate_) {
                lhs = Datum::from_int(0);
                continue;
            }

            auto li = int_for_bitwise(*lhs, "&");
            if (!li.has_value()) {
                return ftcl::unexpected(li.error());
            }
            auto ri = int_for_bitwise(*rhs, "&");
            if (!ri.has_value()) {
                return ftcl::unexpected(ri.error());
            }

            lhs = Datum::from_int((*li) & (*ri));
        }

        return lhs;
    }

    ftcl::expected<Datum, Exception> parse_equality() {
        auto lhs = parse_relational();
        if (!lhs.has_value()) {
            return lhs;
        }

        while (true) {
            if (match2('=', '=')) {
                auto rhs = parse_relational();
                if (!rhs.has_value()) {
                    return rhs;
                }

                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                    continue;
                }

                if (lhs->type == DatumType::String || rhs->type == DatumType::String) {
                    lhs = Datum::from_int(lhs->as_string() == rhs->as_string() ? 1 : 0);
                } else if (lhs->type == DatumType::Float || rhs->type == DatumType::Float) {
                    lhs = Datum::from_int(lhs->as_float() == rhs->as_float() ? 1 : 0);
                } else {
                    lhs = Datum::from_int(lhs->as_int() == rhs->as_int() ? 1 : 0);
                }
                continue;
            }

            if (match2('!', '=')) {
                auto rhs = parse_relational();
                if (!rhs.has_value()) {
                    return rhs;
                }

                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                    continue;
                }

                if (lhs->type == DatumType::String || rhs->type == DatumType::String) {
                    lhs = Datum::from_int(lhs->as_string() != rhs->as_string() ? 1 : 0);
                } else if (lhs->type == DatumType::Float || rhs->type == DatumType::Float) {
                    lhs = Datum::from_int(lhs->as_float() != rhs->as_float() ? 1 : 0);
                } else {
                    lhs = Datum::from_int(lhs->as_int() != rhs->as_int() ? 1 : 0);
                }
                continue;
            }

            if (match_keyword("eq")) {
                auto rhs = parse_relational();
                if (!rhs.has_value()) {
                    return rhs;
                }

                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                    continue;
                }

                lhs = Datum::from_int(lhs->as_string() == rhs->as_string() ? 1 : 0);
                continue;
            }

            if (match_keyword("ne")) {
                auto rhs = parse_relational();
                if (!rhs.has_value()) {
                    return rhs;
                }

                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                    continue;
                }

                lhs = Datum::from_int(lhs->as_string() != rhs->as_string() ? 1 : 0);
                continue;
            }

            if (match_keyword("in")) {
                auto rhs = parse_relational();
                if (!rhs.has_value()) {
                    return rhs;
                }

                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                    continue;
                }

                auto list = Value(rhs->as_string()).as_list();
                if (!list.has_value()) {
                    return ftcl::unexpected(Exception::ftcl_err(Value(list.error())));
                }

                bool found = false;
                for (const auto& item : *list) {
                    if (item.as_string() == lhs->as_string()) {
                        found = true;
                        break;
                    }
                }

                lhs = Datum::from_int(found ? 1 : 0);
                continue;
            }

            if (match_keyword("ni")) {
                auto rhs = parse_relational();
                if (!rhs.has_value()) {
                    return rhs;
                }

                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                    continue;
                }

                auto list = Value(rhs->as_string()).as_list();
                if (!list.has_value()) {
                    return ftcl::unexpected(Exception::ftcl_err(Value(list.error())));
                }

                bool found = false;
                for (const auto& item : *list) {
                    if (item.as_string() == lhs->as_string()) {
                        found = true;
                        break;
                    }
                }

                lhs = Datum::from_int(found ? 0 : 1);
                continue;
            }

            break;
        }

        return lhs;
    }

    ftcl::expected<Datum, Exception> parse_shift() {
        auto lhs = parse_additive();
        if (!lhs.has_value()) {
            return lhs;
        }

        while (true) {
            bool is_left_shift = false;
            if (match2('<', '<')) {
                is_left_shift = true;
            } else if (match2('>', '>')) {
                is_left_shift = false;
            } else {
                break;
            }

            auto rhs = parse_additive();
            if (!rhs.has_value()) {
                return rhs;
            }

            if (!evaluate_) {
                lhs = Datum::from_int(0);
                continue;
            }

            auto li = int_for_bitwise(*lhs, is_left_shift ? "<<" : ">>");
            if (!li.has_value()) {
                return ftcl::unexpected(li.error());
            }
            auto ri = int_for_bitwise(*rhs, is_left_shift ? "<<" : ">>");
            if (!ri.has_value()) {
                return ftcl::unexpected(ri.error());
            }

            ftcl::expected<ftclInt, Exception> shifted = is_left_shift ? checked_lshift(*li, *ri) : checked_rshift(*li, *ri);
            if (!shifted.has_value()) {
                return ftcl::unexpected(shifted.error());
            }

            lhs = Datum::from_int(*shifted);
        }

        return lhs;
    }

    ftcl::expected<Datum, Exception> parse_relational() {
        auto lhs = parse_shift();
        if (!lhs.has_value()) {
            return lhs;
        }

        while (true) {
            if (match2('<', '=')) {
                auto rhs = parse_shift();
                if (!rhs.has_value()) {
                    return rhs;
                }
                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                } else {
                    lhs = Datum::from_int(compare_for_relational(*lhs, *rhs) <= 0 ? 1 : 0);
                }
                continue;
            }

            if (match2('>', '=')) {
                auto rhs = parse_shift();
                if (!rhs.has_value()) {
                    return rhs;
                }
                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                } else {
                    lhs = Datum::from_int(compare_for_relational(*lhs, *rhs) >= 0 ? 1 : 0);
                }
                continue;
            }

            if (match('<')) {
                auto rhs = parse_shift();
                if (!rhs.has_value()) {
                    return rhs;
                }
                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                } else {
                    lhs = Datum::from_int(compare_for_relational(*lhs, *rhs) < 0 ? 1 : 0);
                }
                continue;
            }

            if (match('>')) {
                auto rhs = parse_shift();
                if (!rhs.has_value()) {
                    return rhs;
                }
                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                } else {
                    lhs = Datum::from_int(compare_for_relational(*lhs, *rhs) > 0 ? 1 : 0);
                }
                continue;
            }

            break;
        }

        return lhs;
    }

    ftcl::expected<Datum, Exception> parse_additive() {
        auto lhs = parse_multiplicative();
        if (!lhs.has_value()) {
            return lhs;
        }

        while (true) {
            if (match('+')) {
                auto rhs = parse_multiplicative();
                if (!rhs.has_value()) {
                    return rhs;
                }

                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                    continue;
                }

                if (lhs->type == DatumType::String || rhs->type == DatumType::String) {
                    lhs = Datum::from_string(lhs->as_string() + rhs->as_string());
                } else if (lhs->type == DatumType::Float || rhs->type == DatumType::Float) {
                    lhs = Datum::from_float(lhs->as_float() + rhs->as_float());
                } else {
                    auto sum = checked_add(lhs->as_int(), rhs->as_int());
                    if (!sum.has_value()) {
                        return ftcl::unexpected(sum.error());
                    }
                    lhs = Datum::from_int(*sum);
                }
                continue;
            }

            if (match('-')) {
                auto rhs = parse_multiplicative();
                if (!rhs.has_value()) {
                    return rhs;
                }

                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                    continue;
                }

                if (lhs->type == DatumType::Float || rhs->type == DatumType::Float) {
                    lhs = Datum::from_float(lhs->as_float() - rhs->as_float());
                } else {
                    auto diff = checked_sub(lhs->as_int(), rhs->as_int());
                    if (!diff.has_value()) {
                        return ftcl::unexpected(diff.error());
                    }
                    lhs = Datum::from_int(*diff);
                }
                continue;
            }

            break;
        }

        return lhs;
    }

    ftcl::expected<Datum, Exception> parse_multiplicative() {
        auto lhs = parse_unary();
        if (!lhs.has_value()) {
            return lhs;
        }

        while (true) {
            if (match('*')) {
                auto rhs = parse_unary();
                if (!rhs.has_value()) {
                    return rhs;
                }

                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                    continue;
                }

                if (lhs->type == DatumType::Float || rhs->type == DatumType::Float) {
                    lhs = Datum::from_float(lhs->as_float() * rhs->as_float());
                } else {
                    auto prod = checked_mul(lhs->as_int(), rhs->as_int());
                    if (!prod.has_value()) {
                        return ftcl::unexpected(prod.error());
                    }
                    lhs = Datum::from_int(*prod);
                }
                continue;
            }

            if (match('/')) {
                auto rhs = parse_unary();
                if (!rhs.has_value()) {
                    return rhs;
                }

                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                    continue;
                }

                if (lhs->type == DatumType::Float || rhs->type == DatumType::Float) {
                    if (rhs->as_float() == 0.0) {
                        return ftcl::unexpected(Exception::ftcl_err(Value("divide by zero")));
                    }
                    lhs = Datum::from_float(lhs->as_float() / rhs->as_float());
                } else {
                    auto quo = checked_div(lhs->as_int(), rhs->as_int());
                    if (!quo.has_value()) {
                        return ftcl::unexpected(quo.error());
                    }
                    lhs = Datum::from_int(*quo);
                }
                continue;
            }

            if (match('%')) {
                auto rhs = parse_unary();
                if (!rhs.has_value()) {
                    return rhs;
                }

                if (!evaluate_) {
                    lhs = Datum::from_int(0);
                    continue;
                }

                if (lhs->type == DatumType::Float || rhs->type == DatumType::Float) {
                    return ftcl::unexpected(
                        Exception::ftcl_err(Value("can't use floating-point value as operand of \"%\"")));
                }

                auto rem = checked_mod(lhs->as_int(), rhs->as_int());
                if (!rem.has_value()) {
                    return ftcl::unexpected(rem.error());
                }
                lhs = Datum::from_int(*rem);
                continue;
            }

            break;
        }

        return lhs;
    }

    ftcl::expected<Datum, Exception> parse_unary() {
        if (match('!')) {
            auto v = parse_unary();
            if (!v.has_value()) {
                return v;
            }
            if (!evaluate_) {
                return Datum::from_int(0);
            }
            return Datum::from_int(v->is_true() ? 0 : 1);
        }

        if (match('-')) {
            auto v = parse_unary();
            if (!v.has_value()) {
                return v;
            }
            if (!evaluate_) {
                return Datum::from_int(0);
            }
            if (v->type == DatumType::Float) {
                return Datum::from_float(-v->as_float());
            }
            if (v->type == DatumType::Int) {
                if (v->as_int() == std::numeric_limits<ftclInt>::min()) {
                    return ftcl::unexpected(Exception::ftcl_err(Value("integer overflow")));
                }
                return Datum::from_int(-v->as_int());
            }

            auto parsed = Value(v->as_string()).as_float_opt();
            if (!parsed.has_value()) {
                return ftcl::unexpected(Exception::ftcl_err(Value("expected number but got \"" + v->as_string() + "\"")));
            }
            return Datum::from_float(-*parsed);
        }

        if (match('+')) {
            return parse_unary();
        }

        if (match('~')) {
            auto v = parse_unary();
            if (!v.has_value()) {
                return v;
            }
            if (!evaluate_) {
                return Datum::from_int(0);
            }

            auto iv = int_for_bitwise(*v, "~");
            if (!iv.has_value()) {
                return ftcl::unexpected(iv.error());
            }
            return Datum::from_int(~(*iv));
        }

        return parse_primary();
    }

    ftcl::expected<Datum, Exception> parse_primary() {
        skip_white();

        if (match('(')) {
            auto value = parse_conditional();
            if (!value.has_value()) {
                return value;
            }

            if (!match(')')) {
                return ftcl::unexpected(Exception::ftcl_err(Value("missing close parenthesis in expression")));
            }

            return value;
        }

        if (at_end()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("missing operand in expression")));
        }

        // Variable reference: $name
        if (peek() == '$') {
            return parse_variable_reference();
        }

        // Command substitution: [script]
        if (peek() == '[') {
            return parse_command_substitution();
        }

        // Number
        if (std::isdigit(static_cast<unsigned char>(peek())) != 0 || peek() == '.') {
            std::size_t start = pos_;

            while (!at_end() &&
                   (std::isdigit(static_cast<unsigned char>(peek())) != 0 || peek() == '.' || peek() == 'e' ||
                    peek() == 'E' || peek() == '+' || peek() == '-')) {
                char ch = peek();
                if ((ch == '+' || ch == '-') && pos_ > start) {
                    const char prev = input_[pos_ - 1];
                    if (prev != 'e' && prev != 'E') {
                        break;
                    }
                }
                ++pos_;
            }

            std::string token(input_.substr(start, pos_ - start));
            if (token.find('.') != std::string::npos || token.find('e') != std::string::npos ||
                token.find('E') != std::string::npos) {
                auto parsed = Value(token).as_float_opt();
                if (!parsed.has_value()) {
                    return ftcl::unexpected(Exception::ftcl_err(Value("invalid floating-point number \"" + token + "\"")));
                }
                return Datum::from_float(*parsed);
            }

            auto parsed = Value(token).as_int_opt();
            if (!parsed.has_value()) {
                return ftcl::unexpected(Exception::ftcl_err(Value("invalid integer \"" + token + "\"")));
            }
            return Datum::from_int(*parsed);
        }

        // String literal in quotes.
        if (peek() == '"') {
            auto quoted = parse_quoted_string();
            if (!quoted.has_value()) {
                return ftcl::unexpected(quoted.error());
            }
            return Datum::from_string(*quoted);
        }

        // Braced string literal, typically used for list constants in "in"/"ni".
        if (peek() == '{') {
            auto braced = parse_braced_string();
            if (!braced.has_value()) {
                return ftcl::unexpected(braced.error());
            }
            return Datum::from_string(*braced);
        }

        // bareword
        std::string bare;
        while (!at_end()) {
            const char ch = peek();
            if (std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == ')' || ch == '(' || ch == '+' || ch == '-' ||
                ch == '*' || ch == '/' || ch == '%' || ch == '<' || ch == '>' || ch == '=' || ch == '!' || ch == '&' ||
                ch == '|' || ch == '^' || ch == '?' || ch == ':' || ch == '~') {
                break;
            }
            bare.push_back(ch);
            ++pos_;
        }

        if (bare.empty()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("syntax error in expression")));
        }

        if (bare == "true" || bare == "yes" || bare == "on") {
            return Datum::from_int(1);
        }

        if (bare == "false" || bare == "no" || bare == "off") {
            return Datum::from_int(0);
        }

        if (!evaluate_) {
            return Datum::from_int(0);
        }

        return ftcl::unexpected(Exception::ftcl_err(Value("unknown math function \"" + bare + "\"")));
    }
};

inline ftclResult expr(Interp* interp, const Value& expression) {
    ExprParser parser(interp, expression.as_string());
    auto parsed = parser.parse();
    if (!parsed.has_value()) {
        return ftcl::unexpected(parsed.error());
    }

    switch (parsed->type) {
        case DatumType::Int:
            return ftcl_ok(parsed->int_value);
        case DatumType::Float:
            return ftcl_ok(parsed->float_value);
        case DatumType::String:
            return ftcl_ok(parsed->string_value);
    }

    return ftcl_ok();
}

}  // namespace ftcl
