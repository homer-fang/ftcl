#pragma once

#include "expr.hpp"
#include "parser.hpp"
#include "scope.hpp"
#include "type.hpp"

#include <any>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ftcl {

void install_core_commands(class Interp& interp);

class Interp {
public:
    struct Procedure {
        std::vector<Value> args;
        Value body = Value::empty();
        std::unordered_map<std::string, Value> defaults;
    };

    Interp() = default;

    static Interp new_interp() {
        return Interp{};
    }

    ftclResult eval(std::string_view script) {
        const bool top_level = (eval_depth_ == 0);

        auto parsed = parse_script(script);
        if (!parsed.has_value()) {
            return ftcl::unexpected(parsed.error());
        }

        auto result = eval_parsed_script(*parsed);
        if (top_level) {
            result = normalize_top_level_result(std::move(result));
        }

        return result;
    }

    ftclResult eval_value(const Value& script) {
        return eval(script.as_string());
    }

    bool complete(std::string_view script) {
        auto parsed = parse_script(script);
        return parsed.has_value();
    }

    ftclResult expr(const Value& expression) {
        return ftcl::expr(this, expression);
    }

    ftcl::expected<bool, Exception> expr_bool(const Value& expression) {
        auto result = expr(expression);
        if (!result.has_value()) {
            return ftcl::unexpected(result.error());
        }

        auto b = result->as_bool_opt();
        if (b.has_value()) {
            return *b;
        }

        auto i = result->as_int_opt();
        if (i.has_value()) {
            return *i != 0;
        }

        auto f = result->as_float_opt();
        if (f.has_value()) {
            return *f != 0.0;
        }

        return !result->as_string().empty();
    }

