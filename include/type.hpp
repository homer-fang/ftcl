#pragma once

#include "expected_compat.hpp"
#include "value.hpp"

#include <cassert>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ftcl {

// Forward declarations.
class Interp;

/**
 * Standard result codes compatible with Tcl/ftcl.
 */
enum class ResultCode : ftclInt {
    Okay = 0,
    Error = 1,
    Return = 2,
    Break = 3,
    Continue = 4,
};

/**
 * Encodes either a standard result code or a user-defined integer code.
 */
class ResultCodeValue {
public:
    ResultCodeValue()
        : value_(ResultCode::Okay) {}

    explicit ResultCodeValue(ResultCode code)
        : value_(code) {}

    explicit ResultCodeValue(ftclInt code) {
        set_from_int(code);
    }

    bool is_standard() const {
        return std::holds_alternative<ResultCode>(value_);
    }

    bool is_other() const {
        return std::holds_alternative<ftclInt>(value_);
    }

    ResultCode standard_code() const {
        return std::get<ResultCode>(value_);
    }

    ftclInt other_code() const {
        return std::get<ftclInt>(value_);
    }

    ftclInt as_int() const {
        if (is_standard()) {
            return static_cast<ftclInt>(standard_code());
        }
        return other_code();
    }

    std::string to_string() const {
        if (is_standard()) {
            switch (standard_code()) {
                case ResultCode::Okay:
                    return "ok";
                case ResultCode::Error:
                    return "error";
                case ResultCode::Return:
                    return "return";
                case ResultCode::Break:
                    return "break";
                case ResultCode::Continue:
                    return "continue";
            }
        }

        return std::to_string(other_code());
    }

    static std::optional<ResultCodeValue> from_string(const std::string& input) {
        if (input == "ok") {
            return ResultCodeValue(ResultCode::Okay);
        }
        if (input == "error") {
            return ResultCodeValue(ResultCode::Error);
        }
        if (input == "return") {
            return ResultCodeValue(ResultCode::Return);
        }
        if (input == "break") {
            return ResultCodeValue(ResultCode::Break);
        }
        if (input == "continue") {
            return ResultCodeValue(ResultCode::Continue);
        }

        try {
            std::size_t pos = 0;
            const auto v = std::stoll(input, &pos, 10);
            if (pos != input.size()) {
                return std::nullopt;
            }
            return ResultCodeValue(static_cast<ftclInt>(v));
        } catch (...) {
            return std::nullopt;
        }
    }

    bool operator==(const ResultCodeValue& rhs) const {
        return as_int() == rhs.as_int();
    }

    bool operator!=(const ResultCodeValue& rhs) const {
        return !(*this == rhs);
    }

private:
    std::variant<ResultCode, ftclInt> value_;

    void set_from_int(ftclInt code) {
        switch (code) {
            case 0:
                value_ = ResultCode::Okay;
                break;
            case 1:
                value_ = ResultCode::Error;
                break;
            case 2:
                value_ = ResultCode::Return;
                break;
            case 3:
                value_ = ResultCode::Break;
                break;
            case 4:
                value_ = ResultCode::Continue;
                break;
            default:
                value_ = code;
                break;
        }
    }
};

class ErrorData {
public:
    ErrorData(const Value& error_code, const std::string& error_msg)
        : error_code_(error_code),
          stack_trace_{error_msg},
          is_new_(true) {}

    ErrorData(const Value& error_code, const std::string& error_info, bool is_rethrow)
        : error_code_(error_code),
          stack_trace_{error_info},
          is_new_(!is_rethrow) {}

    Value error_code() const {
        return error_code_;
    }

    Value error_info() const {
        std::string out;
        for (std::size_t i = 0; i < stack_trace_.size(); ++i) {
            if (i > 0) {
                out.push_back('\n');
            }
            out += stack_trace_[i];
        }
        return Value(out);
    }

    bool is_new() const {
        return is_new_;
    }

    void add_info(const std::string& info) {
        stack_trace_.push_back(info);
        is_new_ = false;
    }

private:
    Value error_code_;
    std::vector<std::string> stack_trace_;
    bool is_new_;
};

/**
 * Captures exceptional returns from Tcl/ftcl execution.
 */
class Exception {
public:
    Exception(ResultCodeValue code,
              const Value& value,
              std::size_t level = 0,
              ResultCodeValue next_code = ResultCodeValue(ResultCode::Error),
              std::optional<ErrorData> error_data = std::nullopt)
        : code_(std::move(code)),
          value_(value),
          level_(level),
          next_code_(std::move(next_code)),
          error_data_(std::move(error_data)) {}

    static Exception ftcl_err(const Value& msg) {
        return Exception(
            ResultCodeValue(ResultCode::Error),
            msg,
            0,
            ResultCodeValue(ResultCode::Error),
            ErrorData(Value("NONE"), msg.as_string()));
    }

    static Exception ftcl_err2(const Value& error_code, const Value& msg) {
        return Exception(
            ResultCodeValue(ResultCode::Error),
            msg,
            0,
            ResultCodeValue(ResultCode::Error),
            ErrorData(error_code, msg.as_string()));
    }

    static Exception ftcl_return(const Value& value) {
        return Exception(
            ResultCodeValue(ResultCode::Return),
            value,
            1,
            ResultCodeValue(ResultCode::Okay),
            std::nullopt);
    }

