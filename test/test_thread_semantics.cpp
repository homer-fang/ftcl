#include "commands.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

#define TEST(name) \
    void test_##name() { \
        std::cout << "Testing " #name "..." << std::endl;

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

TEST(thread_spawn_and_await)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp, "set id [thread spawn {expr {21 * 2}}]; thread await $id", "thread await value").as_string(),
              "42",
              "thread await should return worker result");
END_TEST

TEST(thread_ready_and_ids)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set a [thread spawn {expr {1}}]; "
                      "set b [thread spawn {expr {2}}]; "
                      "set ids [thread ids]; "
                      "set ready [thread ready $a]; "
                      "set hasA [expr {$a in $ids}]; "
                      "set hasB [expr {$b in $ids}]; "
                      "set va [thread await $a]; "
                      "set vb [thread await $b]; "
                      "list [expr {$ready == 0 || $ready == 1}] $hasA $hasB $va $vb",
                      "thread ready and ids")
                  .as_string(),
              "1 1 1 1 2",
              "thread ready/ids should expose active tasks");
END_TEST

TEST(thread_error_propagation_and_missing_handle)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set id [thread spawn {expr {1/0}}]; "
                      "catch {thread await $id} msg; "
                      "set msg",
                      "thread await error propagation")
                  .as_string(),
              "divide by zero",
              "thread await should propagate worker script errors");

    ASSERT_EQ(eval_ok(interp,
                      "set id [thread spawn {expr {1+1}}]; "
                      "thread await $id; "
                      "set code [catch {thread await $id} msg]; "
                      "list $code [string first {unknown thread task \"} $msg]",
                      "thread await unknown handle")
                  .as_string(),
              "1 0",
              "awaiting a completed handle twice should fail");
END_TEST

TEST(thread_channel_send_recv_across_threads)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set ch [thread channel create]; "
                      "set tid [thread spawn [list thread channel recv $ch]]; "
                      "thread channel send $ch hello; "
                      "thread await $tid",
                      "thread channel recv in worker")
                  .as_string(),
              "hello",
              "thread channel should deliver values to worker threads");

    ASSERT_EQ(eval_ok(interp,
                      "set ch [thread channel create]; "
                      "set tid [thread spawn [list thread channel send $ch world]]; "
                      "set v [thread channel recv $ch]; "
                      "thread await $tid; "
                      "set v",
                      "thread channel send in worker")
                  .as_string(),
              "world",
              "thread channel should deliver values to main thread");
END_TEST

TEST(thread_channel_fifo_and_errors)
    auto interp = new_interp_with_stdlib();

    ASSERT_EQ(eval_ok(interp,
                      "set ch [thread channel create]; "
                      "thread channel send $ch one; "
                      "thread channel send $ch two; "
                      "list [thread channel recv $ch] [thread channel recv $ch]",
                      "thread channel fifo order")
                  .as_string(),
              "one two",
              "thread channel should preserve FIFO ordering");

    ASSERT_EQ(eval_ok(interp,
                      "set code [catch {thread channel recv 999999} msg]; "
                      "list $code [string first {unknown thread channel \"} $msg]",
                      "thread channel unknown id")
                  .as_string(),
              "1 0",
              "thread channel recv should fail for unknown channel ids");
END_TEST

int main() {
    std::cout << "=== Testing Thread Semantics ===" << std::endl << std::endl;

    test_thread_spawn_and_await();
    test_thread_ready_and_ids();
    test_thread_error_propagation_and_missing_handle();
    test_thread_channel_send_recv_across_threads();
    test_thread_channel_fifo_and_errors();

    std::cout << "=== All tests passed! ===" << std::endl;
    return 0;
}