    ftcl::expected<ftclInt, Exception> expr_int(const Value& expression) {
        auto result = expr(expression);
        if (!result.has_value()) {
            return ftcl::unexpected(result.error());
        }

        auto i = result->as_int_opt();
        if (!i.has_value()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("expected integer but got \"" + result->as_string() + "\"")));
        }
        return *i;
    }

    ftcl::expected<ftclFloat, Exception> expr_float(const Value& expression) {
        auto result = expr(expression);
        if (!result.has_value()) {
            return ftcl::unexpected(result.error());
        }

        auto f = result->as_float_opt();
        if (!f.has_value()) {
            return ftcl::unexpected(
                Exception::ftcl_err(Value("expected floating-point number but got \"" + result->as_string() + "\"")));
        }
        return *f;
    }

    // Command registration
    void add_command(const std::string& name, CommandFunc func) {
        commands_[name] = CommandEntry{std::move(func), ContextID(0), false};
    }

    void add_context_command(const std::string& name, CommandFunc func, ContextID context_id) {
        commands_[name] = CommandEntry{std::move(func), context_id, true};
    }

    bool has_command(const std::string& name) const {
        return commands_.find(name) != commands_.end() || procs_.find(name) != procs_.end();
    }

    void remove_command(const std::string& name) {
        commands_.erase(name);
        procs_.erase(name);
    }

    void rename_command(const std::string& old_name, const std::string& new_name) {
        auto it = commands_.find(old_name);
        if (it != commands_.end()) {
            auto node = commands_.extract(it);
            node.key() = new_name;
            commands_.insert(std::move(node));
            return;
        }

        auto pit = procs_.find(old_name);
        if (pit != procs_.end()) {
            auto node = procs_.extract(pit);
            node.key() = new_name;
            procs_.insert(std::move(node));
        }
    }

    std::vector<Value> command_names() const {
        std::vector<Value> out;
        out.reserve(commands_.size() + procs_.size());

        for (const auto& [name, _] : commands_) {
            out.emplace_back(name);
        }
        for (const auto& [name, _] : procs_) {
            out.emplace_back(name);
        }

        return out;
    }

    ftclResult call_subcommand(ContextID context_id,
                               const std::vector<Value>& argv,
                               std::size_t namec,
                               const std::vector<Subcommand>& subcommands) {
        if (argv.size() <= namec) {
            return ftcl_err("wrong # args: missing subcommand");
        }

        const std::string sub_name = argv[namec].as_string();
        for (const auto& sub : subcommands) {
            if (sub.name() == sub_name) {
                return sub.func()(this, context_id, argv);
            }
        }

        std::string names;
        for (std::size_t i = 0; i < subcommands.size(); ++i) {
            if (i > 0) {
                if (i + 1 == subcommands.size()) {
                    names += (subcommands.size() > 2) ? ", or " : " or ";
                } else {
                    names += ", ";
                }
            }
            names += subcommands[i].name();
        }

        return ftcl_err("unknown or ambiguous subcommand \"" + sub_name + "\": must be " + names);
    }

    // Variables
    ftcl::expected<Value, Exception> var(const Value& var_name) const {
        VarName parsed = var_name.as_var_name();
        if (parsed.index().has_value()) {
            return scopes_.get_elem(parsed.name(), *parsed.index());
        }
        return scopes_.get(parsed.name());
    }

    ftcl::expected<Value, Exception> set_var(const Value& var_name, const Value& value) {
        VarName parsed = var_name.as_var_name();
        if (parsed.index().has_value()) {
            return scopes_.set_elem(parsed.name(), *parsed.index(), value);
        }
        return scopes_.set(parsed.name(), value);
    }

    ftcl::expected<Value, Exception> set_var_return(const Value& var_name, const Value& value) {
        return set_var(var_name, value);
    }

    ftcl::expected<Value, Exception> scalar(const std::string& name) const {
        return scopes_.get(name);
    }

    ftcl::expected<Value, Exception> set_scalar(const std::string& name, const Value& value) {
        return scopes_.set(name, value);
    }

    ftcl::expected<Value, Exception> set_scalar_return(const std::string& name, const Value& value) {
        return scopes_.set(name, value);
    }

    ftcl::expected<Value, Exception> element(const std::string& name, const std::string& index) const {
        return scopes_.get_elem(name, index);
    }

    ftcl::expected<Value, Exception> set_element(const std::string& name,
                                                const std::string& index,
                                                const Value& value) {
        return scopes_.set_elem(name, index, value);
    }

    ftcl::expected<Value, Exception> set_element_return(const std::string& name,
                                                       const std::string& index,
                                                       const Value& value) {
        return scopes_.set_elem(name, index, value);
    }

    void unset_var(const Value& var_name) {
        VarName parsed = var_name.as_var_name();
        if (parsed.index().has_value()) {
            scopes_.unset_elem(parsed.name(), *parsed.index());
        } else {
            scopes_.unset(parsed.name());
        }
    }

    bool var_exists(const Value& var_name) const {
        VarName parsed = var_name.as_var_name();
        if (parsed.index().has_value()) {
            auto elem = scopes_.get_elem(parsed.name(), *parsed.index());
            return elem.has_value();
        }
        return scopes_.exists(parsed.name());
    }

    // Array helpers
    bool array_exists(const std::string& name) const {
        return scopes_.array_exists(name);
    }

    std::vector<Value> array_names(const std::string& name) const {
        return scopes_.array_names(name);
    }

    std::vector<Value> array_get(const std::string& name) const {
        return scopes_.array_get(name);
    }

    ftcl::expected<Value, Exception> array_set(const std::string& name, const std::vector<Value>& list) {
        return scopes_.array_set(name, list);
    }

    std::size_t array_size(const std::string& name) const {
        return scopes_.array_size(name);
    }

    void array_unset(const std::string& name) {
        scopes_.array_unset(name);
    }

    void unset_element(const std::string& name, const std::string& index) {
        scopes_.unset_elem(name, index);
    }

    // Scope management
    std::size_t scope_level() const {
        return scopes_.depth() - 1;
    }

    void push_scope() {
        scopes_.push();
    }

    void pop_scope() {
        scopes_.pop();
    }

    void upvar(std::size_t level, const std::string& name, const std::string& alias = std::string()) {
        scopes_.upvar(level, name, alias);
    }

    std::vector<Value> vars_in_scope() const {
        return scopes_.vars_in_scope(scopes_.current());
    }

    std::vector<Value> vars_in_global_scope() const {
        return scopes_.vars_in_scope(0);
    }

    std::vector<Value> vars_in_local_scope() const {
        if (scopes_.current() == 0) {
            return {};
        }
        return scopes_.local_vars_in_scope(scopes_.current());
    }

    // Procedures
    void add_proc(const std::string& name, const std::vector<Value>& args, const Value& body) {
        Procedure proc;
        proc.args = args;
        proc.body = body;

        for (const auto& arg : args) {
            auto parts = arg.as_list();
            if (parts.has_value() && parts->size() == 2) {
                proc.defaults[(*parts)[0].as_string()] = (*parts)[1];
            }
        }

        procs_[name] = std::move(proc);
    }

    std::vector<Value> proc_names() const {
        std::vector<Value> out;
        out.reserve(procs_.size());
        for (const auto& [name, _] : procs_) {
            out.emplace_back(name);
        }
        return out;
    }

    ftcl::expected<Value, Exception> proc_args(const std::string& name) const {
        auto it = procs_.find(name);
        if (it == procs_.end()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("\"" + name + "\" isn't a procedure")));
        }

        std::vector<Value> arg_names;
        arg_names.reserve(it->second.args.size());

        for (const auto& arg : it->second.args) {
            auto parts = arg.as_list();
            if (parts.has_value() && !parts->empty()) {
                arg_names.push_back((*parts)[0]);
            } else {
                arg_names.push_back(arg);
            }
        }

        return Value::from_list(arg_names);
    }

    ftcl::expected<Value, Exception> proc_body(const std::string& name) const {
        auto it = procs_.find(name);
        if (it == procs_.end()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("\"" + name + "\" isn't a procedure")));
        }
        return it->second.body;
    }

    ftcl::expected<std::optional<Value>, Exception> proc_default(const std::string& proc_name,
                                                                const std::string& arg_name) const {
        auto it = procs_.find(proc_name);
        if (it == procs_.end()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("\"" + proc_name + "\" isn't a procedure")));
        }

        bool arg_exists = false;
        for (const auto& arg : it->second.args) {
            auto parts = arg.as_list();
            if (parts.has_value() && !parts->empty()) {
                if ((*parts)[0].as_string() == arg_name) {
                    arg_exists = true;
                    break;
                }
            } else if (arg.as_string() == arg_name) {
                arg_exists = true;
                break;
            }
        }

        if (!arg_exists) {
            return ftcl::unexpected(
                Exception::ftcl_err(Value("procedure \"" + proc_name + "\" doesn't have an argument \"" + arg_name + "\"")));
        }

        auto dit = it->second.defaults.find(arg_name);
        if (dit == it->second.defaults.end()) {
            return std::optional<Value>{};
        }

        return std::optional<Value>{dit->second};
    }

    ftcl::expected<Value, Exception> command_type(const std::string& name) const {
        if (commands_.find(name) != commands_.end()) {
            return Value("native");
        }
        if (procs_.find(name) != procs_.end()) {
            return Value("proc");
        }
        return ftcl::unexpected(Exception::ftcl_err(Value("\"" + name + "\" isn't a command")));
    }

    // Return options
    Value return_options(const ftclResult& result) const {
        ftclDict dict;
        if (result.has_value()) {
            dict["-code"] = Value("0");
            dict["-level"] = Value("0");
        } else {
            const auto& ex = result.error();

            if (ex.code() == ResultCodeValue(ResultCode::Error)) {
                dict["-code"] = Value("1");
                if (ex.error_data() != nullptr) {
                    dict["-errorcode"] = ex.error_code();
                    dict["-errorinfo"] = ex.error_info();
                }
            } else if (ex.code() == ResultCodeValue(ResultCode::Return)) {
                dict["-code"] = Value(std::to_string(ex.next_code().as_int()));
                if (ex.error_data() != nullptr) {
                    dict["-errorcode"] = ex.error_code();
                    dict["-errorinfo"] = ex.error_info();
                }
            } else {
                dict["-code"] = Value(std::to_string(ex.code().as_int()));
            }

            dict["-level"] = Value(static_cast<ftclInt>(ex.level()));
        }
        return Value::from_dict(dict);
    }

    // Profiling helpers
    void profile_dump() const {
        // no-op lightweight port
    }

    void profile_clear() {
        // no-op lightweight port
    }

    // Recursion controls
    void set_recursion_limit(std::size_t limit) {
        recursion_limit_ = limit;
    }

    std::size_t recursion_limit() const {
        return recursion_limit_;
    }

    // Context cache
    template <class T>
    ContextID save_context(T context) {
        const std::uint64_t id = next_context_id_++;
        contexts_[id] = std::make_any<T>(std::move(context));
        return ContextID(id);
    }

    template <class T>
    T& context(ContextID id) {
        return std::any_cast<T&>(contexts_.at(id.id()));
    }

    template <class T>
    const T& context(ContextID id) const {
        return std::any_cast<const T&>(contexts_.at(id.id()));
    }

