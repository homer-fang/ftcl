#pragma once

#include "expected_compat.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <functional>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <type_traits>
#include <vector>
#include <variant>

namespace ftcl {

using ftclInt = std::int64_t;
using ftclFloat = double;

class VarName {
public:
    static VarName scalar(std::string name) {
        VarName vn;
        vn.name_ = std::move(name);
        vn.index_ = std::nullopt;
        return vn;
    }

    static VarName array(std::string name, std::string index) {
        VarName vn;
        vn.name_ = std::move(name);
        vn.index_ = std::move(index);
        return vn;
    }

    VarName() = default;

    explicit VarName(std::string literal) {
        parse_literal(literal);
    }

    const std::string& name() const {
        return name_;
    }

    const std::optional<std::string>& index() const {
        return index_;
    }

    bool is_array() const {
        return index_.has_value();
    }

    bool is_scalar() const {
        return !index_.has_value();
    }

    std::string to_string() const {
        if (index_.has_value()) {
            return name_ + "(" + *index_ + ")";
        }
        return name_;
    }

private:
    std::string name_;
    std::optional<std::string> index_;

    void parse_literal(const std::string& literal) {
        if (literal.empty()) {
            name_.clear();
            index_.reset();
            return;
        }

        const std::size_t open = literal.find('(');
        if (open == std::string::npos || literal.back() != ')') {
            name_ = literal;
            index_.reset();
            return;
        }

        name_ = literal.substr(0, open);
        if (open + 1 > literal.size() - 1) {
            name_ = literal;
            index_.reset();
            return;
        }

        index_ = literal.substr(open + 1, literal.size() - open - 2);
    }
};

class Value;
using ftclList = std::vector<Value>;
using ftclDict = std::map<std::string, Value>;

class ftclAny {
public:
    virtual ~ftclAny() = default;
    virtual std::string to_string() const = 0;
    virtual std::string debug_string() const = 0;
};

class DataRep {
public:
    enum class Type {
        None,
        Bool,
        Int,
        Float,
        VarName,
        Other,
    };

    using Variant = std::variant<
        std::monostate,
        bool,
        ftclInt,
        ftclFloat,
        std::shared_ptr<VarName>,
        std::shared_ptr<ftclAny>>;

    DataRep() : data_(std::monostate{}) {}
    explicit DataRep(bool v) : data_(v) {}
    explicit DataRep(ftclInt v) : data_(v) {}
    explicit DataRep(ftclFloat v) : data_(v) {}
    explicit DataRep(std::shared_ptr<VarName> v) : data_(std::move(v)) {}
    explicit DataRep(std::shared_ptr<ftclAny> v) : data_(std::move(v)) {}

    Type type() const {
        return std::visit(
            [](const auto& arg) -> Type {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    return Type::None;
                }
                if constexpr (std::is_same_v<T, bool>) {
                    return Type::Bool;
                }
                if constexpr (std::is_same_v<T, ftclInt>) {
                    return Type::Int;
                }
                if constexpr (std::is_same_v<T, ftclFloat>) {
                    return Type::Float;
                }
                if constexpr (std::is_same_v<T, std::shared_ptr<VarName>>) {
                    return Type::VarName;
                }
                return Type::Other;
            },
            data_);
    }

    const Variant& variant() const {
        return data_;
    }

private:
    Variant data_;
};

class InnerValue {
public:
    InnerValue() = default;

    explicit InnerValue(std::string str)
        : string_rep_(std::move(str)) {}

    explicit InnerValue(DataRep data)
        : data_rep_(std::move(data)) {}

    const std::string& get_string_rep() const {
        if (!string_rep_.has_value()) {
            string_rep_ = data_to_string(data_rep_);
        }
        return *string_rep_;
    }

    void set_string_rep(std::string str) {
        string_rep_ = std::move(str);
    }

    bool has_string_rep() const {
        return string_rep_.has_value();
    }

    const DataRep& get_data_rep() const {
        return data_rep_;
    }

    DataRep& get_data_rep_mut() {
        return data_rep_;
    }

    void set_data_rep(DataRep data) {
        data_rep_ = std::move(data);
    }

private:
    static bool same_float_bits(ftclFloat a, ftclFloat b) {
        if (std::isnan(a) && std::isnan(b)) {
            return true;
        }

        return std::memcmp(&a, &b, sizeof(ftclFloat)) == 0;
    }

