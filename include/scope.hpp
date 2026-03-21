#pragma once

#include "type.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ftcl {

class ScopeStack {
private:
    struct UpvarRef {
        std::size_t level;
        std::string name;
    };

    struct Var {
        enum class Kind {
            Scalar,
            Array,
            Upvar,
        };

        Kind kind = Kind::Scalar;
        Value scalar = Value::empty();
        std::unordered_map<std::string, Value> array;
        UpvarRef upvar{0, {}};

        static Var make_scalar(const Value& value) {
            Var v;
            v.kind = Kind::Scalar;
            v.scalar = value;
            return v;
        }

        static Var make_array() {
            Var v;
            v.kind = Kind::Array;
            return v;
        }

        static Var make_upvar(std::size_t level, std::string name) {
            Var v;
            v.kind = Kind::Upvar;
            v.upvar = UpvarRef{level, std::move(name)};
            return v;
        }
    };

    struct Scope {
        std::unordered_map<std::string, Var> map;
    };

public:
    ScopeStack() {
        stack_.push_back(Scope{});
    }

    std::size_t current() const {
        return stack_.empty() ? 0 : stack_.size() - 1;
    }

    std::size_t depth() const {
        return stack_.size();
    }

    void push() {
        stack_.push_back(Scope{});
    }

    void pop() {
        if (stack_.size() > 1) {
            stack_.pop_back();
        }
    }

