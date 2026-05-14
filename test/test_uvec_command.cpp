#include "commands.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

#define TEST(name) \
    void test_##name() { \
        std::cout << "Testing " #name "..." << std::endl;

#define ASSERT_TRUE(condition, message) \
    if (!(condition)) { \
        std::cerr << "FAILED: " << message << std::endl; \
        std::exit(1); \
    } else { \
        std::cout << "  [OK] " << message << std::endl; \
    }

#define ASSERT_EQ(actual, expected, message) \
    if ((actual) != (expected)) { \
        std::cerr << "FAILED: " << message << std::endl; \
        std::cerr << "  expected: [" << expected << "]" << std::endl; \
        std::cerr << "  actual:   [" << actual << "]" << std::endl; \
        std::exit(1); \
    } else { \
        std::cout << "  [OK] " << message << std::endl; \
    }

#define END_TEST \
    std::cout << "  [OK] Test passed!" << std::endl << std::endl; \
    }

using namespace ftcl;

static Value eval_ok(Interp& interp, const std::string& script, const std::string& label) {
    auto result = interp.eval(script);
    if (!result.has_value()) {
        std::cerr << "FAILED: " << label << std::endl;
        std::cerr << "  error: " << result.error().value().as_string() << std::endl;
        std::exit(1);
    }
    return *result;
}

TEST(uvec_command_is_registered)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "info cmdtype uvec", "uvec command type").as_string(),
              "native",
              "uvec should be installed as a native command");
END_TEST

TEST(uvec_create_get_len_to_list)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set v [uvec create {1 2 3}]; "
                      "list [uvec len $v] [uvec get $v 0] [uvec get $v 2] [uvec to_list $v]",
                      "basic uvec create/get/to_list")
                  .as_string(),
              "3 1 3 {1 2 3}",
              "uvec should expose CPU data to scripts");
END_TEST

TEST(uvec_zeroed_set_fill_and_ids)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set before [llength [uvec ids]]; "
                      "set v [uvec zeroed 4]; "
                      "uvec set $v 2 9; "
                      "set a [uvec get $v 2]; "
                      "set filled [uvec fill $v 5]; "
                      "set after [llength [uvec ids]]; "
                      "list $a $filled [expr {$after == $before + 1}] [uvec to_list $v]",
                      "zeroed set fill ids")
                  .as_string(),
              "9 4 1 {5 5 5 5}",
              "uvec set/fill should mutate the vector and ids should track handles");
END_TEST

TEST(uvec_copy_and_destroy)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set a [uvec create {1 2 3}]; "
                      "set b [uvec zeroed 3]; "
                      "set n [uvec copy $b $a]; "
                      "set copied [uvec to_list $b]; "
                      "uvec destroy $a; "
                      "set code [catch {uvec len $a} msg]; "
                      "list $n $copied $code [string first {unknown uvec handle} $msg]",
                      "copy and destroy")
                  .as_string(),
              "3 {1 2 3} 1 0",
              "uvec copy should copy data and destroy should remove the handle");
END_TEST

TEST(uvec_pointer_metadata)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set v [uvec create {7 8}]; "
                      "set p [uvec ptr $v cpu]; "
                      "list [llength $p] [lindex $p 0] [lindex $p 1] [uvec valid $v cpu]",
                      "pointer metadata")
                  .as_string(),
              "3 cpu 2 1",
              "uvec ptr should expose device, length, and address metadata");
END_TEST

TEST(uvec_error_paths)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set code [catch {uvec get 999999 0} msg]; "
                      "list $code [string first {unknown uvec handle} $msg]",
                      "unknown handle error")
                  .as_string(),
              "1 0",
              "unknown uvec handles should fail cleanly");

    ASSERT_EQ(eval_ok(interp,
                      "set code [catch {uvec create {1 abc}} msg]; "
                      "list $code [string first {expected floating-point number} $msg]",
                      "bad numeric value error")
                  .as_string(),
              "1 0",
              "uvec creation should validate numeric values");

    ASSERT_EQ(eval_ok(interp,
                      "set v [uvec create {1}]; "
                      "set code [catch {uvec get $v 9} msg]; "
                      "list $code $msg",
                      "index range error")
                  .as_string(),
              "1 {uvec index out of range}",
              "out-of-range accesses should fail cleanly");
END_TEST

TEST(uvec_cuda_bridge_or_disabled_error)
    auto interp = new_interp_with_stdlib();

#ifdef FTCL_ENABLE_CUDA
    ASSERT_EQ(eval_ok(interp,
                      "set v [uvec filled 3 4 cuda:0]; "
                      "set a [list [uvec latest $v] [uvec valid $v cpu] [uvec valid $v cuda:0]]; "
                      "set data [uvec to_list $v cuda:0]; "
                      "list $a $data [uvec latest $v]",
                      "cuda create and sync")
                  .as_string(),
              "{cuda:0 0 1} {4 4 4} cpu",
              "CUDA-backed uvec should be latest on CUDA until CPU readback");

    ASSERT_EQ(eval_ok(interp,
                      "set v [uvec filled 3 1]; "
                      "set n [uvec fill $v 8 cuda:0]; "
                      "set status [list [uvec latest $v] [uvec valid $v cpu] [uvec valid $v cuda:0]]; "
                      "list $n $status [uvec to_list $v]",
                      "cuda fill and CPU readback")
                  .as_string(),
              "3 {cuda:0 0 1} {8 8 8}",
              "uvec fill on CUDA should invalidate CPU and later synchronize back");
#else
    ASSERT_EQ(eval_ok(interp,
                      "set code [catch {uvec filled 2 1 cuda:0} msg]; "
                      "list $code [string first {CUDA backend is not enabled} $msg]",
                      "cuda disabled error")
                  .as_string(),
              "1 0",
              "CUDA uvec commands should report a clear disabled-backend error");
#endif
END_TEST

int main() {
    std::cout << "=== Testing UVec Command Semantics ===" << std::endl << std::endl;

    test_uvec_command_is_registered();
    test_uvec_create_get_len_to_list();
    test_uvec_zeroed_set_fill_and_ids();
    test_uvec_copy_and_destroy();
    test_uvec_pointer_metadata();
    test_uvec_error_paths();
    test_uvec_cuda_bridge_or_disabled_error();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}