private:
    struct CommandEntry {
        CommandFunc func;
        ContextID context_id;
        bool has_context = false;
    };

    ftclResult normalize_top_level_result(ftclResult result) {
        if (result.has_value()) {
            return result;
        }

        Exception ex = result.error();
        if (ex.code() == ResultCodeValue(ResultCode::Return)) {
            ex.decrement_level();
        }

        if (ex.code() == ResultCodeValue(ResultCode::Okay)) {
            return ex.value();
        }

        if (ex.code() == ResultCodeValue(ResultCode::Error)) {
            return ftcl::unexpected(ex);
        }

        if (ex.code() == ResultCodeValue(ResultCode::Return)) {
            return ftcl::unexpected(ex);
        }

        if (ex.code() == ResultCodeValue(ResultCode::Break)) {
            return ftcl_err("invoked \"break\" outside of a loop");
        }

        if (ex.code() == ResultCodeValue(ResultCode::Continue)) {
            return ftcl_err("invoked \"continue\" outside of a loop");
        }

        return ftcl_err("unexpected result code.");
    }

    ftclResult eval_parsed_script(const Script& script) {
        if (eval_depth_ >= recursion_limit_) {
            return ftcl_err("too many nested calls to Interp::eval (infinite loop?)");
        }

        ++eval_depth_;
        struct DepthGuard {
            std::size_t& depth;
            ~DepthGuard() {
                --depth;
            }
        } guard{eval_depth_};

        Value result = Value::empty();

        for (const auto& cmd : script.commands()) {
            auto argv_or_err = eval_words(cmd);
            if (!argv_or_err.has_value()) {
                return ftcl::unexpected(argv_or_err.error());
            }

            auto call_result = invoke(*argv_or_err);
            if (!call_result.has_value()) {
                return call_result;
            }

            result = *call_result;
        }

        return result;
    }

    ftcl::expected<Value, Exception> eval_word(const Word& word) {
        switch (word.type()) {
            case WordType::Value:
            case WordType::String:
                return word.value();

            case WordType::VarRef: {
                auto val = var(word.value());
                if (!val.has_value()) {
                    return ftcl::unexpected(val.error());
                }
                return *val;
            }

            case WordType::ArrayRef: {
                if (!word.has_child()) {
                    return ftcl::unexpected(Exception::ftcl_err(Value("missing array index expression")));
                }

                auto index = eval_word(word.child());
                if (!index.has_value()) {
                    return ftcl::unexpected(index.error());
                }

                auto elem = element(word.name(), index->as_string());
                if (!elem.has_value()) {
                    return ftcl::unexpected(elem.error());
                }

                return *elem;
            }

            case WordType::Script: {
                auto result = eval_value(word.value());
                if (!result.has_value()) {
                    return ftcl::unexpected(result.error());
                }
                return *result;
            }

            case WordType::NumericBracket: {
                if (!word.has_child()) {
                    return ftcl::unexpected(Exception::ftcl_err(Value("missing numeric bracket expression")));
                }

                auto inner = eval_word(word.child());
                if (!inner.has_value()) {
                    return ftcl::unexpected(inner.error());
                }

                const auto is_decimal_literal_text = [](std::string_view text) {
                    if (text.empty()) {
                        return false;
                    }
                    for (char ch : text) {
                        if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
                            return false;
                        }
                    }
                    return true;
                };

                if (is_decimal_literal_text(inner->as_string())) {
                    return Value("[" + inner->as_string() + "]");
                }

                auto fallback = eval_value(word.value());
                if (!fallback.has_value()) {
                    return ftcl::unexpected(fallback.error());
                }
                return *fallback;
            }

            case WordType::Tokens: {
                std::string out;
                for (const auto& token : word.words()) {
                    auto part = eval_word(token);
                    if (!part.has_value()) {
                        return ftcl::unexpected(part.error());
                    }
                    out += part->as_string();
                }
                return Value(std::move(out));
            }

            case WordType::Expand: {
                if (!word.has_child()) {
                    return ftcl::unexpected(Exception::ftcl_err(Value("missing expansion expression")));
                }
                return eval_word(word.child());
            }
        }

        return ftcl::unexpected(Exception::ftcl_err(Value("unknown word type")));
    }

    ftcl::expected<std::vector<Value>, Exception> eval_words(const WordVec& words) {
        std::vector<Value> argv;

        for (const auto& word : words) {
            if (word.type() == WordType::Expand) {
                auto expanded = eval_word(word.child());
                if (!expanded.has_value()) {
                    return ftcl::unexpected(expanded.error());
                }

                auto list = expanded->as_list();
                if (!list.has_value()) {
                    return ftcl::unexpected(Exception::ftcl_err(Value(list.error())));
                }

                for (const auto& item : *list) {
                    argv.push_back(item);
                }
                continue;
            }

            auto value = eval_word(word);
            if (!value.has_value()) {
                return ftcl::unexpected(value.error());
            }
            argv.push_back(*value);
        }

        return argv;
    }

    ftclResult invoke(const std::vector<Value>& argv) {
        if (argv.empty()) {
            return ftcl_ok();
        }

        const std::string name = argv[0].as_string();

        auto start = std::chrono::steady_clock::now();
        ftclResult result;

        auto cit = commands_.find(name);
        if (cit != commands_.end()) {
            result = cit->second.func(this, cit->second.context_id, argv);
        } else {
            auto pit = procs_.find(name);
            if (pit == procs_.end()) {
                result = ftcl_err("invalid command name \"" + name + "\"");
            } else {
                result = invoke_proc(name, pit->second, argv);
            }
        }

        auto end = std::chrono::steady_clock::now();
        profile_[name] += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        return result;
    }

    ftclResult invoke_proc(const std::string& name, const Procedure& proc, const std::vector<Value>& argv) {
        push_scope();
        struct ScopeGuard {
            Interp* interp;
            ~ScopeGuard() {
                interp->pop_scope();
            }
        } guard{this};

        auto wrong_num_args = [&]() -> ftclResult {
            std::string msg = "wrong # args: should be \"";
            msg += name;

            for (std::size_t i = 0; i < proc.args.size(); ++i) {
                msg.push_back(' ');
                const auto& spec = proc.args[i];

                if (spec.as_string() == "args" && i + 1 == proc.args.size()) {
                    msg += "?arg ...?";
                    break;
                }

                auto parts = spec.as_list();
                if (!parts.has_value() || parts->empty()) {
                    msg += spec.as_string();
                    continue;
                }

                if (parts->size() == 1) {
                    msg += (*parts)[0].as_string();
                } else {
                    msg.push_back('?');
                    msg += (*parts)[0].as_string();
                    msg.push_back('?');
                }
            }

            msg.push_back('"');
            return ftcl_err(msg);
        };

        std::size_t argi = 1;

        for (std::size_t speci = 0; speci < proc.args.size(); ++speci) {
            const auto& spec = proc.args[speci];
            auto parts = spec.as_list();
            if (!parts.has_value() || parts->empty() || parts->size() > 2) {
                return ftcl_err("invalid proc argument specifier \"" + spec.as_string() + "\"");
            }

            const std::string arg_name = (*parts)[0].as_string();

            // "args" only has varargs meaning when it is the last formal.
            if (arg_name == "args" && speci + 1 == proc.args.size()) {
                std::vector<Value> rest;
                for (std::size_t j = argi; j < argv.size(); ++j) {
                    rest.push_back(argv[j]);
                }

                auto set_res = set_scalar("args", Value::from_list(rest));
                if (!set_res.has_value()) {
                    return ftcl::unexpected(set_res.error());
                }

                argi = argv.size();
                break;
            }

            if (argi < argv.size()) {
                auto set_res = set_scalar(arg_name, argv[argi++]);
                if (!set_res.has_value()) {
                    return ftcl::unexpected(set_res.error());
                }
                continue;
            }

            if (parts->size() == 2) {
                auto set_res = set_scalar(arg_name, (*parts)[1]);
                if (!set_res.has_value()) {
                    return ftcl::unexpected(set_res.error());
                }
                continue;
            }

            return wrong_num_args();
        }

        if (argi != argv.size()) {
            return wrong_num_args();
        }

        auto result = eval_value(proc.body);
        if (result.has_value()) {
            return result;
        }

        auto ex = result.error();
        if (ex.code() == ResultCodeValue(ResultCode::Return)) {
            ex.decrement_level();
        }

        if (ex.code() == ResultCodeValue(ResultCode::Okay)) {
            return ex.value();
        }

        if (ex.code() == ResultCodeValue(ResultCode::Error)) {
            return ftcl::unexpected(ex);
        }

        if (ex.code() == ResultCodeValue(ResultCode::Return)) {
            return ftcl::unexpected(ex);
        }

        if (ex.code() == ResultCodeValue(ResultCode::Break)) {
            return ftcl_err("invoked \"break\" outside of a loop");
        }

        if (ex.code() == ResultCodeValue(ResultCode::Continue)) {
            return ftcl_err("invoked \"continue\" outside of a loop");
        }

        return ftcl_err("unexpected result code.");
    }

    std::unordered_map<std::string, CommandEntry> commands_;
    std::unordered_map<std::string, Procedure> procs_;
    ScopeStack scopes_;

    std::unordered_map<std::string, std::chrono::nanoseconds> profile_;

    std::size_t eval_depth_ = 0;
    std::size_t recursion_limit_ = 1000;

    std::unordered_map<std::uint64_t, std::any> contexts_;
    std::uint64_t next_context_id_ = 1;
};

