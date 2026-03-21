#pragma once

#include "commands.hpp"
#include "dict.hpp"
#include "eval_ptr.hpp"
#include "expr.hpp"
#include "interp.hpp"
#include "list.hpp"
#include "macros.hpp"
#include "parser.hpp"
#include "scope.hpp"
#include "test_harness.hpp"
#include "tokenizer.hpp"
#include "type.hpp"
#include "util.hpp"
#include "value.hpp"

namespace ftcl {

// This header mirrors Rust's lib.rs by exposing the primary public API.
using ::ftcl::Interp;
using ::ftcl::ftclResult;
using ::ftcl::ResultCode;
using ::ftcl::Value;

}  // namespace ftcl