    static Exception ftcl_return_ext(const Value& value,
                                     std::size_t level,
                                     ResultCodeValue next_code) {
        return Exception(
            level > 0 ? ResultCodeValue(ResultCode::Return) : next_code,
            value,
            level,
            next_code,
            std::nullopt);
    }

    static Exception ftcl_return_err(const Value& msg,
                                     std::size_t level,
                                     const std::optional<Value>& error_code = std::nullopt,
                                     const std::optional<Value>& error_info = std::nullopt) {
        const Value ec = error_code.value_or(Value("NONE"));
        const Value ei = error_info.value_or(Value(""));

        return Exception(
            level == 0 ? ResultCodeValue(ResultCode::Error) : ResultCodeValue(ResultCode::Return),
            msg,
            level,
            ResultCodeValue(ResultCode::Error),
            ErrorData(ec, ei.as_string(), true));
    }

    static Exception ftcl_break() {
        return Exception(
            ResultCodeValue(ResultCode::Break),
            Value::empty(),
            0,
            ResultCodeValue(ResultCode::Break),
            std::nullopt);
    }

    static Exception ftcl_continue() {
        return Exception(
            ResultCodeValue(ResultCode::Continue),
            Value::empty(),
            0,
            ResultCodeValue(ResultCode::Continue),
            std::nullopt);
    }

    ResultCodeValue code() const {
        return code_;
    }

    Value value() const {
        return value_;
    }

    std::size_t level() const {
        return level_;
    }

    ResultCodeValue next_code() const {
        return next_code_;
    }

    bool is_error() const {
        return code_.is_standard() && code_.standard_code() == ResultCode::Error;
    }

    const ErrorData* error_data() const {
        return error_data_ ? &(*error_data_) : nullptr;
    }

    Value error_code() const {
        if (!error_data_) {
            throw std::logic_error("exception is not an error");
        }
        return error_data_->error_code();
    }

    Value error_info() const {
        if (!error_data_) {
            throw std::logic_error("exception is not an error");
        }
        return error_data_->error_info();
    }

    void add_error_info(const std::string& line) {
        if (!error_data_) {
            throw std::logic_error("add_error_info called for non-Error Exception");
        }
        error_data_->add_info(line);
    }

    void decrement_level() {
        if (!(code_.is_standard() && code_.standard_code() == ResultCode::Return && level_ > 0)) {
            return;
        }

        --level_;
        if (level_ == 0) {
            code_ = next_code_;
        }
    }

    bool is_new_error() const {
        return error_data_.has_value() && error_data_->is_new();
    }

private:
    ResultCodeValue code_;
    Value value_;
    std::size_t level_;
    ResultCodeValue next_code_;
    std::optional<ErrorData> error_data_;
};

using ftclResult = ftcl::expected<Value, Exception>;
using ftclException = Exception;
using ftclResultCode = ResultCodeValue;

class ContextID {
public:
    explicit ContextID(std::uint64_t id = 0)
        : id_(id) {}

    std::uint64_t id() const {
        return id_;
    }

    bool operator==(const ContextID& rhs) const {
        return id_ == rhs.id_;
    }

    bool operator!=(const ContextID& rhs) const {
        return !(*this == rhs);
    }

    struct Hash {
        std::size_t operator()(const ContextID& id) const noexcept {
            return std::hash<std::uint64_t>{}(id.id_);
        }
    };

private:
    std::uint64_t id_;
};

using CommandFunc = std::function<ftclResult(Interp*, ContextID, const std::vector<Value>&)>;

class Subcommand {
public:
    Subcommand(std::string name, CommandFunc func)
        : name_(std::move(name)),
          func_(std::move(func)) {}

    const std::string& name() const {
        return name_;
    }

    const CommandFunc& func() const {
        return func_;
    }

private:
    std::string name_;
    CommandFunc func_;
};

inline ftclResult ftcl_ok(const Value& value) {
    return ftclResult(value);
}

inline ftclResult ftcl_ok(const std::string& value) {
    return ftclResult(Value(value));
}

inline ftclResult ftcl_ok(const char* value) {
    return ftclResult(Value(value));
}

inline ftclResult ftcl_ok(ftclInt value) {
    return ftclResult(Value(value));
}

inline ftclResult ftcl_ok(ftclFloat value) {
    return ftclResult(Value(value));
}

inline ftclResult ftcl_ok(bool value) {
    return ftclResult(Value(value));
}

inline ftclResult ftcl_ok(const ftclList& value) {
    return ftclResult(Value::from_list(value));
}

inline ftclResult ftcl_ok(const ftclDict& value) {
    return ftclResult(Value::from_dict(value));
}

inline ftclResult ftcl_ok() {
    return ftclResult(Value::empty());
}

inline ftclResult ftcl_err(const Value& msg) {
    return ftcl::unexpected(Exception::ftcl_err(msg));
}

inline ftclResult ftcl_err(const std::string& msg) {
    return ftcl::unexpected(Exception::ftcl_err(Value(msg)));
}

inline ftclResult ftcl_err(const char* msg) {
    return ftcl::unexpected(Exception::ftcl_err(Value(msg)));
}

inline ftclResult ftcl_err2(const Value& error_code, const Value& msg) {
    return ftcl::unexpected(Exception::ftcl_err2(error_code, msg));
}

}  // namespace ftcl