inline ftcl::expected<std::string, Exception> expr_resolve_array_index(Interp* interp, const std::string& raw_index) {
    std::string resolved;
    resolved.reserve(raw_index.size());

    for (std::size_t i = 0; i < raw_index.size(); ++i) {
        if (raw_index[i] != '$') {
            resolved.push_back(raw_index[i]);
            continue;
        }

        if (i + 1 >= raw_index.size()) {
            resolved.push_back('$');
            continue;
        }

        std::string name;
        if (raw_index[i + 1] == '{') {
            std::size_t j = i + 2;
            while (j < raw_index.size() && raw_index[j] != '}') {
                name.push_back(raw_index[j]);
                ++j;
            }
            if (j >= raw_index.size()) {
                return ftcl::unexpected(Exception::ftcl_err(Value("missing close brace for variable name")));
            }
            i = j;
        } else {
            std::size_t j = i + 1;
            while (j < raw_index.size()) {
                const char ch = raw_index[j];
                if (std::isalnum(static_cast<unsigned char>(ch)) == 0 && ch != '_') {
                    break;
                }
                name.push_back(ch);
                ++j;
            }
            if (name.empty()) {
                resolved.push_back('$');
                continue;
            }
            i = j - 1;
        }

        auto value = interp->var(Value(name));
        if (!value.has_value()) {
            return ftcl::unexpected(value.error());
        }
        resolved += value->as_string();
    }

    return resolved;
}

