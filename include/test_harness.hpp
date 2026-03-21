#pragma once

#include "commands.hpp"
#include "interp.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace ftcl {

struct TestContext {
    std::size_t num_tests = 0;
    std::size_t num_passed = 0;
    std::size_t num_failed = 0;
    std::size_t num_errors = 0;
};

enum class TestCode {
    Ok,
    Error,
};

struct TestInfo {
    std::string name;
    std::string description;
    std::string setup;
    std::string body;
    std::string cleanup;
    TestCode code = TestCode::Ok;
    std::string expect;

    void print_failure(const std::string& got_code, const std::string& received) const {
        std::cout << "\n*** FAILED " << name << " " << description << "\n";
        std::cout << "Expected " << (code == TestCode::Ok ? "-ok" : "-error") << " <" << expect << ">\n";
        std::cout << "Received " << got_code << " <" << received << ">\n";
    }

    void print_error(const ftclResult& result) const {
        std::cout << "\n*** ERROR " << name << " " << description << "\n";
        std::cout << "Expected " << (code == TestCode::Ok ? "-ok" : "-error") << " <" << expect << ">\n";

        if (result.has_value()) {
            std::cout << "Received -ok <" << result->as_string() << ">\n";
            return;
        }

        const auto& ex = result.error();
        if (ex.code() == ResultCodeValue(ResultCode::Error)) {
            std::cout << "Received -error <" << ex.value().as_string() << ">\n";
        } else if (ex.code() == ResultCodeValue(ResultCode::Return)) {
            std::cout << "Received -return <" << ex.value().as_string() << ">\n";
        } else if (ex.code() == ResultCodeValue(ResultCode::Break)) {
            std::cout << "Received -break <>\n";
        } else if (ex.code() == ResultCodeValue(ResultCode::Continue)) {
            std::cout << "Received -continue <>\n";
        } else {
            std::cout << "Received -" << ex.code().as_int() << " <" << ex.value().as_string() << ">\n";
        }
    }

    void print_helper_error(const std::string& part, const std::string& msg) const {
        std::cout << "\n*** ERROR (in " << part << ") " << name << " " << description << "\n";
        std::cout << "    " << msg << "\n";
    }
};

inline void incr_errors(Interp* interp, ContextID context_id) {
    interp->context<TestContext>(context_id).num_errors += 1;
}

inline void run_test(Interp* interp, ContextID context_id, const TestInfo& info) {
    interp->push_scope();

    // Setup
    if (!info.setup.empty()) {
        auto setup_result = interp->eval(info.setup);
        if (!setup_result.has_value() && setup_result.error().code() == ResultCodeValue(ResultCode::Error)) {
            info.print_helper_error("-setup", setup_result.error().value().as_string());
        }
    }

    // Body
    ftclResult body_result = interp->eval_value(Value(info.body));

    // Cleanup
    if (!info.cleanup.empty()) {
        auto cleanup_result = interp->eval(info.cleanup);
        if (!cleanup_result.has_value() && cleanup_result.error().code() == ResultCodeValue(ResultCode::Error)) {
            info.print_helper_error("-cleanup", cleanup_result.error().value().as_string());
        }
    }

    interp->pop_scope();

    auto& ctx = interp->context<TestContext>(context_id);
    ctx.num_tests += 1;

    if (body_result.has_value()) {
        if (info.code == TestCode::Ok) {
            if (body_result->as_string() == info.expect) {
                ctx.num_passed += 1;
            } else {
                ctx.num_failed += 1;
                info.print_failure("-ok", body_result->as_string());
            }
            return;
        }
    } else {
        if (info.code == TestCode::Error) {
            const std::string got = body_result.error().value().as_string();
            if (got == info.expect) {
                ctx.num_passed += 1;
            } else {
                ctx.num_failed += 1;
                info.print_failure("-error", got);
            }
            return;
        }
    }

    ctx.num_errors += 1;
    info.print_error(body_result);
}

inline ftclResult simple_test(Interp* interp, ContextID context_id, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 6, 6, "name description script -ok|-error result");
    if (!chk.has_value()) {
        return chk;
    }

    TestInfo info;
    info.name = argv[1].as_string();
    info.description = argv[2].as_string();
    info.body = argv[3].as_string();
    info.expect = argv[5].as_string();

    const std::string code = argv[4].as_string();
    if (code == "-ok") {
        info.code = TestCode::Ok;
    } else if (code == "-error") {
        info.code = TestCode::Error;
    } else {
        incr_errors(interp, context_id);
        info.print_helper_error("test command", "invalid option: \"" + code + "\"");
        return ftcl_ok();
    }

    run_test(interp, context_id, info);
    return ftcl_ok();
}

inline ftclResult fancy_test(Interp* interp, ContextID context_id, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 4, 0, "name description option value ?option value...?");
    if (!chk.has_value()) {
        return chk;
    }

    TestInfo info;
    info.name = argv[1].as_string();
    info.description = argv[2].as_string();

    for (std::size_t i = 3; i < argv.size(); i += 2) {
        const std::string opt = argv[i].as_string();
        if (i + 1 >= argv.size()) {
            incr_errors(interp, context_id);
            info.print_helper_error("test command", "missing value for " + opt);
            return ftcl_ok();
        }

        const std::string val = argv[i + 1].as_string();
        if (opt == "-setup") {
            info.setup = val;
        } else if (opt == "-body") {
            info.body = val;
        } else if (opt == "-cleanup") {
            info.cleanup = val;
        } else if (opt == "-ok") {
            info.code = TestCode::Ok;
            info.expect = val;
        } else if (opt == "-error") {
            info.code = TestCode::Error;
            info.expect = val;
        } else {
            incr_errors(interp, context_id);
            info.print_helper_error("test command", "invalid option: \"" + opt + "\"");
            return ftcl_ok();
        }
    }

    run_test(interp, context_id, info);
    return ftcl_ok();
}

inline ftclResult test_cmd(Interp* interp, ContextID context_id, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 4, 0, "name description args...");
    if (!chk.has_value()) {
        return chk;
    }

    const std::string tag = argv[3].as_string();
    if (!tag.empty() && tag[0] == '-') {
        return fancy_test(interp, context_id, argv);
    }

    return simple_test(interp, context_id, argv);
}

inline bool test_harness(Interp& interp, const std::vector<std::string>& args) {
    std::cout << "ftcl C++ -- Test Harness" << std::endl;

    if (args.empty()) {
        std::cerr << "missing test script" << std::endl;
        return false;
    }

    const std::filesystem::path script_path(args[0]);
    std::ifstream in(script_path, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "couldn't read test script: " << args[0] << std::endl;
        return false;
    }

    std::string script((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    std::error_code ec;
    const auto old_cwd = std::filesystem::current_path(ec);
    const bool had_old_cwd = !ec;

    if (script_path.has_parent_path()) {
        std::filesystem::current_path(script_path.parent_path(), ec);
    }

    ContextID context_id = interp.save_context(TestContext{});
    interp.add_context_command("test", test_cmd, context_id);

    ftclResult run = interp.eval(script);

    if (had_old_cwd) {
        std::filesystem::current_path(old_cwd, ec);
    }

    if (!run.has_value()) {
        std::cerr << run.error().value().as_string() << std::endl;
        return false;
    }

    const auto& ctx = interp.context<TestContext>(context_id);
    std::cout << "\n" << ctx.num_tests << " tests, " << ctx.num_passed << " passed, " << ctx.num_failed << " failed, "
              << ctx.num_errors << " errors" << std::endl;

    return ctx.num_failed + ctx.num_errors == 0;
}

}  // namespace ftcl
