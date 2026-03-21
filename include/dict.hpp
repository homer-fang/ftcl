#pragma once

#include "type.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ftcl {

inline ftclDict dict_new() {
    return ftclDict{};
}

inline ftclDict list_to_dict(const std::vector<Value>& list) {
    ftclDict dict;

    for (std::size_t i = 0; i + 1 < list.size(); i += 2) {
        dict[list[i].as_string()] = list[i + 1];
    }

    return dict;
}

inline Value dict_to_string(const ftclDict& dict) {
    return Value::from_dict(dict);
}

inline ftcl::expected<Value, std::string> dict_path_get(const Value& root, const std::vector<Value>& keys) {
    auto dict = root.as_dict();
    if (!dict.has_value()) {
        return ftcl::unexpected(dict.error());
    }

    if (keys.empty()) {
        return root;
    }

    Value current = root;

    for (const auto& key : keys) {
        auto d = current.as_dict();
        if (!d.has_value()) {
            return ftcl::unexpected("missing value to go with key");
        }

        const auto it = d->find(key.as_string());
        if (it == d->end()) {
            return ftcl::unexpected("key \"" + key.as_string() + "\" not known in dictionary");
        }

        current = it->second;
    }

    return current;
}

inline ftcl::expected<Value, std::string> dict_path_insert(const Value& root,
                                                          const std::vector<Value>& keys,
                                                          const Value& value) {
    if (keys.empty()) {
        return value;
    }

    auto dict = root.as_dict();
    if (!dict.has_value()) {
        return ftcl::unexpected(dict.error());
    }

    ftclDict out = *dict;

    if (keys.size() == 1) {
        out[keys[0].as_string()] = value;
        return Value::from_dict(out);
    }

    const std::string head = keys[0].as_string();

    Value child = Value::from_dict({});
    auto it = out.find(head);
    if (it != out.end()) {
        child = it->second;
    }

    std::vector<Value> rest;
    rest.reserve(keys.size() - 1);
    for (std::size_t i = 1; i < keys.size(); ++i) {
        rest.push_back(keys[i]);
    }

    auto inserted = dict_path_insert(child, rest, value);
    if (!inserted.has_value()) {
        return ftcl::unexpected(inserted.error());
    }

    out[head] = *inserted;
    return Value::from_dict(out);
}

inline ftcl::expected<Value, std::string> dict_path_remove(const Value& root, const std::vector<Value>& keys) {
    auto dict = root.as_dict();
    if (!dict.has_value()) {
        return ftcl::unexpected(dict.error());
    }

    ftclDict out = *dict;

    if (keys.empty()) {
        return Value::from_dict(out);
    }

    if (keys.size() == 1) {
        out.erase(keys[0].as_string());
        return Value::from_dict(out);
    }

    const std::string head = keys[0].as_string();
    auto it = out.find(head);
    if (it == out.end()) {
        return ftcl::unexpected("key \"" + head + "\" not known in dictionary");
    }

    std::vector<Value> rest;
    rest.reserve(keys.size() - 1);
    for (std::size_t i = 1; i < keys.size(); ++i) {
        rest.push_back(keys[i]);
    }

    auto child_removed = dict_path_remove(it->second, rest);
    if (!child_removed.has_value()) {
        return ftcl::unexpected(child_removed.error());
    }

    it->second = *child_removed;
    return Value::from_dict(out);
}

}  // namespace ftcl