    ftcl::expected<Value, Exception> get(const std::string& name) const {
        auto resolved = resolve_const(current(), name);
        if (!resolved.has_value()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("can't read \"" + name + "\": no such variable")));
        }

        const auto& [level, resolved_name] = *resolved;
        const auto it = stack_[level].map.find(resolved_name);
        if (it == stack_[level].map.end()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("can't read \"" + name + "\": no such variable")));
        }

        if (it->second.kind == Var::Kind::Array) {
            return ftcl::unexpected(Exception::ftcl_err(Value("can't read \"" + name + "\": variable is array")));
        }

        return it->second.scalar;
    }

    ftcl::expected<Value, Exception> get_elem(const std::string& name, const std::string& index) const {
        auto resolved = resolve_const(current(), name);
        if (!resolved.has_value()) {
            return ftcl::unexpected(Exception::ftcl_err(
                Value("can't read \"" + name + "(" + index + ")\": no such variable")));
        }

        const auto& [level, resolved_name] = *resolved;
        const auto it = stack_[level].map.find(resolved_name);
        if (it == stack_[level].map.end()) {
            return ftcl::unexpected(Exception::ftcl_err(
                Value("can't read \"" + name + "(" + index + ")\": no such variable")));
        }

        if (it->second.kind != Var::Kind::Array) {
            return ftcl::unexpected(Exception::ftcl_err(
                Value("can't read \"" + name + "(" + index + ")\": variable isn't array")));
        }

        const auto ei = it->second.array.find(index);
        if (ei == it->second.array.end()) {
            return ftcl::unexpected(Exception::ftcl_err(
                Value("can't read \"" + name + "(" + index + ")\": no such element in array")));
        }

        return ei->second;
    }

    ftcl::expected<Value, Exception> set(const std::string& name, const Value& value) {
        auto resolved = resolve_mut(current(), name);
        auto& var = stack_[resolved.first].map[resolved.second];

        if (var.kind == Var::Kind::Array) {
            return ftcl::unexpected(Exception::ftcl_err(Value("can't set \"" + name + "\": variable is array")));
        }

        var = Var::make_scalar(value);
        return value;
    }

    ftcl::expected<Value, Exception> set_elem(const std::string& name,
                                             const std::string& index,
                                             const Value& value) {
        auto resolved = resolve_mut(current(), name);
        auto& scope_map = stack_[resolved.first].map;
        auto it = scope_map.find(resolved.second);

        if (it == scope_map.end()) {
            Var created = Var::make_array();
            created.array[index] = value;
            scope_map[resolved.second] = std::move(created);
            return value;
        }

        auto& var = it->second;
        if (var.kind == Var::Kind::Scalar) {
            return ftcl::unexpected(
                Exception::ftcl_err(Value("can't set \"" + name + "(" + index + ")\": variable isn't array")));
        }

        if (var.kind != Var::Kind::Array) {
            var = Var::make_array();
        }

        var.array[index] = value;
        return value;
    }

    void unset(const std::string& name) {
        auto resolved = resolve_const(current(), name);
        if (!resolved.has_value()) {
            return;
        }
        stack_[resolved->first].map.erase(resolved->second);
    }

    void unset_elem(const std::string& name, const std::string& index) {
        auto resolved = resolve_const(current(), name);
        if (!resolved.has_value()) {
            return;
        }

        auto it = stack_[resolved->first].map.find(resolved->second);
        if (it == stack_[resolved->first].map.end()) {
            return;
        }

        if (it->second.kind == Var::Kind::Array) {
            it->second.array.erase(index);
        }
    }

    bool exists(const std::string& name) const {
        auto resolved = resolve_const(current(), name);
        if (!resolved.has_value()) {
            return false;
        }
        return stack_[resolved->first].map.find(resolved->second) != stack_[resolved->first].map.end();
    }

    bool array_exists(const std::string& name) const {
        auto resolved = resolve_const(current(), name);
        if (!resolved.has_value()) {
            return false;
        }

        const auto it = stack_[resolved->first].map.find(resolved->second);
        return it != stack_[resolved->first].map.end() && it->second.kind == Var::Kind::Array;
    }

    std::size_t array_size(const std::string& name) const {
        auto resolved = resolve_const(current(), name);
        if (!resolved.has_value()) {
            return 0;
        }

        const auto it = stack_[resolved->first].map.find(resolved->second);
        if (it == stack_[resolved->first].map.end() || it->second.kind != Var::Kind::Array) {
            return 0;
        }

        return it->second.array.size();
    }

    std::vector<Value> array_names(const std::string& name) const {
        std::vector<Value> out;

        auto resolved = resolve_const(current(), name);
        if (!resolved.has_value()) {
            return out;
        }

        const auto it = stack_[resolved->first].map.find(resolved->second);
        if (it == stack_[resolved->first].map.end() || it->second.kind != Var::Kind::Array) {
            return out;
        }

        out.reserve(it->second.array.size());
        for (const auto& [key, _] : it->second.array) {
            out.emplace_back(key);
        }
        return out;
    }

    std::vector<Value> array_get(const std::string& name) const {
        std::vector<Value> out;

        auto resolved = resolve_const(current(), name);
        if (!resolved.has_value()) {
            return out;
        }

        const auto it = stack_[resolved->first].map.find(resolved->second);
        if (it == stack_[resolved->first].map.end() || it->second.kind != Var::Kind::Array) {
            return out;
        }

        out.reserve(it->second.array.size() * 2);
        for (const auto& [key, value] : it->second.array) {
            out.emplace_back(key);
            out.emplace_back(value);
        }
        return out;
    }

    ftcl::expected<Value, Exception> array_set(const std::string& name, const std::vector<Value>& list) {
        if (list.size() % 2 != 0) {
            return ftcl::unexpected(Exception::ftcl_err(Value("list must have an even number of elements")));
        }

        auto resolved = resolve_mut(current(), name);
        auto& scope_map = stack_[resolved.first].map;
        auto it = scope_map.find(resolved.second);

        if (it == scope_map.end()) {
            Var created = Var::make_array();
            for (std::size_t i = 0; i + 1 < list.size(); i += 2) {
                created.array[list[i].as_string()] = list[i + 1];
            }
            scope_map[resolved.second] = std::move(created);
            return Value::empty();
        }

        auto& var = it->second;
        if (var.kind == Var::Kind::Scalar) {
            return ftcl::unexpected(Exception::ftcl_err(Value("can't set \"" + name + "\": variable isn't array")));
        }

        if (var.kind != Var::Kind::Array) {
            var = Var::make_array();
        }

        for (std::size_t i = 0; i + 1 < list.size(); i += 2) {
            var.array[list[i].as_string()] = list[i + 1];
        }

        return Value::empty();
    }

    void array_unset(const std::string& name) {
        auto resolved = resolve_const(current(), name);
        if (!resolved.has_value()) {
            return;
        }

        auto it = stack_[resolved->first].map.find(resolved->second);
        if (it == stack_[resolved->first].map.end()) {
            return;
        }

        if (it->second.kind == Var::Kind::Array) {
            stack_[resolved->first].map.erase(it);
        }
    }

    void upvar(std::size_t level, const std::string& name, const std::string& alias = std::string()) {
        const std::string local_name = alias.empty() ? name : alias;
        stack_[current()].map[local_name] = Var::make_upvar(level, name);
    }

    std::vector<Value> vars_in_scope(std::size_t level) const {
        std::vector<Value> out;
        if (level >= stack_.size()) {
            return out;
        }

        out.reserve(stack_[level].map.size());
        for (const auto& [name, _] : stack_[level].map) {
            out.emplace_back(name);
        }
        return out;
    }

    std::vector<Value> local_vars_in_scope(std::size_t level) const {
        std::vector<Value> out;
        if (level >= stack_.size()) {
            return out;
        }

        out.reserve(stack_[level].map.size());
        for (const auto& [name, var] : stack_[level].map) {
            if (var.kind == Var::Kind::Upvar) {
                continue;
            }
            out.emplace_back(name);
        }
        return out;
    }

private:
    std::optional<std::pair<std::size_t, std::string>> resolve_const(std::size_t level,
                                                                     const std::string& name) const {
        if (level >= stack_.size()) {
            return std::nullopt;
        }

        auto it = stack_[level].map.find(name);
        if (it == stack_[level].map.end()) {
            return std::make_pair(level, name);
        }

        const Var& var = it->second;
        if (var.kind == Var::Kind::Upvar) {
            return resolve_const(var.upvar.level, var.upvar.name);
        }

        return std::make_pair(level, name);
    }

    std::pair<std::size_t, std::string> resolve_mut(std::size_t level, const std::string& name) {
        if (level >= stack_.size()) {
            return {current(), name};
        }

        auto it = stack_[level].map.find(name);
        if (it == stack_[level].map.end()) {
            return {level, name};
        }

        Var& var = it->second;
        if (var.kind == Var::Kind::Upvar) {
            return resolve_mut(var.upvar.level, var.upvar.name);
        }

        return {level, name};
    }

    std::vector<Scope> stack_;
};

}  // namespace ftcl