inline ftcl::expected<Value, Exception> expr_resolve_var(Interp* interp, const std::string& var_name) {
    if (interp == nullptr) {
        return ftcl::unexpected(Exception::ftcl_err(Value("missing interpreter for expression variable lookup")));
    }

    VarName parsed(var_name);
    if (!parsed.index().has_value()) {
        return interp->var(Value(parsed.name()));
    }

    auto index = expr_resolve_array_index(interp, *parsed.index());
    if (!index.has_value()) {
        return ftcl::unexpected(index.error());
    }

    return interp->element(parsed.name(), *index);
}

inline ftcl::expected<Value, Exception> expr_eval_script(Interp* interp, const std::string& script) {
    if (interp == nullptr) {
        return ftcl::unexpected(Exception::ftcl_err(Value("missing interpreter for command substitution")));
    }

    auto result = interp->eval(script);
    if (!result.has_value()) {
        const auto& ex = result.error();
        if (ex.code() == ResultCodeValue(ResultCode::Break)) {
            return ftcl::unexpected(Exception::ftcl_err(Value("invoked \"break\" outside of a loop")));
        }
        if (ex.code() == ResultCodeValue(ResultCode::Continue)) {
            return ftcl::unexpected(Exception::ftcl_err(Value("invoked \"continue\" outside of a loop")));
        }
        return ftcl::unexpected(ex);
    }

    return *result;
}

}  // namespace ftcl
