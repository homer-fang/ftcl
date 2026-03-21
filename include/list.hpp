#pragma once

#include "value.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ftcl {

inline std::string list_to_string(const std::vector<Value>& list) {
    return Value::from_list(list).as_string();
}

inline ftcl::expected<std::vector<Value>, std::string> get_list(const Value& value) {
    return value.as_list();
}

inline Value list_index(const Value& list_value, ftclInt index) {
    auto list = list_value.as_list();
    if (!list.has_value()) {
        return Value::empty();
    }

    if (index < 0 || static_cast<std::size_t>(index) >= list->size()) {
        return Value::empty();
    }

    return (*list)[static_cast<std::size_t>(index)];
}

inline ftcl::expected<Value, std::string> list_range(const Value& list_value, ftclInt first, ftclInt last) {
    auto list = list_value.as_list();
    if (!list.has_value()) {
        return ftcl::unexpected(list.error());
    }

    const auto& vec = *list;

    if (vec.empty()) {
        return Value::from_list({});
    }

    ftclInt lo = std::max<ftclInt>(0, first);
    ftclInt hi = std::max<ftclInt>(0, last);

    if (lo > hi || lo >= static_cast<ftclInt>(vec.size())) {
        return Value::from_list({});
    }

    if (hi >= static_cast<ftclInt>(vec.size())) {
        hi = static_cast<ftclInt>(vec.size()) - 1;
    }

    std::vector<Value> out;
    out.reserve(static_cast<std::size_t>(hi - lo + 1));

    for (ftclInt i = lo; i <= hi; ++i) {
        out.push_back(vec[static_cast<std::size_t>(i)]);
    }

    return Value::from_list(out);
}

}  // namespace ftcl