    static std::string fmt_float(ftclFloat value) {
        if (std::isnan(value)) {
            return "NaN";
        }

        if (value == std::numeric_limits<ftclFloat>::infinity()) {
            return "Inf";
        }

        if (value == -std::numeric_limits<ftclFloat>::infinity()) {
            return "-Inf";
        }

        // Prefer the shortest round-trippable string, similar to Rust's f64 Display.
        std::optional<std::string> best;

        for (int precision = 1; precision <= 17; ++precision) {
            std::ostringstream oss;
            oss.imbue(std::locale::classic());
            oss << std::setprecision(precision) << std::defaultfloat << value;

            const std::string candidate = oss.str();
            char* parse_end = nullptr;
            errno = 0;
            const ftclFloat round_trip = std::strtod(candidate.c_str(), &parse_end);

            if (parse_end != nullptr && *parse_end == '\0' && errno != ERANGE && same_float_bits(round_trip, value)) {
                if (!best.has_value() || candidate.size() < best->size()) {
                    best = candidate;
                    continue;
                }

                if (candidate.size() == best->size()) {
                    const bool best_exp =
                        best->find('e') != std::string::npos || best->find('E') != std::string::npos;
                    const bool cand_exp =
                        candidate.find('e') != std::string::npos || candidate.find('E') != std::string::npos;
                    if (best_exp && !cand_exp) {
                        best = candidate;
                    }
                }
            }
        }

        if (best.has_value()) {
            return *best;
        }

        // Fallback: if available, use floating-point to_chars.
        std::array<char, 128> buffer{};
        auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general);
        if (ec == std::errc{}) {
            return std::string(buffer.data(), static_cast<std::size_t>(end - buffer.data()));
        }

        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        oss << std::setprecision(17) << std::defaultfloat << value;
        return oss.str();
    }

    static std::string data_to_string(const DataRep& rep) {
        switch (rep.type()) {
            case DataRep::Type::None:
                return "";
            case DataRep::Type::Bool: {
                const auto* b = std::get_if<bool>(&rep.variant());
                return (b != nullptr && *b) ? "1" : "0";
            }
            case DataRep::Type::Int: {
                const auto* i = std::get_if<ftclInt>(&rep.variant());
                return i == nullptr ? "" : std::to_string(*i);
            }
            case DataRep::Type::Float: {
                const auto* f = std::get_if<ftclFloat>(&rep.variant());
                return f == nullptr ? "" : fmt_float(*f);
            }
            case DataRep::Type::VarName: {
                const auto* vp = std::get_if<std::shared_ptr<VarName>>(&rep.variant());
                if (vp == nullptr || *vp == nullptr) {
                    return "";
                }
                return (*vp)->to_string();
            }
            case DataRep::Type::Other: {
                const auto* op = std::get_if<std::shared_ptr<ftclAny>>(&rep.variant());
                if (op == nullptr || *op == nullptr) {
                    return "";
                }
                return (*op)->to_string();
            }
        }
        return "";
    }

    mutable std::optional<std::string> string_rep_;
    DataRep data_rep_;
};

class Value {
public:
    Value()
        : inner_(std::make_shared<InnerValue>(std::string())) {}

    explicit Value(const std::string& str)
        : inner_(std::make_shared<InnerValue>(str)) {}

    explicit Value(std::string&& str)
        : inner_(std::make_shared<InnerValue>(std::move(str))) {}

    explicit Value(const char* str)
        : inner_(std::make_shared<InnerValue>(str == nullptr ? std::string() : std::string(str))) {}

    explicit Value(bool b)
        : inner_(std::make_shared<InnerValue>(DataRep(b))) {}

    explicit Value(ftclInt i)
        : inner_(std::make_shared<InnerValue>(DataRep(i))) {}

    explicit Value(ftclFloat f)
        : inner_(std::make_shared<InnerValue>(DataRep(f))) {}

    explicit Value(DataRep rep)
        : inner_(std::make_shared<InnerValue>(std::move(rep))) {}

    static Value empty() {
        return Value("");
    }

    static Value from_string(const std::string& s) {
        return Value(s);
    }

    static Value from_list(const ftclList& list) {
        std::string out;
        bool first = true;
        for (const auto& item : list) {
            if (!first) {
                out.push_back(' ');
            }
            first = false;
            out += list_quote(item.as_string());
        }
        return Value(out);
    }

    static Value from_dict(const ftclDict& dict) {
        ftclList items;
        items.reserve(dict.size() * 2);
        for (const auto& [k, v] : dict) {
            items.emplace_back(k);
            items.emplace_back(v);
        }
        return from_list(items);
    }

    const std::string& as_string() const {
        return inner_->get_string_rep();
    }

    const std::string& as_str() const {
        return as_string();
    }

    const char* as_cstr() const {
        return as_string().c_str();
    }

    std::string to_string() const {
        return as_string();
    }

    std::optional<ftclInt> as_int_opt() const {
        const std::string& s = as_string();
        if (s.empty()) {
            return std::nullopt;
        }

        ftclInt value = 0;
        const char* begin = s.data();
        const char* end = s.data() + s.size();
        auto [ptr, ec] = std::from_chars(begin, end, value, 10);
        if (ec != std::errc() || ptr != end) {
            return std::nullopt;
        }
        return value;
    }

    std::optional<ftclFloat> as_float_opt() const {
        const std::string& s = as_string();
        if (s.empty()) {
            return std::nullopt;
        }

        char* tail = nullptr;
        const double value = std::strtod(s.c_str(), &tail);
        if (tail == nullptr || *tail != '\0') {
            return std::nullopt;
        }
        return value;
    }

