#pragma once

#include "type.hpp"

#include <sstream>

namespace ftcl {

template <class... Ts>
inline std::string str_cat(Ts&&... parts) {
    std::ostringstream oss;
    (oss << ... << std::forward<Ts>(parts));
    return oss.str();
}

}  // namespace ftcl

#define FTCL_ftcl_OK(...) ::ftcl::ftcl_ok(__VA_ARGS__)
#define FTCL_ftcl_OK_EMPTY() ::ftcl::ftcl_ok()
#define FTCL_ftcl_ERR(msg) ::ftcl::ftcl_err(msg)
#define FTCL_ftcl_ERR2(code, msg) ::ftcl::ftcl_err2(code, msg)
#define FTCL_STR_CAT(...) ::ftcl::str_cat(__VA_ARGS__)