    std::optional<bool> as_bool_opt() const {
        std::string s = as_string();
        std::transform(
            s.begin(),
            s.end(),
            s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (s == "1" || s == "true" || s == "yes" || s == "on") {
            return true;
        }
        if (s == "0" || s == "false" || s == "no" || s == "off") {
            return false;
        }
        return std::nullopt;
    }

    ftcl::expected<ftclList, std::string> as_list() const {
        return parse_list(as_string());
    }

    ftcl::expected<ftclDict, std::string> as_dict() const {
        auto list = as_list();
        if (!list.has_value()) {
            return ftcl::unexpected(list.error());
        }

        if (list->size() % 2 != 0) {
            return ftcl::unexpected("missing value to go with key");
        }

        ftclDict dict;
        for (std::size_t i = 0; i < list->size(); i += 2) {
            dict[(*list)[i].as_string()] = (*list)[i + 1];
        }
        return dict;
    }

    VarName as_var_name() const {
        return VarName(as_string());
    }

    DataRep& data_rep_mut() {
        return inner_->get_data_rep_mut();
    }

    const DataRep& data_rep() const {
        return inner_->get_data_rep();
    }

    bool operator==(const Value& rhs) const {
        return as_string() == rhs.as_string();
    }

    bool operator!=(const Value& rhs) const {
        return !(*this == rhs);
    }

private:
    static bool is_plain_list_element(std::string_view s) {
        if (s.empty()) {
            return false;
        }

        for (char ch : s) {
            if (std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '{' || ch == '}' || ch == '"' ||
                ch == ';' || ch == '[' || ch == ']' || ch == '$' || ch == '\\') {
                return false;
            }
        }

        return true;
    }

    static bool can_use_braces(std::string_view s) {
        if (s.empty()) {
            return true;
        }

        if (s.back() == '\\') {
            return false;
        }

        int depth = 0;
        for (char ch : s) {
            if (ch == '{') {
                ++depth;
            } else if (ch == '}') {
                if (depth == 0) {
                    return false;
                }
                --depth;
            }
        }

        return depth == 0;
    }

    static bool needs_backslash_escape(char ch) {
        return std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '{' || ch == '}' || ch == '"' ||
               ch == ';' || ch == '[' || ch == ']' || ch == '$' || ch == '\\';
    }

    static std::string list_backslash_quote(std::string_view s) {
        std::string out;
        out.reserve(s.size() * 2);
        for (char ch : s) {
            if (needs_backslash_escape(ch)) {
                out.push_back('\\');
            }
            out.push_back(ch);
        }
        return out;
    }

    static std::string list_quote(std::string_view s) {
        if (is_plain_list_element(s)) {
            return std::string(s);
        }

        if (can_use_braces(s)) {
            std::string out;
            out.reserve(s.size() + 2);
            out.push_back('{');
            out.append(s.data(), s.size());
            out.push_back('}');
            return out;
        }

        return list_backslash_quote(s);
    }

    static ftcl::expected<ftclList, std::string> parse_list(std::string_view src) {
        ftclList result;
        std::string token;

        auto push_token = [&]() {
            result.emplace_back(token);
            token.clear();
        };

        std::size_t i = 0;
        while (i < src.size()) {
            while (i < src.size() && std::isspace(static_cast<unsigned char>(src[i])) != 0) {
                ++i;
            }
            if (i >= src.size()) {
                break;
            }

            token.clear();

            if (src[i] == '{') {
                int depth = 1;
                ++i;
                while (i < src.size() && depth > 0) {
                    const char ch = src[i++];
                    if (ch == '\\' && i < src.size()) {
                        token.push_back(src[i++]);
                        continue;
                    }
                    if (ch == '{') {
                        ++depth;
                        token.push_back(ch);
                        continue;
                    }
                    if (ch == '}') {
                        --depth;
                        if (depth > 0) {
                            token.push_back(ch);
                        }
                        continue;
                    }
                    token.push_back(ch);
                }

                if (depth != 0) {
                    return ftcl::unexpected("unmatched open brace in list");
                }

                push_token();
                continue;
            }

            if (src[i] == '"') {
                ++i;
                while (i < src.size() && src[i] != '"') {
                    if (src[i] == '\\' && i + 1 < src.size()) {
                        token.push_back(src[i + 1]);
                        i += 2;
                    } else {
                        token.push_back(src[i++]);
                    }
                }
                if (i >= src.size()) {
                    return ftcl::unexpected("unmatched open quote in list");
                }
                ++i;
                push_token();
                continue;
            }

            while (i < src.size() && std::isspace(static_cast<unsigned char>(src[i])) == 0) {
                if (src[i] == '\\' && i + 1 < src.size()) {
                    token.push_back(src[i + 1]);
                    i += 2;
                } else {
                    token.push_back(src[i++]);
                }
            }

            push_token();
        }

        return result;
    }

    std::shared_ptr<InnerValue> inner_;
};

}  // namespace ftcl

namespace std {
template <>
struct hash<ftcl::Value> {
    std::size_t operator()(const ftcl::Value& v) const noexcept {
        return std::hash<std::string>{}(v.as_string());
    }
};
}  // namespace std

