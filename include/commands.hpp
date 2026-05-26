#pragma once

#include "dict.hpp"
#include "geometry.hpp"
#ifdef FTCL_GEOMETRY_CUDA
#include "geometry_cuda.hpp"
#endif
#include "interp.hpp"
#include "list.hpp"
#include "macros.hpp"
#include "util.hpp"
#include "uvec.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cmath>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <sstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <conio.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace ftcl {

inline Interp new_interp_with_stdlib();

inline std::string command_name(const std::vector<Value>& argv, std::size_t namec) {
    std::string out;
    for (std::size_t i = 0; i < namec && i < argv.size(); ++i) {
        if (i > 0) {
            out.push_back(' ');
        }
        out += argv[i].as_string();
    }
    return out;
}

inline ftclResult check_args(std::size_t namec,
                             const std::vector<Value>& argv,
                             std::size_t min,
                             std::size_t max,
                             const std::string& argsig) {
    if (argv.size() < min || (max > 0 && argv.size() > max)) {
        return ftcl_err("wrong # args: should be \"" + command_name(argv, namec) + " " + argsig + "\"");
    }
    return ftcl_ok();
}

// -----------------------------------------------------------------------------
// Core commands

inline ftclResult cmd_append(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 0, "varName ?value value ...?");
    if (!chk.has_value()) {
        return chk;
    }

    std::string new_string;
    auto old = interp->var(argv[1]);
    if (old.has_value()) {
        new_string = old->as_string();
    }

    for (std::size_t i = 2; i < argv.size(); ++i) {
        new_string += argv[i].as_string();
    }

    auto set = interp->set_var_return(argv[1], Value(new_string));
    if (!set.has_value()) {
        return ftcl::unexpected(set.error());
    }
    return *set;
}

inline ftclResult cmd_assert_eq(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 3, 3, "received expected");
    if (!chk.has_value()) {
        return chk;
    }

    if (argv[1] == argv[2]) {
        return ftcl_ok();
    }

    return ftcl_err("assertion failed: received \"" + argv[1].as_string() + "\", expected \"" + argv[2].as_string() + "\".");
}

inline ftclResult cmd_break(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 1, 1, "");
    if (!chk.has_value()) {
        return chk;
    }
    return ftcl::unexpected(Exception::ftcl_break());
}

inline ftclResult cmd_continue(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 1, 1, "");
    if (!chk.has_value()) {
        return chk;
    }
    return ftcl::unexpected(Exception::ftcl_continue());
}

inline ftclResult cmd_error(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 2, "message");
    if (!chk.has_value()) {
        return chk;
    }
    return ftcl_err(argv[1]);
}

inline ftclResult cmd_exit(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 1, 2, "?returnCode?");
    if (!chk.has_value()) {
        return chk;
    }

    ftclInt code = 0;
    if (argv.size() == 2) {
        auto parsed = parse_int(argv[1]);
        if (!parsed.has_value()) {
            return ftcl_err(parsed.error());
        }
        code = *parsed;
    }

    // Avoid terminating host process in embedded mode.
    return ftcl::unexpected(Exception(ResultCodeValue(code), Value::empty(), 0, ResultCodeValue(code), std::nullopt));
}

inline ftclResult cmd_expr(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 2, "expr");
    if (!chk.has_value()) {
        return chk;
    }

    return interp->expr(argv[1]);
}

inline ftclResult cmd_set(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 3, "varName ?newValue?");
    if (!chk.has_value()) {
        return chk;
    }

    if (argv.size() == 3) {
        auto set = interp->set_var_return(argv[1], argv[2]);
        if (!set.has_value()) {
            return ftcl::unexpected(set.error());
        }
        return *set;
    }

    auto get = interp->var(argv[1]);
    if (!get.has_value()) {
        return ftcl::unexpected(get.error());
    }
    return *get;
}

inline ftclResult cmd_unset(Interp* interp, ContextID, const std::vector<Value>& argv) {
    bool nocomplain = false;
    std::size_t i = 1;

    while (i < argv.size()) {
        const std::string opt = argv[i].as_string();
        if (opt == "--") {
            ++i;
            break;
        }
        if (opt == "-nocomplain" || opt == "-ncomplain") {
            nocomplain = true;
            ++i;
            continue;
        }
        if (!opt.empty() && opt[0] == '-') {
            return ftcl_err("bad option \"" + opt + "\": must be -nocomplain or --");
        }
        break;
    }

    (void)nocomplain;

    for (; i < argv.size(); ++i) {
        interp->unset_var(argv[i]);
    }

    return ftcl_ok();
}

inline ftclResult cmd_puts(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 2, "string");
    if (!chk.has_value()) {
        return chk;
    }

    std::cout << argv[1].as_string() << std::endl;
    return ftcl_ok();
}

inline ftclResult cmd_gets(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 3, "channelId ?varName?");
    if (!chk.has_value()) {
        return chk;
    }

    const std::string channel = argv[1].as_string();
    if (channel != "stdin") {
        return ftcl_err("can not find channel named \"" + channel + "\"");
    }

    std::string line;
    const bool ok = static_cast<bool>(std::getline(std::cin, line));

    if (argv.size() == 3) {
        auto set = interp->set_var(argv[2], Value(ok ? line : std::string()));
        if (!set.has_value()) {
            return ftcl::unexpected(set.error());
        }
        if (!ok) {
            return ftcl_ok(static_cast<ftclInt>(-1));
        }
        return ftcl_ok(static_cast<ftclInt>(line.size()));
    }

    if (!ok) {
        return ftcl_ok(static_cast<ftclInt>(-1));
    }
    return ftcl_ok(line);
}

class ScopedStdinRawMode {
public:
    ScopedStdinRawMode() {
#if defined(_WIN32)
        active_ = false;
#else
        if (::isatty(STDIN_FILENO) == 0) {
            return;
        }

        if (::tcgetattr(STDIN_FILENO, &saved_) != 0) {
            return;
        }

        termios raw = saved_;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;

        if (::tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) {
            active_ = true;
        }
#endif
    }

    ~ScopedStdinRawMode() {
#if defined(_WIN32)
        (void)active_;
#else
        if (active_) {
            (void)::tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
        }
#endif
    }

private:
    bool active_ = false;
#if !defined(_WIN32)
    termios saved_{};
#endif
};

enum class ReadCharStatus {
    GotChar,
    WouldBlock,
    Eof,
};

inline ReadCharStatus read_one_char(char& ch, bool non_blocking) {
#if defined(_WIN32)
    if (non_blocking && ::_kbhit() == 0) {
        return ReadCharStatus::WouldBlock;
    }

    const int code = ::_getch();
    if (code == EOF) {
        return ReadCharStatus::Eof;
    }
    ch = static_cast<char>(code);
    return ReadCharStatus::GotChar;
#else
    ScopedStdinRawMode raw_mode;

    if (non_blocking) {
        const int old_flags = ::fcntl(STDIN_FILENO, F_GETFL, 0);
        const bool can_restore = old_flags != -1;
        if (can_restore) {
            (void)::fcntl(STDIN_FILENO, F_SETFL, old_flags | O_NONBLOCK);
        }

        unsigned char c = 0;
        const ssize_t n = ::read(STDIN_FILENO, &c, 1);

        if (can_restore) {
            (void)::fcntl(STDIN_FILENO, F_SETFL, old_flags);
        }

        if (n == 1) {
            ch = static_cast<char>(c);
            return ReadCharStatus::GotChar;
        }
        if (n == 0) {
            return ReadCharStatus::Eof;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ReadCharStatus::WouldBlock;
        }
        return ReadCharStatus::Eof;
    }

    const int code = std::cin.get();
    if (code == EOF) {
        return ReadCharStatus::Eof;
    }
    ch = static_cast<char>(code);
    return ReadCharStatus::GotChar;
#endif
}

inline ftclResult cmd_getch(Interp* interp, ContextID, const std::vector<Value>& argv) {
    bool non_blocking = false;
    std::size_t idx = 1;

    if (idx < argv.size() && argv[idx].as_string() == "-noblock") {
        non_blocking = true;
        ++idx;
    }

    if (idx < argv.size() && argv[idx].as_string() == "--") {
        ++idx;
    }

    if (argv.size() - idx > 1) {
        return ftcl_err("wrong # args: should be \"getch ?-noblock? ?varName?\"");
    }

    char ch = '\0';
    const ReadCharStatus status = read_one_char(ch, non_blocking);
    const bool has_char = status == ReadCharStatus::GotChar;
    const std::string one = has_char ? std::string(1, ch) : std::string();
    const bool has_var = idx < argv.size();

    if (has_var) {
        auto set = interp->set_var(argv[idx], Value(one));
        if (!set.has_value()) {
            return ftcl::unexpected(set.error());
        }

        if (status == ReadCharStatus::WouldBlock) {
            return ftcl_ok(static_cast<ftclInt>(0));
        }
        if (status == ReadCharStatus::Eof) {
            return ftcl_ok(static_cast<ftclInt>(-1));
        }
        return ftcl_ok(static_cast<ftclInt>(1));
    }

    if (status != ReadCharStatus::GotChar) {
        return ftcl_ok(Value::empty());
    }

    return ftcl_ok(one);
}

inline ftclResult cmd_rename(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 3, 3, "oldName newName");
    if (!chk.has_value()) {
        return chk;
    }

    const std::string old_name = argv[1].as_string();
    const std::string new_name = argv[2].as_string();

    if (!interp->has_command(old_name)) {
        return ftcl_err("can't rename \"" + old_name + "\": command doesn't exist");
    }

    if (new_name.empty()) {
        interp->remove_command(old_name);
    } else {
        interp->rename_command(old_name, new_name);
    }

    return ftcl_ok();
}

inline ftclResult cmd_source(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 2, "filename");
    if (!chk.has_value()) {
        return chk;
    }

    std::ifstream in(argv[1].as_string(), std::ios::binary);
    if (!in.is_open()) {
        std::string reason = std::strerror(errno);
        return ftcl_err("couldn't read file \"" + argv[1].as_string() + "\": " + reason);
    }

    std::string script((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return interp->eval(script);
}

inline ftclResult cmd_time(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 3, "command ?count?");
    if (!chk.has_value()) {
        return chk;
    }

    ftclInt count = 1;
    if (argv.size() == 3) {
        auto i = parse_int(argv[2]);
        if (!i.has_value()) {
            return ftcl_err(i.error());
        }
        count = *i;
    }

    auto start = std::chrono::steady_clock::now();
    if (count > 0) {
        for (ftclInt i = 0; i < count; ++i) {
            auto result = interp->eval_value(argv[1]);
            if (!result.has_value()) {
                return result;
            }
        }
    }
    auto end = std::chrono::steady_clock::now();

    const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    const ftclInt avg_ns = count > 0 ? static_cast<ftclInt>(elapsed_ns / static_cast<long long>(count)) : 0;
    return ftcl_ok(std::to_string(avg_ns) + " nanoseconds per iteration");
}

inline ftclResult cmd_sleep(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 2, "milliseconds");
    if (!chk.has_value()) {
        return chk;
    }

    auto parsed = parse_int(argv[1]);
    if (!parsed.has_value()) {
        return ftcl_err(parsed.error());
    }
    if (*parsed < 0) {
        return ftcl_err("expected non-negative integer but got \"" + argv[1].as_string() + "\"");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(*parsed));
    return ftcl_ok();
}

class ThreadTaskManager {
public:
    ftclInt spawn(std::string script) {
        const ftclInt id = next_id_.fetch_add(1);
        auto future = std::async(std::launch::async, [script = std::move(script)]() -> ftclResult {
            try {
                auto worker = new_interp_with_stdlib();
                return worker.eval(script);
            } catch (const std::exception& ex) {
                return ftcl_err("thread worker exception: " + std::string(ex.what()));
            } catch (...) {
                return ftcl_err("thread worker exception: unknown");
            }
        });

        std::lock_guard<std::mutex> lock(mu_);
        tasks_.emplace(id, std::move(future));
        return id;
    }

    ftcl::expected<bool, Exception> ready(ftclInt id) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = tasks_.find(id);
        if (it == tasks_.end()) {
            return ftcl::unexpected(Exception::ftcl_err(Value("unknown thread task \"" + std::to_string(id) + "\"")));
        }

        return it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    ftcl::expected<ftclResult, Exception> await(ftclInt id) {
        std::future<ftclResult> future;
        {
            std::lock_guard<std::mutex> lock(mu_);
            auto it = tasks_.find(id);
            if (it == tasks_.end()) {
                return ftcl::unexpected(Exception::ftcl_err(Value("unknown thread task \"" + std::to_string(id) + "\"")));
            }
            future = std::move(it->second);
            tasks_.erase(it);
        }

        try {
            return future.get();
        } catch (const std::exception& ex) {
            return ftcl::unexpected(Exception::ftcl_err(Value("thread await failed: " + std::string(ex.what()))));
        } catch (...) {
            return ftcl::unexpected(Exception::ftcl_err(Value("thread await failed: unknown exception")));
        }
    }

    std::vector<ftclInt> ids() {
        std::lock_guard<std::mutex> lock(mu_);
        std::vector<ftclInt> out;
        out.reserve(tasks_.size());
        for (const auto& [id, _] : tasks_) {
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

private:
    std::mutex mu_;
    std::unordered_map<ftclInt, std::future<ftclResult>> tasks_;
    std::atomic<ftclInt> next_id_{1};
};

inline ThreadTaskManager& thread_task_manager() {
    static ThreadTaskManager manager;
    return manager;
}

class ThreadChannelManager {
private:
    struct Channel {
        std::mutex mu;
        std::condition_variable cv;
        std::deque<std::string> queue;
    };

public:
    ftclInt create() {
        const ftclInt id = next_id_.fetch_add(1);
        auto channel = std::make_shared<Channel>();

        std::lock_guard<std::mutex> lock(mu_);
        channels_.emplace(id, std::move(channel));
        return id;
    }

    ftcl::expected<std::shared_ptr<Channel>, Exception> get(ftclInt id) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = channels_.find(id);
        if (it == channels_.end()) {
            return ftcl::unexpected(
                Exception::ftcl_err(Value("unknown thread channel \"" + std::to_string(id) + "\"")));
        }
        return it->second;
    }

    ftclResult send(ftclInt id, std::string payload) {
        auto channel = get(id);
        if (!channel.has_value()) {
            return ftcl::unexpected(channel.error());
        }

        {
            std::lock_guard<std::mutex> lock((*channel)->mu);
            (*channel)->queue.push_back(std::move(payload));
        }
        (*channel)->cv.notify_one();
        return ftcl_ok();
    }

    ftcl::expected<std::string, Exception> recv(ftclInt id) {
        auto channel = get(id);
        if (!channel.has_value()) {
            return ftcl::unexpected(channel.error());
        }

        std::unique_lock<std::mutex> lock((*channel)->mu);
        while ((*channel)->queue.empty()) {
            (*channel)->cv.wait(lock);
        }

        std::string value = std::move((*channel)->queue.front());
        (*channel)->queue.pop_front();
        return value;
    }

    ftcl::expected<std::optional<std::string>, Exception> try_recv(ftclInt id) {
        auto channel = get(id);
        if (!channel.has_value()) {
            return ftcl::unexpected(channel.error());
        }

        std::lock_guard<std::mutex> lock((*channel)->mu);
        if ((*channel)->queue.empty()) {
            return std::optional<std::string>{};
        }

        std::string value = std::move((*channel)->queue.front());
        (*channel)->queue.pop_front();
        return std::optional<std::string>(std::move(value));
    }

    std::mutex mu_;
    std::unordered_map<ftclInt, std::shared_ptr<Channel>> channels_;
    std::atomic<ftclInt> next_id_{1};
};

inline ThreadChannelManager& thread_channel_manager() {
    static ThreadChannelManager manager;
    return manager;
}

inline ftcl::expected<ftclInt, Exception> parse_thread_task_id(const Value& v) {
    auto parsed = parse_int(v);
    if (!parsed.has_value()) {
        return ftcl::unexpected(Exception::ftcl_err(Value(parsed.error())));
    }
    if (*parsed <= 0) {
        return ftcl::unexpected(Exception::ftcl_err(Value("invalid thread task id \"" + v.as_string() + "\"")));
    }
    return *parsed;
}

inline ftcl::expected<ftclInt, Exception> parse_thread_channel_id(const Value& v) {
    auto parsed = parse_int(v);
    if (!parsed.has_value()) {
        return ftcl::unexpected(Exception::ftcl_err(Value(parsed.error())));
    }
    if (*parsed <= 0) {
        return ftcl::unexpected(Exception::ftcl_err(Value("invalid thread channel id \"" + v.as_string() + "\"")));
    }
    return *parsed;
}

inline ftclResult cmd_thread_channel_create(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(3, argv, 3, 3, "");
    if (!chk.has_value()) {
        return chk;
    }

    const ftclInt id = thread_channel_manager().create();
    return ftcl_ok(id);
}

inline ftclResult cmd_thread_channel_send(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(3, argv, 5, 5, "channelId value");
    if (!chk.has_value()) {
        return chk;
    }

    auto id = parse_thread_channel_id(argv[3]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }

    return thread_channel_manager().send(*id, argv[4].as_string());
}

inline ftclResult cmd_thread_channel_recv(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(3, argv, 4, 4, "channelId");
    if (!chk.has_value()) {
        return chk;
    }

    auto id = parse_thread_channel_id(argv[3]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }

    auto value = thread_channel_manager().recv(*id);
    if (!value.has_value()) {
        return ftcl::unexpected(value.error());
    }

    return ftcl_ok(*value);
}

inline ftclResult cmd_thread_channel_try_recv(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(3, argv, 4, 4, "channelId");
    if (!chk.has_value()) {
        return chk;
    }

    auto id = parse_thread_channel_id(argv[3]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }

    auto value = thread_channel_manager().try_recv(*id);
    if (!value.has_value()) {
        return ftcl::unexpected(value.error());
    }

    if (!value->has_value()) {
        return ftcl_ok(Value::empty());
    }

    return ftcl_ok(value->value());
}

inline ftclResult cmd_thread_channel(Interp* interp, ContextID context_id, const std::vector<Value>& argv) {
    std::vector<Subcommand> subs = {
        Subcommand("create", cmd_thread_channel_create),
        Subcommand("send", cmd_thread_channel_send),
        Subcommand("recv", cmd_thread_channel_recv),
        Subcommand("try_recv", cmd_thread_channel_try_recv),
    };
    return interp->call_subcommand(context_id, argv, 2, subs);
}

inline ftclResult cmd_thread_spawn(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "script");
    if (!chk.has_value()) {
        return chk;
    }

    const ftclInt id = thread_task_manager().spawn(argv[2].as_string());
    return ftcl_ok(id);
}

inline ftclResult cmd_thread_ready(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "taskId");
    if (!chk.has_value()) {
        return chk;
    }

    auto id = parse_thread_task_id(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }

    auto ready = thread_task_manager().ready(*id);
    if (!ready.has_value()) {
        return ftcl::unexpected(ready.error());
    }

    return ftcl_ok(*ready);
}

inline ftclResult cmd_thread_await(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "taskId");
    if (!chk.has_value()) {
        return chk;
    }

    auto id = parse_thread_task_id(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }

    auto result = thread_task_manager().await(*id);
    if (!result.has_value()) {
        return ftcl::unexpected(result.error());
    }

    if (!result->has_value()) {
        return ftcl::unexpected(result->error());
    }

    return result->value();
}

inline ftclResult cmd_thread_ids(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 2, 2, "");
    if (!chk.has_value()) {
        return chk;
    }

    std::vector<Value> out;
    for (ftclInt id : thread_task_manager().ids()) {
        out.emplace_back(id);
    }
    return ftcl_ok(Value::from_list(out));
}

inline ftclResult cmd_thread(Interp* interp, ContextID context_id, const std::vector<Value>& argv) {
    std::vector<Subcommand> subs = {
        Subcommand("spawn", cmd_thread_spawn),
        Subcommand("ready", cmd_thread_ready),
        Subcommand("await", cmd_thread_await),
        Subcommand("ids", cmd_thread_ids),
        Subcommand("channel", cmd_thread_channel),
    };
    return interp->call_subcommand(context_id, argv, 1, subs);
}

inline ftclResult cmd_throw(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 3, 3, "type message");
    if (!chk.has_value()) {
        return chk;
    }

    return ftcl_err2(argv[1], argv[2]);
}

inline ftclResult cmd_return(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 1, 0, "?options...? ?value?");
    if (!chk.has_value()) {
        return chk;
    }

    ResultCodeValue code(ResultCode::Okay);
    ftclInt level = 1;
    std::optional<Value> error_code;
    std::optional<Value> error_info;

    Value return_value = Value::empty();
    std::size_t opt_end = argv.size();

    if ((argv.size() - 1) % 2 == 1) {
        return_value = argv.back();
        opt_end = argv.size() - 1;
    }

    for (std::size_t i = 1; i + 1 < opt_end; i += 2) {
        const std::string opt = argv[i].as_string();
        const Value val = argv[i + 1];

        if (opt == "-code") {
            if (auto rc = ResultCodeValue::from_string(val.as_string()); rc.has_value()) {
                code = *rc;
            } else if (auto iv = val.as_int_opt(); iv.has_value()) {
                code = ResultCodeValue(*iv);
            } else {
                return ftcl_err("invalid -code value \"" + val.as_string() + "\"");
            }
        } else if (opt == "-level") {
            auto iv = parse_int(val);
            if (!iv.has_value()) {
                return ftcl_err(iv.error());
            }
            level = *iv;
            if (level < 0) {
                return ftcl_err("bad -level value: expected non-negative integer but got \"" + val.as_string() + "\"");
            }
        } else if (opt == "-errorcode") {
            error_code = val;
        } else if (opt == "-errorinfo") {
            error_info = val;
        } else {
            return ftcl_err("invalid return option: \"" + opt + "\"");
        }
    }

    if (code == ResultCodeValue(ResultCode::Error)) {
        return ftcl::unexpected(Exception::ftcl_return_err(return_value, static_cast<std::size_t>(level), error_code, error_info));
    }

    if (level == 0 && code == ResultCodeValue(ResultCode::Okay)) {
        return return_value;
    }

    return ftcl::unexpected(Exception::ftcl_return_ext(return_value, static_cast<std::size_t>(level), code));
}

inline ftclResult cmd_catch(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 4, "script ?resultVarName? ?optionsVarName?");
    if (!chk.has_value()) {
        return chk;
    }

    ftclResult result = interp->eval_value(argv[1]);

    ftclInt code = 0;
    Value value = Value::empty();

    if (result.has_value()) {
        code = 0;
        value = *result;
    } else {
        code = result.error().code().as_int();
        value = result.error().value();
    }

    if (argv.size() >= 3) {
        auto set = interp->set_var(argv[2], value);
        if (!set.has_value()) {
            return ftcl::unexpected(set.error());
        }
    }

    if (argv.size() == 4) {
        auto set = interp->set_var(argv[3], interp->return_options(result));
        if (!set.has_value()) {
            return ftcl::unexpected(set.error());
        }
    }

    if (!result.has_value() && result.error().code() == ResultCodeValue(ResultCode::Error)) {
        const auto& ex = result.error();
        if (ex.error_data() != nullptr) {
            auto set_code = interp->set_var(Value("errorCode"), ex.error_code());
            if (!set_code.has_value()) {
                return ftcl::unexpected(set_code.error());
            }

            auto set_info = interp->set_var(Value("errorInfo"), ex.error_info());
            if (!set_info.has_value()) {
                return ftcl::unexpected(set_info.error());
            }
        }
    }

    return ftcl_ok(code);
}

inline ftclResult cmd_proc(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 4, 4, "name args body");
    if (!chk.has_value()) {
        return chk;
    }

    auto args = argv[2].as_list();
    if (!args.has_value()) {
        return ftcl_err(args.error());
    }

    for (const auto& arg : *args) {
        auto spec = arg.as_list();
        if (!spec.has_value()) {
            return ftcl_err(spec.error());
        }

        if (spec->empty()) {
            return ftcl_err("argument with no name");
        }

        if (spec->size() > 2) {
            return ftcl_err("too many fields in argument specifier \"" + arg.as_string() + "\"");
        }
    }

    interp->add_proc(argv[1].as_string(), *args, argv[3]);
    return ftcl_ok();
}

inline ftclResult cmd_if(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto no_expr = [](const std::string& keyword) {
        return ftcl_err("wrong # args: no expression after \"" + keyword + "\" argument");
    };
    auto no_script = [](const std::string& token) {
        return ftcl_err("wrong # args: no script following after \"" + token + "\" argument");
    };

    std::size_t i = 1;
    bool branch_taken = false;
    Value branch_result = Value::empty();
    std::string keyword = "if";

    while (true) {
        if (i >= argv.size()) {
            return no_expr(keyword);
        }

        Value expr_token = argv[i++];
        bool cond = false;
        if (!branch_taken) {
            auto parsed = interp->expr_bool(expr_token);
            if (!parsed.has_value()) {
                return ftcl::unexpected(parsed.error());
            }
            cond = *parsed;
        }

        if (i < argv.size() && argv[i].as_string() == "then") {
            ++i;
            if (i >= argv.size()) {
                return no_script("then");
            }
        }

        if (i >= argv.size()) {
            return no_script(expr_token.as_string());
        }

        Value body = argv[i++];
        if (!branch_taken && cond) {
            auto result = interp->eval_value(body);
            if (!result.has_value()) {
                return result;
            }
            branch_result = *result;
            branch_taken = true;
        }

        if (i >= argv.size()) {
            break;
        }

        const std::string sep = argv[i].as_string();
        if (sep == "elseif") {
            ++i;
            keyword = "elseif";
            continue;
        }

        if (sep == "else") {
            ++i;
            if (i >= argv.size()) {
                return no_script("else");
            }
            if (branch_taken) {
                return branch_result;
            }
            return interp->eval_value(argv[i]);
        }

        // Minimal form: `if expr body elseBody` (without explicit "else")
        if (i == argv.size() - 1) {
            if (branch_taken) {
                return branch_result;
            }
            return interp->eval_value(argv[i]);
        }

        return ftcl_err("wrong # args: should be \"if expr ?then? script ?elseif expr ?then? script ...? ?else script?\"");
    }

    if (branch_taken) {
        return branch_result;
    }
    return ftcl_ok();
}

inline ftclResult cmd_while(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 3, 3, "test command");
    if (!chk.has_value()) {
        return chk;
    }

    while (true) {
        auto cond = interp->expr_bool(argv[1]);
        if (!cond.has_value()) {
            return ftcl::unexpected(cond.error());
        }
        if (!*cond) {
            break;
        }

        auto body = interp->eval_value(argv[2]);
        if (!body.has_value()) {
            auto ex = body.error();
            if (ex.code() == ResultCodeValue(ResultCode::Break)) {
                break;
            }
            if (ex.code() == ResultCodeValue(ResultCode::Continue)) {
                continue;
            }
            return ftcl::unexpected(ex);
        }
    }

    return ftcl_ok();
}

inline ftclResult cmd_for(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 5, 5, "start test next command");
    if (!chk.has_value()) {
        return chk;
    }

    auto start = interp->eval_value(argv[1]);
    if (!start.has_value()) {
        return start;
    }

    while (true) {
        auto cond = interp->expr_bool(argv[2]);
        if (!cond.has_value()) {
            return ftcl::unexpected(cond.error());
        }
        if (!*cond) {
            break;
        }

        auto body = interp->eval_value(argv[4]);
        if (!body.has_value()) {
            auto ex = body.error();
            if (ex.code() == ResultCodeValue(ResultCode::Break)) {
                break;
            }
            if (ex.code() != ResultCodeValue(ResultCode::Continue)) {
                return ftcl::unexpected(ex);
            }
        }

        auto next = interp->eval_value(argv[3]);
        if (!next.has_value()) {
            auto ex = next.error();
            if (ex.code() == ResultCodeValue(ResultCode::Break)) {
                break;
            }
            if (ex.code() == ResultCodeValue(ResultCode::Continue)) {
                return ftcl_err("invoked \"continue\" outside of a loop");
            }
            return ftcl::unexpected(ex);
        }
    }

    return ftcl_ok();
}

inline ftclResult cmd_foreach(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 4, 4, "varList list body");
    if (!chk.has_value()) {
        return chk;
    }

    auto var_list = argv[1].as_list();
    if (!var_list.has_value()) {
        return ftcl_err(var_list.error());
    }

    auto list = argv[2].as_list();
    if (!list.has_value()) {
        return ftcl_err(list.error());
    }

    std::size_t i = 0;
    while (i < list->size()) {
        for (const auto& var : *var_list) {
            Value assign = Value::empty();
            if (i < list->size()) {
                assign = (*list)[i++];
            }
            auto set = interp->set_var(var, assign);
            if (!set.has_value()) {
                return ftcl::unexpected(set.error());
            }
        }

        auto body = interp->eval_value(argv[3]);
        if (!body.has_value()) {
            auto ex = body.error();
            if (ex.code() == ResultCodeValue(ResultCode::Break)) {
                break;
            }
            if (ex.code() != ResultCodeValue(ResultCode::Continue)) {
                return ftcl::unexpected(ex);
            }
        }
    }

    return ftcl_ok();
}

inline ftclResult cmd_incr(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 3, "varName ?increment?");
    if (!chk.has_value()) {
        return chk;
    }

    ftclInt increment = 1;
    if (argv.size() == 3) {
        auto parsed = parse_int(argv[2]);
        if (!parsed.has_value()) {
            return ftcl_err(parsed.error());
        }
        increment = *parsed;
    }

    ftclInt current = 0;
    auto old = interp->var(argv[1]);
    if (old.has_value()) {
        auto parsed = parse_int(*old);
        if (!parsed.has_value()) {
            return ftcl_err(parsed.error());
        }
        current = *parsed;
    }

    const ftclInt next = current + increment;
    auto set = interp->set_var_return(argv[1], Value(next));
    if (!set.has_value()) {
        return ftcl::unexpected(set.error());
    }

    return *set;
}

inline ftclResult cmd_global(Interp* interp, ContextID, const std::vector<Value>& argv) {
    if (interp->scope_level() > 0) {
        for (std::size_t i = 1; i < argv.size(); ++i) {
            interp->upvar(0, argv[i].as_string());
        }
    }
    return ftcl_ok();
}

inline ftclResult cmd_list(Interp*, ContextID, const std::vector<Value>& argv) {
    std::vector<Value> items;
    for (std::size_t i = 1; i < argv.size(); ++i) {
        items.push_back(argv[i]);
    }
    return ftcl_ok(Value::from_list(items));
}

inline ftclResult cmd_llength(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 2, "list");
    if (!chk.has_value()) {
        return chk;
    }

    auto list = argv[1].as_list();
    if (!list.has_value()) {
        return ftcl_err(list.error());
    }

    return ftcl_ok(static_cast<ftclInt>(list->size()));
}

inline ftclResult cmd_lindex(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 0, "list ?index ...?");
    if (!chk.has_value()) {
        return chk;
    }

    std::vector<Value> indices;
    if (argv.size() == 3) {
        auto parsed = argv[2].as_list();
        if (!parsed.has_value()) {
            return ftcl_err(parsed.error());
        }
        indices = *parsed;
    } else {
        for (std::size_t i = 2; i < argv.size(); ++i) {
            indices.push_back(argv[i]);
        }
    }

    Value cur = argv[1];
    for (const auto& index_val : indices) {
        auto list = cur.as_list();
        if (!list.has_value()) {
            return ftcl_err(list.error());
        }

        auto index = parse_int(index_val);
        if (!index.has_value()) {
            return ftcl_err(index.error());
        }

        if (*index < 0 || static_cast<std::size_t>(*index) >= list->size()) {
            cur = Value::empty();
        } else {
            cur = (*list)[static_cast<std::size_t>(*index)];
        }
    }

    return ftcl_ok(cur);
}

inline ftclResult cmd_lappend(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 0, "varName ?value ...?");
    if (!chk.has_value()) {
        return chk;
    }

    ftclList list;
    auto old = interp->var(argv[1]);
    if (old.has_value()) {
        auto parsed = old->as_list();
        if (!parsed.has_value()) {
            return ftcl_err(parsed.error());
        }
        list = *parsed;
    }

    for (std::size_t i = 2; i < argv.size(); ++i) {
        list.push_back(argv[i]);
    }

    auto set = interp->set_var_return(argv[1], Value::from_list(list));
    if (!set.has_value()) {
        return ftcl::unexpected(set.error());
    }
    return *set;
}

inline ftclResult cmd_join(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 2, 3, "list ?joinString?");
    if (!chk.has_value()) {
        return chk;
    }

    auto list = argv[1].as_list();
    if (!list.has_value()) {
        return ftcl_err(list.error());
    }

    const std::string joiner = argv.size() == 3 ? argv[2].as_string() : " ";
    std::string out;
    for (std::size_t i = 0; i < list->size(); ++i) {
        if (i > 0) {
            out += joiner;
        }
        out += (*list)[i].as_string();
    }

    return ftcl_ok(out);
}

// string subcommands
inline ftclResult cmd_string_cat(Interp*, ContextID, const std::vector<Value>& argv) {
    std::string out;
    for (std::size_t i = 2; i < argv.size(); ++i) {
        out += argv[i].as_string();
    }
    return ftcl_ok(out);
}

inline bool utf8_is_continuation_byte(unsigned char byte) {
    return (byte & 0xC0u) == 0x80u;
}

inline std::size_t utf8_char_count(std::string_view s) {
    std::size_t count = 0;
    for (unsigned char byte : s) {
        if (!utf8_is_continuation_byte(byte)) {
            ++count;
        }
    }
    return count;
}

inline std::size_t utf8_next_char_byte(std::string_view s, std::size_t byte_index) {
    if (byte_index >= s.size()) {
        return s.size();
    }

    std::size_t i = byte_index + 1;
    while (i < s.size() && utf8_is_continuation_byte(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    return i;
}

inline std::size_t utf8_advance_chars(std::string_view s, std::size_t byte_index, std::size_t char_count) {
    std::size_t i = byte_index;
    for (std::size_t c = 0; c < char_count && i < s.size(); ++c) {
        i = utf8_next_char_byte(s, i);
    }
    return i;
}

inline std::optional<std::size_t> utf8_byte_index_of_char(std::string_view s, std::size_t char_index) {
    std::size_t current_char = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (!utf8_is_continuation_byte(static_cast<unsigned char>(s[i]))) {
            if (current_char == char_index) {
                return i;
            }
            ++current_char;
        }
    }

    if (current_char == char_index) {
        return s.size();
    }
    return std::nullopt;
}

struct StringCompareOptions {
    bool nocase = false;
    std::optional<ftclInt> length;
};

inline ftcl::expected<StringCompareOptions, Exception> parse_string_compare_options(
    const std::vector<Value>& argv,
    const std::string& cmd_name) {
    const std::size_t arglen = argv.size();
    StringCompareOptions opts;

    std::size_t i = 2;
    while (i < arglen - 2) {
        const std::string opt = argv[i].as_string();
        if (opt == "-nocase") {
            opts.nocase = true;
            ++i;
            continue;
        }

        if (opt == "-length") {
            if (i + 1 >= arglen - 2) {
                return ftcl::unexpected(Exception::ftcl_err(
                    Value("wrong # args: should be \"string " + cmd_name +
                          " ?-nocase? ?-length length? string1 string2\"")));
            }

            auto parsed = parse_int(argv[i + 1]);
            if (!parsed.has_value()) {
                return ftcl::unexpected(Exception::ftcl_err(Value(parsed.error())));
            }

            opts.length = *parsed;
            i += 2;
            continue;
        }

        return ftcl::unexpected(
            Exception::ftcl_err(Value("bad option \"" + opt + "\": must be -nocase or -length")));
    }

    return opts;
}

inline ftclResult cmd_string_length(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "string");
    if (!chk.has_value()) {
        return chk;
    }

    return ftcl_ok(static_cast<ftclInt>(utf8_char_count(argv[2].as_string())));
}

inline ftclResult cmd_string_compare(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 7, "?-nocase? ?-length length? string1 string2");
    if (!chk.has_value()) {
        return chk;
    }

    auto opts = parse_string_compare_options(argv, "compare");
    if (!opts.has_value()) {
        return ftcl::unexpected(opts.error());
    }

    const std::size_t arglen = argv.size();
    std::string a = argv[arglen - 2].as_string();
    std::string b = argv[arglen - 1].as_string();

    if (opts->nocase) {
        a = to_lower(a);
        b = to_lower(b);
    }

    auto cmp = compare_len(a, b, opts->length);
    if (!cmp.has_value()) {
        return ftcl_err(cmp.error());
    }

    return ftcl_ok(static_cast<ftclInt>(*cmp));
}

inline ftclResult cmd_string_equal(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 7, "?-nocase? ?-length length? string1 string2");
    if (!chk.has_value()) {
        return chk;
    }

    auto opts = parse_string_compare_options(argv, "equal");
    if (!opts.has_value()) {
        return ftcl::unexpected(opts.error());
    }

    const std::size_t arglen = argv.size();
    std::string a = argv[arglen - 2].as_string();
    std::string b = argv[arglen - 1].as_string();

    if (opts->nocase) {
        a = to_lower(a);
        b = to_lower(b);
    }

    auto cmp = compare_len(a, b, opts->length);
    if (!cmp.has_value()) {
        return ftcl_err(cmp.error());
    }

    return ftcl_ok(*cmp == 0);
}

inline ftclResult cmd_string_first(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "needleString haystackString ?startIndex?");
    if (!chk.has_value()) {
        return chk;
    }

    const std::string needle = argv[2].as_string();
    const std::string haystack = argv[3].as_string();

    std::size_t start_char = 0;
    if (argv.size() == 5) {
        auto parsed = parse_int(argv[4]);
        if (!parsed.has_value()) {
            return ftcl_err(parsed.error());
        }
        if (*parsed > 0) {
            start_char = static_cast<std::size_t>(*parsed);
        }
    }

    const std::size_t total_chars = utf8_char_count(haystack);
    if (start_char >= total_chars) {
        return ftcl_ok(static_cast<ftclInt>(-1));
    }

    auto start_byte_opt = utf8_byte_index_of_char(haystack, start_char);
    if (!start_byte_opt.has_value()) {
        return ftcl_ok(static_cast<ftclInt>(-1));
    }

    const std::size_t start_byte = *start_byte_opt;
    const std::size_t pos_in_slice = haystack.substr(start_byte).find(needle);
    if (pos_in_slice == std::string::npos) {
        return ftcl_ok(static_cast<ftclInt>(-1));
    }

    const std::size_t byte_pos = start_byte + pos_in_slice;
    return ftcl_ok(static_cast<ftclInt>(utf8_char_count(std::string_view(haystack).substr(0, byte_pos))));
}

inline ftclResult cmd_string_last(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "needleString haystackString ?lastIndex?");
    if (!chk.has_value()) {
        return chk;
    }

    const std::string needle = argv[2].as_string();
    const std::string haystack = argv[3].as_string();
    const std::size_t total_chars = utf8_char_count(haystack);
    std::string_view slice = haystack;

    if (argv.size() == 5) {
        auto parsed = parse_int(argv[4]);
        if (!parsed.has_value()) {
            return ftcl_err(parsed.error());
        }
        if (*parsed < 0) {
            return ftcl_ok(static_cast<ftclInt>(-1));
        }
        const std::size_t last_char = static_cast<std::size_t>(*parsed);
        if (last_char < total_chars) {
            auto end_byte_opt = utf8_byte_index_of_char(haystack, last_char + 1);
            if (end_byte_opt.has_value()) {
                slice = std::string_view(haystack).substr(0, *end_byte_opt);
            }
        }
    }

    const std::size_t pos = slice.rfind(needle);
    if (pos == std::string::npos) {
        return ftcl_ok(static_cast<ftclInt>(-1));
    }

    return ftcl_ok(static_cast<ftclInt>(utf8_char_count(std::string_view(haystack).substr(0, pos))));
}

inline ftclResult cmd_string_map(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "?-nocase? charMap string");
    if (!chk.has_value()) {
        return chk;
    }

    bool nocase = false;
    std::size_t map_index = 2;
    if (argv.size() == 5) {
        if (argv[2].as_string() == "-nocase") {
            nocase = true;
            map_index = 3;
        } else {
            return ftcl_err("bad option \"" + argv[2].as_string() + "\": must be -nocase");
        }
    }

    auto mapping = argv[map_index].as_list();
    if (!mapping.has_value()) {
        return ftcl_err(mapping.error());
    }
    if (mapping->size() % 2 != 0) {
        return ftcl_err("missing value to go with key");
    }

    struct MappingEntry {
        std::string from;
        std::size_t from_char_count;
        std::string to;
    };

    std::vector<MappingEntry> entries;
    entries.reserve(mapping->size() / 2);

    for (std::size_t i = 0; i + 1 < mapping->size(); i += 2) {
        std::string from = (*mapping)[i].as_string();
        if (nocase) {
            from = to_lower(from);
        }

        const std::size_t count = utf8_char_count(from);
        if (count == 0) {
            continue;
        }

        entries.push_back(MappingEntry{from, count, (*mapping)[i + 1].as_string()});
    }

    const std::string input = argv[map_index + 1].as_string();
    const std::string input_match = nocase ? to_lower(input) : input;

    std::string out;
    std::size_t i = 0;
    while (i < input.size()) {
        bool matched = false;

        for (const auto& entry : entries) {
            if (input_match.size() - i < entry.from.size()) {
                continue;
            }

            if (std::string_view(input_match).substr(i, entry.from.size()) == entry.from) {
                out += entry.to;
                i = utf8_advance_chars(input, i, entry.from_char_count);
                matched = true;
                break;
            }
        }

        if (matched) {
            continue;
        }

        const std::size_t next = utf8_next_char_byte(input, i);
        out.append(input, i, next - i);
        i = next;
    }

    return ftcl_ok(out);
}

inline ftclResult cmd_string_range(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 5, 5, "string first last");
    if (!chk.has_value()) {
        return chk;
    }

    auto first = parse_int(argv[3]);
    auto last = parse_int(argv[4]);
    if (!first.has_value()) {
        return ftcl_err(first.error());
    }
    if (!last.has_value()) {
        return ftcl_err(last.error());
    }

    const std::string s = argv[2].as_string();
    if (*last < 0) {
        return ftcl_ok("");
    }

    ftclInt lo = std::max<ftclInt>(0, *first);
    ftclInt hi = std::max<ftclInt>(0, *last);
    if (lo > hi) {
        return ftcl_ok("");
    }

    const std::size_t total = utf8_char_count(s);
    if (static_cast<std::size_t>(lo) >= total) {
        return ftcl_ok("");
    }

    if (static_cast<std::size_t>(hi) >= total) {
        hi = static_cast<ftclInt>(total - 1);
    }

    auto start_byte = utf8_byte_index_of_char(s, static_cast<std::size_t>(lo));
    auto end_byte = utf8_byte_index_of_char(s, static_cast<std::size_t>(hi) + 1);
    if (!start_byte.has_value()) {
        return ftcl_ok("");
    }

    const std::size_t start = *start_byte;
    const std::size_t end = end_byte.value_or(s.size());
    return ftcl_ok(s.substr(start, end - start));
}

inline ftclResult cmd_string_tolower(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "string");
    if (!chk.has_value()) {
        return chk;
    }
    return ftcl_ok(to_lower(argv[2].as_string()));
}

inline ftclResult cmd_string_toupper(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "string");
    if (!chk.has_value()) {
        return chk;
    }
    return ftcl_ok(to_upper(argv[2].as_string()));
}

inline ftclResult cmd_string_trim(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "string");
    if (!chk.has_value()) {
        return chk;
    }

    std::string s = argv[2].as_string();
    auto not_space = [](unsigned char c) { return std::isspace(c) == 0; };

    const auto first = std::find_if(s.begin(), s.end(), [&](char c) { return not_space(static_cast<unsigned char>(c)); });
    const auto last = std::find_if(s.rbegin(), s.rend(), [&](char c) { return not_space(static_cast<unsigned char>(c)); }).base();

    if (first >= last) {
        return ftcl_ok("");
    }

    const std::string mode = argv[1].as_string();
    if (mode == "trimleft") {
        return ftcl_ok(std::string(first, s.end()));
    }
    if (mode == "trimright") {
        return ftcl_ok(std::string(s.begin(), last));
    }
    return ftcl_ok(std::string(first, last));
}

inline ftclResult cmd_string_todo(Interp*, ContextID, const std::vector<Value>&) {
    return ftcl_err("TODO");
}

inline ftclResult cmd_string(Interp* interp, ContextID context_id, const std::vector<Value>& argv) {
    std::vector<Subcommand> subs = {
        Subcommand("cat", cmd_string_cat),        Subcommand("compare", cmd_string_compare),
        Subcommand("equal", cmd_string_equal),    Subcommand("first", cmd_string_first),
        Subcommand("last", cmd_string_last),      Subcommand("length", cmd_string_length),
        Subcommand("map", cmd_string_map),        Subcommand("range", cmd_string_range),
        Subcommand("tolower", cmd_string_tolower),Subcommand("toupper", cmd_string_toupper),
        Subcommand("trim", cmd_string_trim),      Subcommand("trimleft", cmd_string_trim),
        Subcommand("trimright", cmd_string_trim),
    };
    return interp->call_subcommand(context_id, argv, 1, subs);
}

// info subcommands
inline ftclResult cmd_info_args(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "procname");
    if (!chk.has_value()) {
        return chk;
    }

    auto out = interp->proc_args(argv[2].as_string());
    if (!out.has_value()) {
        return ftcl::unexpected(out.error());
    }
    return *out;
}

inline ftclResult cmd_info_body(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "procname");
    if (!chk.has_value()) {
        return chk;
    }

    auto out = interp->proc_body(argv[2].as_string());
    if (!out.has_value()) {
        return ftcl::unexpected(out.error());
    }
    return *out;
}

inline ftclResult cmd_info_cmdtype(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "command");
    if (!chk.has_value()) {
        return chk;
    }

    auto out = interp->command_type(argv[2].as_string());
    if (!out.has_value()) {
        return ftcl::unexpected(out.error());
    }
    return *out;
}

inline ftclResult cmd_info_commands(Interp* interp, ContextID, const std::vector<Value>&) {
    return ftcl_ok(Value::from_list(interp->command_names()));
}

inline ftclResult cmd_info_exists(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "varname");
    if (!chk.has_value()) {
        return chk;
    }
    return ftcl_ok(interp->var_exists(argv[2]));
}

inline ftclResult cmd_info_complete(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "command");
    if (!chk.has_value()) {
        return chk;
    }
    return ftcl_ok(interp->complete(argv[2].as_string()));
}

inline ftclResult cmd_info_globals(Interp* interp, ContextID, const std::vector<Value>&) {
    return ftcl_ok(Value::from_list(interp->vars_in_global_scope()));
}

inline ftclResult cmd_info_locals(Interp* interp, ContextID, const std::vector<Value>&) {
    return ftcl_ok(Value::from_list(interp->vars_in_local_scope()));
}

inline ftclResult cmd_info_procs(Interp* interp, ContextID, const std::vector<Value>&) {
    return ftcl_ok(Value::from_list(interp->proc_names()));
}

inline ftclResult cmd_info_vars(Interp* interp, ContextID, const std::vector<Value>&) {
    return ftcl_ok(Value::from_list(interp->vars_in_scope()));
}

inline ftclResult cmd_info_default(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 5, 5, "procname arg varname");
    if (!chk.has_value()) {
        return chk;
    }

    auto d = interp->proc_default(argv[2].as_string(), argv[3].as_string());
    if (!d.has_value()) {
        return ftcl::unexpected(d.error());
    }

    if (d->has_value()) {
        auto set = interp->set_var(argv[4], **d);
        if (!set.has_value()) {
            return ftcl::unexpected(set.error());
        }
        return ftcl_ok(static_cast<ftclInt>(1));
    }

    auto set = interp->set_var(argv[4], Value::empty());
    if (!set.has_value()) {
        return ftcl::unexpected(set.error());
    }
    return ftcl_ok(static_cast<ftclInt>(0));
}

inline ftclResult cmd_info(Interp* interp, ContextID context_id, const std::vector<Value>& argv) {
    if (argv.size() == 1) {
        return ftcl_err("wrong # args: should be \"info subcommand ?arg ...?\"");
    }

    std::vector<Subcommand> subs = {
        Subcommand("args", cmd_info_args),         Subcommand("body", cmd_info_body),
        Subcommand("cmdtype", cmd_info_cmdtype),   Subcommand("commands", cmd_info_commands),
        Subcommand("complete", cmd_info_complete), Subcommand("default", cmd_info_default),
        Subcommand("exists", cmd_info_exists),     Subcommand("globals", cmd_info_globals),
        Subcommand("locals", cmd_info_locals),     Subcommand("procs", cmd_info_procs),
        Subcommand("vars", cmd_info_vars),
    };
    return interp->call_subcommand(context_id, argv, 1, subs);
}

// dict subcommands
inline ftclResult cmd_dict_create(Interp*, ContextID, const std::vector<Value>& argv) {
    if (argv.size() % 2 != 0) {
        return ftcl_err("wrong # args: should be \"dict create ?key value?\"");
    }

    std::vector<Value> kv;
    for (std::size_t i = 2; i < argv.size(); ++i) {
        kv.push_back(argv[i]);
    }

    return ftcl_ok(Value::from_dict(list_to_dict(kv)));
}

inline ftclResult cmd_dict_get(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 0, "dictionary ?key ...?");
    if (!chk.has_value()) {
        return chk;
    }

    Value current = argv[2];
    for (std::size_t i = 3; i < argv.size(); ++i) {
        auto dict = current.as_dict();
        if (!dict.has_value()) {
            return ftcl_err(dict.error());
        }
        auto it = dict->find(argv[i].as_string());
        if (it == dict->end()) {
            return ftcl_err("key \"" + argv[i].as_string() + "\" not known in dictionary");
        }
        current = it->second;
    }

    return ftcl_ok(current);
}

inline ftclResult cmd_dict_exists(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 0, "dictionary key ?key ...?");
    if (!chk.has_value()) {
        return chk;
    }

    Value current = argv[2];
    for (std::size_t i = 3; i < argv.size(); ++i) {
        auto dict = current.as_dict();
        if (!dict.has_value()) {
            return ftcl_ok(false);
        }
        auto it = dict->find(argv[i].as_string());
        if (it == dict->end()) {
            return ftcl_ok(false);
        }
        current = it->second;
    }

    return ftcl_ok(true);
}

inline ftclResult cmd_dict_keys(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "dictionary");
    if (!chk.has_value()) {
        return chk;
    }

    auto dict = argv[2].as_dict();
    if (!dict.has_value()) {
        return ftcl_err(dict.error());
    }

    ftclList keys;
    for (const auto& [k, _] : *dict) {
        keys.emplace_back(k);
    }

    return ftcl_ok(Value::from_list(keys));
}

inline ftclResult cmd_dict_values(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "dictionary");
    if (!chk.has_value()) {
        return chk;
    }

    auto dict = argv[2].as_dict();
    if (!dict.has_value()) {
        return ftcl_err(dict.error());
    }

    ftclList vals;
    for (const auto& [_, v] : *dict) {
        vals.push_back(v);
    }

    return ftcl_ok(Value::from_list(vals));
}

inline ftclResult cmd_dict_size(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "dictionary");
    if (!chk.has_value()) {
        return chk;
    }

    auto dict = argv[2].as_dict();
    if (!dict.has_value()) {
        return ftcl_err(dict.error());
    }

    return ftcl_ok(static_cast<ftclInt>(dict->size()));
}

inline ftclResult cmd_dict_remove(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 0, "dictionary ?key ...?");
    if (!chk.has_value()) {
        return chk;
    }

    auto dict = argv[2].as_dict();
    if (!dict.has_value()) {
        return ftcl_err(dict.error());
    }

    for (std::size_t i = 3; i < argv.size(); ++i) {
        dict->erase(argv[i].as_string());
    }

    return ftcl_ok(Value::from_dict(*dict));
}

inline ftclResult cmd_dict_set(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 5, 0, "dictVarName key ?key ...? value");
    if (!chk.has_value()) {
        return chk;
    }

    std::vector<Value> keys;
    for (std::size_t i = 3; i + 1 < argv.size(); ++i) {
        keys.push_back(argv[i]);
    }

    Value root = Value::from_dict({});
    auto old = interp->var(argv[2]);
    if (old.has_value()) {
        root = *old;
    }

    auto inserted = dict_path_insert(root, keys, argv.back());
    if (!inserted.has_value()) {
        return ftcl_err(inserted.error());
    }

    auto set = interp->set_var_return(argv[2], *inserted);
    if (!set.has_value()) {
        return ftcl::unexpected(set.error());
    }

    return *set;
}

inline ftclResult cmd_dict_unset(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 0, "dictVarName key ?key ...?");
    if (!chk.has_value()) {
        return chk;
    }

    std::vector<Value> keys;
    for (std::size_t i = 3; i < argv.size(); ++i) {
        keys.push_back(argv[i]);
    }

    Value root = Value::from_dict({});
    auto old = interp->var(argv[2]);
    if (old.has_value()) {
        root = *old;
    }

    auto removed = dict_path_remove(root, keys);
    if (!removed.has_value()) {
        return ftcl_err(removed.error());
    }

    auto set = interp->set_var_return(argv[2], *removed);
    if (!set.has_value()) {
        return ftcl::unexpected(set.error());
    }

    return *set;
}

inline ftclResult cmd_dict(Interp* interp, ContextID context_id, const std::vector<Value>& argv) {
    std::vector<Subcommand> subs = {
        Subcommand("create", cmd_dict_create), Subcommand("exists", cmd_dict_exists),
        Subcommand("get", cmd_dict_get),       Subcommand("keys", cmd_dict_keys),
        Subcommand("remove", cmd_dict_remove), Subcommand("set", cmd_dict_set),
        Subcommand("size", cmd_dict_size),     Subcommand("unset", cmd_dict_unset),
        Subcommand("values", cmd_dict_values),
    };
    return interp->call_subcommand(context_id, argv, 1, subs);
}

// array subcommands
inline ftclResult cmd_array_exists(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "arrayName");
    if (!chk.has_value()) {
        return chk;
    }
    return ftcl_ok(interp->array_exists(argv[2].as_string()));
}

inline ftclResult cmd_array_names(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "arrayName");
    if (!chk.has_value()) {
        return chk;
    }
    return ftcl_ok(Value::from_list(interp->array_names(argv[2].as_string())));
}

inline ftclResult cmd_array_get(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "arrayName");
    if (!chk.has_value()) {
        return chk;
    }
    return ftcl_ok(Value::from_list(interp->array_get(argv[2].as_string())));
}

inline ftclResult cmd_array_set(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 4, "arrayName list");
    if (!chk.has_value()) {
        return chk;
    }

    VarName var_name = argv[2].as_var_name();

    if (!var_name.index().has_value()) {
        auto list = argv[3].as_list();
        if (!list.has_value()) {
            return ftcl_err(list.error());
        }

        auto set = interp->array_set(var_name.name(), *list);
        if (!set.has_value()) {
            return ftcl::unexpected(set.error());
        }

        return *set;
    }

    // Tcl/ftcl compatibility: create array if absent, then error on element-form name.
    auto ensure_array = interp->array_set(var_name.name(), std::vector<Value>{});
    if (!ensure_array.has_value()) {
        return ftcl::unexpected(ensure_array.error());
    }

    return ftcl_err("can't set \"" + argv[2].as_string() + "\": variable isn't array");
}

inline ftclResult cmd_array_size(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "arrayName");
    if (!chk.has_value()) {
        return chk;
    }
    return ftcl_ok(static_cast<ftclInt>(interp->array_size(argv[2].as_string())));
}

inline ftclResult cmd_array_unset(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 4, "arrayName ?index?");
    if (!chk.has_value()) {
        return chk;
    }

    if (argv.size() == 4) {
        interp->unset_element(argv[2].as_string(), argv[3].as_string());
    } else {
        interp->array_unset(argv[2].as_string());
    }

    return ftcl_ok();
}

inline ftclResult cmd_array(Interp* interp, ContextID context_id, const std::vector<Value>& argv) {
    std::vector<Subcommand> subs = {
        Subcommand("exists", cmd_array_exists), Subcommand("get", cmd_array_get),
        Subcommand("names", cmd_array_names),   Subcommand("set", cmd_array_set),
        Subcommand("size", cmd_array_size),     Subcommand("unset", cmd_array_unset),
    };
    return interp->call_subcommand(context_id, argv, 1, subs);
}


// uvec subcommands: script-level bridge to cross-device vector storage.
class UVecCommandManager {
public:
    ftclInt create(UVec<ftclFloat> vec) {
        std::lock_guard<std::mutex> lock(mu_);
        const ftclInt id = next_id_++;
        vectors_.emplace(id, std::move(vec));
        return id;
    }

    ftclResult destroy(ftclInt id) {
        std::lock_guard<std::mutex> lock(mu_);
        const auto erased = vectors_.erase(id);
        if (erased == 0) {
            return ftcl_err("unknown uvec handle \"" + std::to_string(id) + "\"");
        }
        return ftcl_ok();
    }

    std::vector<ftclInt> ids() const {
        std::lock_guard<std::mutex> lock(mu_);
        std::vector<ftclInt> out;
        out.reserve(vectors_.size());
        for (const auto& [id, _] : vectors_) {
            out.push_back(id);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    template <class Func>
    ftclResult with_vec(ftclInt id, Func&& func) {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = vectors_.find(id);
        if (it == vectors_.end()) {
            return ftcl_err("unknown uvec handle \"" + std::to_string(id) + "\"");
        }
        return func(it->second);
    }

    template <class Func>
    ftclResult with_two(ftclInt dst_id, ftclInt src_id, Func&& func) {
        std::lock_guard<std::mutex> lock(mu_);
        auto dst = vectors_.find(dst_id);
        if (dst == vectors_.end()) {
            return ftcl_err("unknown uvec handle \"" + std::to_string(dst_id) + "\"");
        }
        auto src = vectors_.find(src_id);
        if (src == vectors_.end()) {
            return ftcl_err("unknown uvec handle \"" + std::to_string(src_id) + "\"");
        }
        return func(dst->second, src->second);
    }

private:
    mutable std::mutex mu_;
    std::unordered_map<ftclInt, UVec<ftclFloat>> vectors_;
    ftclInt next_id_ = 1;
};

inline UVecCommandManager& uvec_command_manager() {
    static UVecCommandManager manager;
    return manager;
}

inline ftcl::expected<Device, Exception> parse_uvec_device(const Value& value) {
    const std::string text = value.as_string();
    if (text == "cpu" || text == "CPU") {
        return Device::cpu();
    }
    if (text == "cuda" || text == "CUDA") {
        return Device::cuda(0);
    }

    const std::string cuda_prefix = "cuda:";
    const std::string cuda_upper_prefix = "CUDA:";
    if (text.rfind(cuda_prefix, 0) == 0 || text.rfind(cuda_upper_prefix, 0) == 0) {
        auto index = parse_int(Value(text.substr(cuda_prefix.size())));
        if (!index.has_value()) {
            return ftcl::unexpected(Exception::ftcl_err(Value(index.error())));
        }
        if (*index < 0) {
            return ftcl::unexpected(Exception::ftcl_err(Value("invalid CUDA device \"" + text + "\"")));
        }
        return Device::cuda(static_cast<int>(*index));
    }

    if (text.rfind("cuda", 0) == 0 && text.size() > 4) {
        auto index = parse_int(Value(text.substr(4)));
        if (!index.has_value()) {
            return ftcl::unexpected(Exception::ftcl_err(Value(index.error())));
        }
        if (*index < 0) {
            return ftcl::unexpected(Exception::ftcl_err(Value("invalid CUDA device \"" + text + "\"")));
        }
        return Device::cuda(static_cast<int>(*index));
    }

    return ftcl::unexpected(Exception::ftcl_err(Value("bad device \"" + text + "\": must be cpu or cuda:N")));
}

inline ftcl::expected<ftclInt, Exception> parse_uvec_handle(const Value& value) {
    auto id = parse_int(value);
    if (!id.has_value()) {
        return ftcl::unexpected(Exception::ftcl_err(Value(id.error())));
    }
    if (*id <= 0) {
        return ftcl::unexpected(Exception::ftcl_err(Value("invalid uvec handle \"" + value.as_string() + "\"")));
    }
    return *id;
}

inline ftcl::expected<std::size_t, Exception> parse_uvec_index(const Value& value, const std::string& what) {
    auto parsed = parse_int(value);
    if (!parsed.has_value()) {
        return ftcl::unexpected(Exception::ftcl_err(Value(parsed.error())));
    }
    if (*parsed < 0) {
        return ftcl::unexpected(Exception::ftcl_err(Value("bad " + what + " \"" + value.as_string() + "\": must be non-negative")));
    }
    return static_cast<std::size_t>(*parsed);
}

inline ftcl::expected<ftclFloat, Exception> parse_uvec_float(const Value& value) {
    auto parsed = parse_float(value);
    if (!parsed.has_value()) {
        return ftcl::unexpected(Exception::ftcl_err(Value(parsed.error())));
    }
    return *parsed;
}

inline ftcl::expected<std::vector<ftclFloat>, Exception> parse_uvec_value_list(const Value& value) {
    auto list = value.as_list();
    if (!list.has_value()) {
        return ftcl::unexpected(Exception::ftcl_err(Value(list.error())));
    }

    std::vector<ftclFloat> out;
    out.reserve(list->size());
    for (const auto& item : *list) {
        auto parsed = parse_uvec_float(item);
        if (!parsed.has_value()) {
            return ftcl::unexpected(parsed.error());
        }
        out.push_back(*parsed);
    }
    return out;
}

inline Value uvec_to_list_value(const UVec<ftclFloat>& vec) {
    ftclList values;
    values.reserve(vec.size());
    for (const auto& item : vec.cpu_vector()) {
        values.emplace_back(item);
    }
    return Value::from_list(values);
}

inline std::string uvec_pointer_address(const void* ptr) {
    std::ostringstream oss;
    oss << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(ptr);
    return oss.str();
}

inline ftclResult cmd_uvec_create(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 4, "list ?device?");
    if (!chk.has_value()) {
        return chk;
    }

    Device device = Device::cpu();
    if (argv.size() == 4) {
        auto parsed_device = parse_uvec_device(argv[3]);
        if (!parsed_device.has_value()) {
            return ftcl::unexpected(parsed_device.error());
        }
        device = *parsed_device;
    }

    auto values = parse_uvec_value_list(argv[2]);
    if (!values.has_value()) {
        return ftcl::unexpected(values.error());
    }

    UVec<ftclFloat> vec = UVec<ftclFloat>::from_cpu(std::move(*values));
    if (!device.is_cpu()) {
        auto dst = vec.as_mut_uptr(device);
        if (!dst.has_value()) {
            return ftcl_err(dst.error());
        }
    }

    return ftcl_ok(uvec_command_manager().create(std::move(vec)));
}

inline ftclResult cmd_uvec_filled(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "length value ?device?");
    if (!chk.has_value()) {
        return chk;
    }

    auto len = parse_uvec_index(argv[2], "length");
    if (!len.has_value()) {
        return ftcl::unexpected(len.error());
    }
    auto value = parse_uvec_float(argv[3]);
    if (!value.has_value()) {
        return ftcl::unexpected(value.error());
    }

    Device device = Device::cpu();
    if (argv.size() == 5) {
        auto parsed_device = parse_uvec_device(argv[4]);
        if (!parsed_device.has_value()) {
            return ftcl::unexpected(parsed_device.error());
        }
        device = *parsed_device;
    }

    auto vec = UVec<ftclFloat>::filled(*len, *value, device);
    if (!vec.has_value()) {
        return ftcl_err(vec.error());
    }
    return ftcl_ok(uvec_command_manager().create(std::move(*vec)));
}

inline ftclResult cmd_uvec_zeroed(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 4, "length ?device?");
    if (!chk.has_value()) {
        return chk;
    }

    auto len = parse_uvec_index(argv[2], "length");
    if (!len.has_value()) {
        return ftcl::unexpected(len.error());
    }

    Device device = Device::cpu();
    if (argv.size() == 4) {
        auto parsed_device = parse_uvec_device(argv[3]);
        if (!parsed_device.has_value()) {
            return ftcl::unexpected(parsed_device.error());
        }
        device = *parsed_device;
    }

    auto vec = UVec<ftclFloat>::zeroed(*len, device);
    if (!vec.has_value()) {
        return ftcl_err(vec.error());
    }
    return ftcl_ok(uvec_command_manager().create(std::move(*vec)));
}

inline ftclResult cmd_uvec_destroy(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "handle");
    if (!chk.has_value()) {
        return chk;
    }
    auto id = parse_uvec_handle(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }
    return uvec_command_manager().destroy(*id);
}

inline ftclResult cmd_uvec_ids(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 2, 2, "");
    if (!chk.has_value()) {
        return chk;
    }

    ftclList ids;
    for (ftclInt id : uvec_command_manager().ids()) {
        ids.emplace_back(id);
    }
    return ftcl_ok(Value::from_list(ids));
}

inline ftclResult cmd_uvec_len(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "handle");
    if (!chk.has_value()) {
        return chk;
    }
    auto id = parse_uvec_handle(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }

    return uvec_command_manager().with_vec(*id, [](UVec<ftclFloat>& vec) {
        return ftcl_ok(static_cast<ftclInt>(vec.size()));
    });
}

inline ftclResult cmd_uvec_get(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "handle index ?syncDevice?");
    if (!chk.has_value()) {
        return chk;
    }
    auto id = parse_uvec_handle(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }
    auto index = parse_uvec_index(argv[3], "index");
    if (!index.has_value()) {
        return ftcl::unexpected(index.error());
    }

    Device sync_device = Device::cpu();
    if (argv.size() == 5) {
        auto parsed_device = parse_uvec_device(argv[4]);
        if (!parsed_device.has_value()) {
            return ftcl::unexpected(parsed_device.error());
        }
        sync_device = *parsed_device;
    }

    return uvec_command_manager().with_vec(*id, [&](UVec<ftclFloat>& vec) {
        if (*index >= vec.size()) {
            return ftcl_err("uvec index out of range");
        }
        auto synced = vec.as_uptr(sync_device);
        if (!synced.has_value()) {
            return ftcl_err(synced.error());
        }
        auto cpu = vec.as_uptr(Device::cpu());
        if (!cpu.has_value()) {
            return ftcl_err(cpu.error());
        }
        return ftcl_ok(cpu->get()[*index]);
    });
}

inline ftclResult cmd_uvec_set(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 5, 5, "handle index value");
    if (!chk.has_value()) {
        return chk;
    }
    auto id = parse_uvec_handle(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }
    auto index = parse_uvec_index(argv[3], "index");
    if (!index.has_value()) {
        return ftcl::unexpected(index.error());
    }
    auto value = parse_uvec_float(argv[4]);
    if (!value.has_value()) {
        return ftcl::unexpected(value.error());
    }

    return uvec_command_manager().with_vec(*id, [&](UVec<ftclFloat>& vec) {
        if (*index >= vec.size()) {
            return ftcl_err("uvec index out of range");
        }
        vec[*index] = *value;
        return ftcl_ok(*value);
    });
}

inline ftclResult cmd_uvec_fill(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "handle value ?device?");
    if (!chk.has_value()) {
        return chk;
    }
    auto id = parse_uvec_handle(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }
    auto value = parse_uvec_float(argv[3]);
    if (!value.has_value()) {
        return ftcl::unexpected(value.error());
    }

    Device device = Device::cpu();
    if (argv.size() == 5) {
        auto parsed_device = parse_uvec_device(argv[4]);
        if (!parsed_device.has_value()) {
            return ftcl::unexpected(parsed_device.error());
        }
        device = *parsed_device;
    }

    return uvec_command_manager().with_vec(*id, [&](UVec<ftclFloat>& vec) {
        auto count = vec.fill(*value, device);
        if (!count.has_value()) {
            return ftcl_err(count.error());
        }
        return ftcl_ok(static_cast<ftclInt>(*count));
    });
}

inline ftclResult cmd_uvec_copy(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 7, "dstHandle srcHandle ?length? ?dstDevice? ?srcDevice?");
    if (!chk.has_value()) {
        return chk;
    }
    auto dst_id = parse_uvec_handle(argv[2]);
    if (!dst_id.has_value()) {
        return ftcl::unexpected(dst_id.error());
    }
    auto src_id = parse_uvec_handle(argv[3]);
    if (!src_id.has_value()) {
        return ftcl::unexpected(src_id.error());
    }

    std::optional<std::size_t> explicit_len;
    if (argv.size() >= 5) {
        auto len = parse_uvec_index(argv[4], "length");
        if (!len.has_value()) {
            return ftcl::unexpected(len.error());
        }
        explicit_len = *len;
    }

    Device dst_device = Device::cpu();
    Device src_device = Device::cpu();
    if (argv.size() >= 6) {
        auto parsed = parse_uvec_device(argv[5]);
        if (!parsed.has_value()) {
            return ftcl::unexpected(parsed.error());
        }
        dst_device = *parsed;
    }
    if (argv.size() >= 7) {
        auto parsed = parse_uvec_device(argv[6]);
        if (!parsed.has_value()) {
            return ftcl::unexpected(parsed.error());
        }
        src_device = *parsed;
    }

    return uvec_command_manager().with_two(*dst_id, *src_id, [&](UVec<ftclFloat>& dst, UVec<ftclFloat>& src) {
        const std::size_t len = explicit_len.value_or(std::min(dst.size(), src.size()));
        auto copied = dst.copy_from(src, dst_device, src_device, len);
        if (!copied.has_value()) {
            return ftcl_err(copied.error());
        }
        return ftcl_ok(static_cast<ftclInt>(*copied));
    });
}

inline ftclResult cmd_uvec_to_list(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 4, "handle ?syncDevice?");
    if (!chk.has_value()) {
        return chk;
    }
    auto id = parse_uvec_handle(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }

    Device sync_device = Device::cpu();
    if (argv.size() == 4) {
        auto parsed_device = parse_uvec_device(argv[3]);
        if (!parsed_device.has_value()) {
            return ftcl::unexpected(parsed_device.error());
        }
        sync_device = *parsed_device;
    }

    return uvec_command_manager().with_vec(*id, [&](UVec<ftclFloat>& vec) {
        auto synced = vec.as_uptr(sync_device);
        if (!synced.has_value()) {
            return ftcl_err(synced.error());
        }
        return ftcl_ok(uvec_to_list_value(vec));
    });
}

inline ftclResult cmd_uvec_latest(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "handle");
    if (!chk.has_value()) {
        return chk;
    }
    auto id = parse_uvec_handle(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }

    return uvec_command_manager().with_vec(*id, [](UVec<ftclFloat>& vec) {
        return ftcl_ok(vec.latest_device().to_string());
    });
}

inline ftclResult cmd_uvec_valid(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 4, "handle device");
    if (!chk.has_value()) {
        return chk;
    }
    auto id = parse_uvec_handle(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }
    auto device = parse_uvec_device(argv[3]);
    if (!device.has_value()) {
        return ftcl::unexpected(device.error());
    }

    return uvec_command_manager().with_vec(*id, [&](UVec<ftclFloat>& vec) {
        return ftcl_ok(vec.valid_on(*device));
    });
}

inline ftclResult cmd_uvec_ptr(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 4, "handle ?device?");
    if (!chk.has_value()) {
        return chk;
    }
    auto id = parse_uvec_handle(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }

    Device device = Device::cpu();
    if (argv.size() == 4) {
        auto parsed_device = parse_uvec_device(argv[3]);
        if (!parsed_device.has_value()) {
            return ftcl::unexpected(parsed_device.error());
        }
        device = *parsed_device;
    }

    return uvec_command_manager().with_vec(*id, [&](UVec<ftclFloat>& vec) {
        auto ptr = vec.as_uptr(device);
        if (!ptr.has_value()) {
            return ftcl_err(ptr.error());
        }
        ftclList out;
        out.emplace_back(ptr->device().to_string());
        out.emplace_back(static_cast<ftclInt>(ptr->len()));
        out.emplace_back(uvec_pointer_address(ptr->get()));
        return ftcl_ok(Value::from_list(out));
    });
}

inline ftclResult cmd_uvec_mut_ptr(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 4, "handle ?device?");
    if (!chk.has_value()) {
        return chk;
    }
    auto id = parse_uvec_handle(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }

    Device device = Device::cpu();
    if (argv.size() == 4) {
        auto parsed_device = parse_uvec_device(argv[3]);
        if (!parsed_device.has_value()) {
            return ftcl::unexpected(parsed_device.error());
        }
        device = *parsed_device;
    }

    return uvec_command_manager().with_vec(*id, [&](UVec<ftclFloat>& vec) {
        auto ptr = vec.as_mut_uptr(device);
        if (!ptr.has_value()) {
            return ftcl_err(ptr.error());
        }
        ftclList out;
        out.emplace_back(ptr->device().to_string());
        out.emplace_back(static_cast<ftclInt>(ptr->len()));
        out.emplace_back(uvec_pointer_address(ptr->get()));
        return ftcl_ok(Value::from_list(out));
    });
}

inline ftclResult cmd_uvec(Interp* interp, ContextID context_id, const std::vector<Value>& argv) {
    std::vector<Subcommand> subs = {
        Subcommand("copy", cmd_uvec_copy),       Subcommand("create", cmd_uvec_create),
        Subcommand("destroy", cmd_uvec_destroy), Subcommand("fill", cmd_uvec_fill),
        Subcommand("filled", cmd_uvec_filled),   Subcommand("get", cmd_uvec_get),
        Subcommand("ids", cmd_uvec_ids),         Subcommand("latest", cmd_uvec_latest),
        Subcommand("len", cmd_uvec_len),         Subcommand("mut_ptr", cmd_uvec_mut_ptr),
        Subcommand("ptr", cmd_uvec_ptr),         Subcommand("set", cmd_uvec_set),
        Subcommand("to_list", cmd_uvec_to_list), Subcommand("valid", cmd_uvec_valid),
        Subcommand("zeroed", cmd_uvec_zeroed),
    };
    return interp->call_subcommand(context_id, argv, 1, subs);
}


// geom subcommands: script-level access to CPU geometry algorithms and UVec point batches.
inline ftcl::expected<geom::Point, Exception> parse_geom_point(const Value& value) {
    auto parts = value.as_list();
    if (!parts.has_value()) {
        return ftcl::unexpected(Exception::ftcl_err(Value(parts.error())));
    }
    if (parts->size() != 2) {
        return ftcl::unexpected(Exception::ftcl_err(Value("expected point as {x y}")));
    }

    auto x = parse_uvec_float((*parts)[0]);
    if (!x.has_value()) {
        return ftcl::unexpected(x.error());
    }
    auto y = parse_uvec_float((*parts)[1]);
    if (!y.has_value()) {
        return ftcl::unexpected(y.error());
    }
    return geom::Point{*x, *y};
}

inline ftcl::expected<std::vector<geom::Point>, Exception> parse_geom_points(const Value& value) {
    auto list = value.as_list();
    if (!list.has_value()) {
        return ftcl::unexpected(Exception::ftcl_err(Value(list.error())));
    }

    std::vector<geom::Point> points;
    points.reserve(list->size());
    for (const auto& item : *list) {
        auto point = parse_geom_point(item);
        if (!point.has_value()) {
            return ftcl::unexpected(point.error());
        }
        points.push_back(*point);
    }
    return points;
}

inline Value geom_point_to_value(const geom::Point& point) {
    ftclList out;
    out.emplace_back(point.x);
    out.emplace_back(point.y);
    return Value::from_list(out);
}

inline Value geom_points_to_value(const std::vector<geom::Point>& points) {
    ftclList out;
    out.reserve(points.size());
    for (const auto& point : points) {
        out.push_back(geom_point_to_value(point));
    }
    return Value::from_list(out);
}

inline std::vector<ftclFloat> flatten_geom_points(const std::vector<geom::Point>& points) {
    std::vector<ftclFloat> out;
    out.reserve(points.size() * 2);
    for (const auto& point : points) {
        out.push_back(point.x);
        out.push_back(point.y);
    }
    return out;
}

inline ftclResult cmd_geom_distance(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 4, "pointA pointB");
    if (!chk.has_value()) {
        return chk;
    }
    auto a = parse_geom_point(argv[2]);
    if (!a.has_value()) {
        return ftcl::unexpected(a.error());
    }
    auto b = parse_geom_point(argv[3]);
    if (!b.has_value()) {
        return ftcl::unexpected(b.error());
    }
    return ftcl_ok(geom::distance(*a, *b));
}

inline ftclResult cmd_geom_distance2(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 4, "pointA pointB");
    if (!chk.has_value()) {
        return chk;
    }
    auto a = parse_geom_point(argv[2]);
    if (!a.has_value()) {
        return ftcl::unexpected(a.error());
    }
    auto b = parse_geom_point(argv[3]);
    if (!b.has_value()) {
        return ftcl::unexpected(b.error());
    }
    return ftcl_ok(geom::distance2(*a, *b));
}

inline ftclResult cmd_geom_orient(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 5, 5, "pointA pointB pointC");
    if (!chk.has_value()) {
        return chk;
    }
    auto a = parse_geom_point(argv[2]);
    auto b = parse_geom_point(argv[3]);
    auto c = parse_geom_point(argv[4]);
    if (!a.has_value()) {
        return ftcl::unexpected(a.error());
    }
    if (!b.has_value()) {
        return ftcl::unexpected(b.error());
    }
    if (!c.has_value()) {
        return ftcl::unexpected(c.error());
    }
    return ftcl_ok(static_cast<ftclInt>(geom::orientation(*a, *b, *c)));
}

inline ftclResult cmd_geom_on_segment(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 5, 5, "pointA pointB pointP");
    if (!chk.has_value()) {
        return chk;
    }
    auto a = parse_geom_point(argv[2]);
    auto b = parse_geom_point(argv[3]);
    auto p = parse_geom_point(argv[4]);
    if (!a.has_value()) {
        return ftcl::unexpected(a.error());
    }
    if (!b.has_value()) {
        return ftcl::unexpected(b.error());
    }
    if (!p.has_value()) {
        return ftcl::unexpected(p.error());
    }
    return ftcl_ok(geom::on_segment(*a, *b, *p));
}

inline ftclResult cmd_geom_segment_intersect(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 6, 6, "pointA pointB pointC pointD");
    if (!chk.has_value()) {
        return chk;
    }
    auto a = parse_geom_point(argv[2]);
    auto b = parse_geom_point(argv[3]);
    auto c = parse_geom_point(argv[4]);
    auto d = parse_geom_point(argv[5]);
    if (!a.has_value()) {
        return ftcl::unexpected(a.error());
    }
    if (!b.has_value()) {
        return ftcl::unexpected(b.error());
    }
    if (!c.has_value()) {
        return ftcl::unexpected(c.error());
    }
    if (!d.has_value()) {
        return ftcl::unexpected(d.error());
    }
    return ftcl_ok(geom::segments_intersect(*a, *b, *c, *d));
}

inline ftclResult cmd_geom_line_intersection(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 6, 6, "pointA pointB pointC pointD");
    if (!chk.has_value()) {
        return chk;
    }
    auto a = parse_geom_point(argv[2]);
    auto b = parse_geom_point(argv[3]);
    auto c = parse_geom_point(argv[4]);
    auto d = parse_geom_point(argv[5]);
    if (!a.has_value()) {
        return ftcl::unexpected(a.error());
    }
    if (!b.has_value()) {
        return ftcl::unexpected(b.error());
    }
    if (!c.has_value()) {
        return ftcl::unexpected(c.error());
    }
    if (!d.has_value()) {
        return ftcl::unexpected(d.error());
    }

    auto point = geom::line_intersection(*a, *b, *c, *d);
    if (!point.has_value()) {
        return ftcl_ok(Value::empty());
    }
    return ftcl_ok(geom_point_to_value(*point));
}

inline ftclResult cmd_geom_point_line_distance(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 5, 5, "pointP pointA pointB");
    if (!chk.has_value()) {
        return chk;
    }
    auto p = parse_geom_point(argv[2]);
    auto a = parse_geom_point(argv[3]);
    auto b = parse_geom_point(argv[4]);
    if (!p.has_value()) {
        return ftcl::unexpected(p.error());
    }
    if (!a.has_value()) {
        return ftcl::unexpected(a.error());
    }
    if (!b.has_value()) {
        return ftcl::unexpected(b.error());
    }
    return ftcl_ok(geom::point_line_distance(*p, *a, *b));
}

inline ftclResult cmd_geom_point_segment_distance(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 5, 5, "pointP pointA pointB");
    if (!chk.has_value()) {
        return chk;
    }
    auto p = parse_geom_point(argv[2]);
    auto a = parse_geom_point(argv[3]);
    auto b = parse_geom_point(argv[4]);
    if (!p.has_value()) {
        return ftcl::unexpected(p.error());
    }
    if (!a.has_value()) {
        return ftcl::unexpected(a.error());
    }
    if (!b.has_value()) {
        return ftcl::unexpected(b.error());
    }
    return ftcl_ok(geom::point_segment_distance(*p, *a, *b));
}

inline ftclResult cmd_geom_polygon_area(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "points");
    if (!chk.has_value()) {
        return chk;
    }
    auto points = parse_geom_points(argv[2]);
    if (!points.has_value()) {
        return ftcl::unexpected(points.error());
    }
    return ftcl_ok(geom::polygon_area(*points));
}

inline ftclResult cmd_geom_polygon_signed_area(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "points");
    if (!chk.has_value()) {
        return chk;
    }
    auto points = parse_geom_points(argv[2]);
    if (!points.has_value()) {
        return ftcl::unexpected(points.error());
    }
    return ftcl_ok(geom::polygon_signed_area(*points));
}

inline ftclResult cmd_geom_polygon_perimeter(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "points");
    if (!chk.has_value()) {
        return chk;
    }
    auto points = parse_geom_points(argv[2]);
    if (!points.has_value()) {
        return ftcl::unexpected(points.error());
    }
    return ftcl_ok(geom::polygon_perimeter(*points));
}

inline ftclResult cmd_geom_point_in_polygon(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 4, "point polygon");
    if (!chk.has_value()) {
        return chk;
    }
    auto point = parse_geom_point(argv[2]);
    if (!point.has_value()) {
        return ftcl::unexpected(point.error());
    }
    auto polygon = parse_geom_points(argv[3]);
    if (!polygon.has_value()) {
        return ftcl::unexpected(polygon.error());
    }
    return ftcl_ok(geom::point_location_to_string(geom::point_in_polygon(*point, *polygon)));
}

inline ftclResult cmd_geom_convex_hull(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "points");
    if (!chk.has_value()) {
        return chk;
    }
    auto points = parse_geom_points(argv[2]);
    if (!points.has_value()) {
        return ftcl::unexpected(points.error());
    }
    return ftcl_ok(geom_points_to_value(geom::convex_hull(std::move(*points))));
}

inline ftclResult cmd_geom_bbox(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "points");
    if (!chk.has_value()) {
        return chk;
    }
    auto points = parse_geom_points(argv[2]);
    if (!points.has_value()) {
        return ftcl::unexpected(points.error());
    }
    auto box = geom::bounding_box(*points);
    if (!box.has_value()) {
        return ftcl_err("cannot compute bounding box of empty point set");
    }
    ftclList out;
    out.push_back(geom_point_to_value(box->min));
    out.push_back(geom_point_to_value(box->max));
    return ftcl_ok(Value::from_list(out));
}

inline ftclResult cmd_geom_closest_pair_distance(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 3, "points");
    if (!chk.has_value()) {
        return chk;
    }
    auto points = parse_geom_points(argv[2]);
    if (!points.has_value()) {
        return ftcl::unexpected(points.error());
    }
    auto best = geom::closest_pair_distance(*points);
    if (!best.has_value()) {
        return ftcl_err("closest_pair_distance requires at least two points");
    }
    return ftcl_ok(*best);
}

inline ftclResult cmd_geom_uvec_points(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 4, "points ?device?");
    if (!chk.has_value()) {
        return chk;
    }
    auto points = parse_geom_points(argv[2]);
    if (!points.has_value()) {
        return ftcl::unexpected(points.error());
    }

    Device device = Device::cpu();
    if (argv.size() == 4) {
        auto parsed_device = parse_uvec_device(argv[3]);
        if (!parsed_device.has_value()) {
            return ftcl::unexpected(parsed_device.error());
        }
        device = *parsed_device;
    }

    UVec<ftclFloat> vec = UVec<ftclFloat>::from_cpu(flatten_geom_points(*points));
    if (!device.is_cpu()) {
        auto ptr = vec.as_mut_uptr(device);
        if (!ptr.has_value()) {
            return ftcl_err(ptr.error());
        }
    }
    return ftcl_ok(uvec_command_manager().create(std::move(vec)));
}


inline ftcl::expected<geom::Segment, Exception> parse_geom_segment(const Value& value) {
    auto parts = value.as_list();
    if (!parts.has_value()) {
        return ftcl::unexpected(Exception::ftcl_err(Value(parts.error())));
    }

    if (parts->size() == 2) {
        auto a = parse_geom_point((*parts)[0]);
        auto b = parse_geom_point((*parts)[1]);
        if (!a.has_value()) {
            return ftcl::unexpected(a.error());
        }
        if (!b.has_value()) {
            return ftcl::unexpected(b.error());
        }
        return geom::Segment{*a, *b};
    }

    if (parts->size() == 4) {
        auto x1 = parse_uvec_float((*parts)[0]);
        auto y1 = parse_uvec_float((*parts)[1]);
        auto x2 = parse_uvec_float((*parts)[2]);
        auto y2 = parse_uvec_float((*parts)[3]);
        if (!x1.has_value()) return ftcl::unexpected(x1.error());
        if (!y1.has_value()) return ftcl::unexpected(y1.error());
        if (!x2.has_value()) return ftcl::unexpected(x2.error());
        if (!y2.has_value()) return ftcl::unexpected(y2.error());
        return geom::Segment{geom::Point{*x1, *y1}, geom::Point{*x2, *y2}};
    }

    return ftcl::unexpected(Exception::ftcl_err(Value("expected segment as {{x1 y1} {x2 y2}} or {x1 y1 x2 y2}")));
}

inline ftcl::expected<std::vector<geom::Segment>, Exception> parse_geom_segments(const Value& value) {
    auto list = value.as_list();
    if (!list.has_value()) {
        return ftcl::unexpected(Exception::ftcl_err(Value(list.error())));
    }

    std::vector<geom::Segment> segments;
    segments.reserve(list->size());
    for (const auto& item : *list) {
        auto segment = parse_geom_segment(item);
        if (!segment.has_value()) {
            return ftcl::unexpected(segment.error());
        }
        segments.push_back(*segment);
    }
    return segments;
}

inline ftcl::expected<geom::BoundingBox, Exception> parse_geom_aabb(const Value& value) {
    auto parts = value.as_list();
    if (!parts.has_value()) {
        return ftcl::unexpected(Exception::ftcl_err(Value(parts.error())));
    }

    geom::Point lo;
    geom::Point hi;
    if (parts->size() == 2) {
        auto a = parse_geom_point((*parts)[0]);
        auto b = parse_geom_point((*parts)[1]);
        if (!a.has_value()) return ftcl::unexpected(a.error());
        if (!b.has_value()) return ftcl::unexpected(b.error());
        lo = *a;
        hi = *b;
    } else if (parts->size() == 4) {
        auto x1 = parse_uvec_float((*parts)[0]);
        auto y1 = parse_uvec_float((*parts)[1]);
        auto x2 = parse_uvec_float((*parts)[2]);
        auto y2 = parse_uvec_float((*parts)[3]);
        if (!x1.has_value()) return ftcl::unexpected(x1.error());
        if (!y1.has_value()) return ftcl::unexpected(y1.error());
        if (!x2.has_value()) return ftcl::unexpected(x2.error());
        if (!y2.has_value()) return ftcl::unexpected(y2.error());
        lo = geom::Point{*x1, *y1};
        hi = geom::Point{*x2, *y2};
    } else {
        return ftcl::unexpected(Exception::ftcl_err(Value("expected aabb as {{minX minY} {maxX maxY}} or {minX minY maxX maxY}")));
    }

    return geom::BoundingBox{geom::Point{std::min(lo.x, hi.x), std::min(lo.y, hi.y)},
                             geom::Point{std::max(lo.x, hi.x), std::max(lo.y, hi.y)}};
}

inline ftcl::expected<std::vector<geom::BoundingBox>, Exception> parse_geom_aabbs(const Value& value) {
    auto list = value.as_list();
    if (!list.has_value()) {
        return ftcl::unexpected(Exception::ftcl_err(Value(list.error())));
    }

    std::vector<geom::BoundingBox> boxes;
    boxes.reserve(list->size());
    for (const auto& item : *list) {
        auto box = parse_geom_aabb(item);
        if (!box.has_value()) {
            return ftcl::unexpected(box.error());
        }
        boxes.push_back(*box);
    }
    return boxes;
}

inline std::vector<ftclFloat> flatten_geom_segments(const std::vector<geom::Segment>& segments) {
    std::vector<ftclFloat> out;
    out.reserve(segments.size() * 4);
    for (const auto& segment : segments) {
        out.push_back(segment.a.x);
        out.push_back(segment.a.y);
        out.push_back(segment.b.x);
        out.push_back(segment.b.y);
    }
    return out;
}

inline std::vector<ftclFloat> flatten_geom_aabbs(const std::vector<geom::BoundingBox>& boxes) {
    std::vector<ftclFloat> out;
    out.reserve(boxes.size() * 4);
    for (const auto& box : boxes) {
        out.push_back(box.min.x);
        out.push_back(box.min.y);
        out.push_back(box.max.x);
        out.push_back(box.max.y);
    }
    return out;
}

inline ftcl::expected<std::vector<geom::Point>, Exception> geom_points_from_flat(const std::vector<ftclFloat>& flat,
                                                                                const std::string& what) {
    if (flat.size() % 2 != 0) {
        return ftcl::unexpected(Exception::ftcl_err(Value(what + " must contain an even number of coordinates")));
    }
    std::vector<geom::Point> points;
    points.reserve(flat.size() / 2);
    for (std::size_t i = 0; i < flat.size(); i += 2) {
        points.push_back(geom::Point{flat[i], flat[i + 1]});
    }
    return points;
}

inline ftcl::expected<std::vector<geom::Segment>, Exception> geom_segments_from_flat(const std::vector<ftclFloat>& flat,
                                                                                    const std::string& what) {
    if (flat.size() % 4 != 0) {
        return ftcl::unexpected(Exception::ftcl_err(Value(what + " must contain 4 coordinates per segment")));
    }
    std::vector<geom::Segment> segments;
    segments.reserve(flat.size() / 4);
    for (std::size_t i = 0; i < flat.size(); i += 4) {
        segments.push_back(geom::Segment{geom::Point{flat[i], flat[i + 1]}, geom::Point{flat[i + 2], flat[i + 3]}});
    }
    return segments;
}

inline ftcl::expected<std::vector<geom::BoundingBox>, Exception> geom_aabbs_from_flat(const std::vector<ftclFloat>& flat,
                                                                                     const std::string& what) {
    if (flat.size() % 4 != 0) {
        return ftcl::unexpected(Exception::ftcl_err(Value(what + " must contain 4 coordinates per aabb")));
    }
    std::vector<geom::BoundingBox> boxes;
    boxes.reserve(flat.size() / 4);
    for (std::size_t i = 0; i < flat.size(); i += 4) {
        boxes.push_back(geom::BoundingBox{geom::Point{std::min(flat[i], flat[i + 2]), std::min(flat[i + 1], flat[i + 3])},
                                          geom::Point{std::max(flat[i], flat[i + 2]), std::max(flat[i + 1], flat[i + 3])}});
    }
    return boxes;
}

inline ftclResult geom_load_uvec_cpu(ftclInt id, Device device, std::vector<ftclFloat>& out) {
    return uvec_command_manager().with_vec(id, [&](UVec<ftclFloat>& vec) {
        auto synced = vec.as_uptr(device);
        if (!synced.has_value()) {
            return ftcl_err(synced.error());
        }
        const auto& cpu = vec.cpu_vector();
        out.assign(cpu.begin(), cpu.end());
        return ftcl_ok();
    });
}

inline ftclResult geom_create_uvec_from_cpu(std::vector<ftclFloat> values, Device device) {
    UVec<ftclFloat> out = UVec<ftclFloat>::from_cpu(std::move(values));
    if (!device.is_cpu()) {
        auto ptr = out.as_mut_uptr(device);
        if (!ptr.has_value()) {
            return ftcl_err(ptr.error());
        }
    }
    return ftcl_ok(uvec_command_manager().create(std::move(out)));
}

inline ftcl::expected<Device, Exception> geom_parse_optional_device(const std::vector<Value>& argv,
                                                                    std::size_t index,
                                                                    Device fallback = Device::cpu()) {
    if (argv.size() <= index) {
        return fallback;
    }
    return parse_uvec_device(argv[index]);
}

inline ftcl::expected<std::size_t, Exception> geom_parse_positive_size(const Value& value, const std::string& what) {
    auto parsed = parse_uvec_index(value, what);
    if (!parsed.has_value()) {
        return ftcl::unexpected(parsed.error());
    }
    if (*parsed == 0) {
        return ftcl::unexpected(Exception::ftcl_err(Value(what + " must be positive")));
    }
    return *parsed;
}

inline ftclResult cmd_geom_uvec_segments(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 4, "segments ?device?");
    if (!chk.has_value()) return chk;
    auto segments = parse_geom_segments(argv[2]);
    if (!segments.has_value()) return ftcl::unexpected(segments.error());
    auto device = geom_parse_optional_device(argv, 3);
    if (!device.has_value()) return ftcl::unexpected(device.error());
    return geom_create_uvec_from_cpu(flatten_geom_segments(*segments), *device);
}

inline ftclResult cmd_geom_uvec_aabbs(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 4, "aabbs ?device?");
    if (!chk.has_value()) return chk;
    auto boxes = parse_geom_aabbs(argv[2]);
    if (!boxes.has_value()) return ftcl::unexpected(boxes.error());
    auto device = geom_parse_optional_device(argv, 3);
    if (!device.has_value()) return ftcl::unexpected(device.error());
    return geom_create_uvec_from_cpu(flatten_geom_aabbs(*boxes), *device);
}

inline ftclResult cmd_geom_batch_distance_matrix(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "pointsAHandle pointsBHandle ?device?");
    if (!chk.has_value()) return chk;
    auto lhs_id = parse_uvec_handle(argv[2]);
    auto rhs_id = parse_uvec_handle(argv[3]);
    if (!lhs_id.has_value()) return ftcl::unexpected(lhs_id.error());
    if (!rhs_id.has_value()) return ftcl::unexpected(rhs_id.error());
    auto device = geom_parse_optional_device(argv, 4);
    if (!device.has_value()) return ftcl::unexpected(device.error());

#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_two(*lhs_id, *rhs_id, [&](UVec<ftclFloat>& lhs, UVec<ftclFloat>& rhs) {
            if (lhs.size() % 2 != 0 || rhs.size() % 2 != 0) return ftcl_err("geom point UVec must contain an even number of coordinates");
            const std::size_t lhs_count = lhs.size() / 2;
            const std::size_t rhs_count = rhs.size() / 2;
            auto lhs_ptr = lhs.as_uptr(*device);
            auto rhs_ptr = rhs.as_uptr(*device);
            if (!lhs_ptr.has_value()) return ftcl_err(lhs_ptr.error());
            if (!rhs_ptr.has_value()) return ftcl_err(rhs_ptr.error());
            auto values = UVec<ftclFloat>::uninitialized(lhs_count * rhs_count, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::batch_distance_matrix_cuda(*dst, *lhs_ptr, *rhs_ptr, lhs_count, rhs_count);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif

    std::vector<ftclFloat> lhs_flat, rhs_flat;
    auto lhs_load = geom_load_uvec_cpu(*lhs_id, *device, lhs_flat);
    if (!lhs_load.has_value()) return lhs_load;
    auto rhs_load = geom_load_uvec_cpu(*rhs_id, *device, rhs_flat);
    if (!rhs_load.has_value()) return rhs_load;
    auto lhs = geom_points_from_flat(lhs_flat, "pointsA UVec");
    auto rhs = geom_points_from_flat(rhs_flat, "pointsB UVec");
    if (!lhs.has_value()) return ftcl::unexpected(lhs.error());
    if (!rhs.has_value()) return ftcl::unexpected(rhs.error());
    return geom_create_uvec_from_cpu(geom::batch_distance_matrix(*lhs, *rhs), *device);
}

inline ftclResult cmd_geom_batch_point_in_polygon(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "pointsHandle polygon ?device?");
    if (!chk.has_value()) return chk;
    auto points_id = parse_uvec_handle(argv[2]);
    if (!points_id.has_value()) return ftcl::unexpected(points_id.error());
    auto polygon = parse_geom_points(argv[3]);
    if (!polygon.has_value()) return ftcl::unexpected(polygon.error());
    auto device = geom_parse_optional_device(argv, 4);
    if (!device.has_value()) return ftcl::unexpected(device.error());

#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        UVec<ftclFloat> polygon_vec = UVec<ftclFloat>::from_cpu(flatten_geom_points(*polygon));
        auto poly_mut = polygon_vec.as_mut_uptr(*device);
        if (!poly_mut.has_value()) return ftcl_err(poly_mut.error());
        auto poly_ptr = polygon_vec.as_uptr(*device);
        if (!poly_ptr.has_value()) return ftcl_err(poly_ptr.error());
        auto ran = uvec_command_manager().with_vec(*points_id, [&](UVec<ftclFloat>& points_vec) {
            if (points_vec.size() % 2 != 0) return ftcl_err("points UVec must contain an even number of coordinates");
            const std::size_t point_count = points_vec.size() / 2;
            auto src = points_vec.as_uptr(*device);
            if (!src.has_value()) return ftcl_err(src.error());
            auto values = UVec<ftclFloat>::uninitialized(point_count, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::batch_point_in_polygon_cuda(*dst, *src, *poly_ptr, point_count, polygon->size());
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif

    std::vector<ftclFloat> points_flat;
    auto load = geom_load_uvec_cpu(*points_id, *device, points_flat);
    if (!load.has_value()) return load;
    auto points = geom_points_from_flat(points_flat, "points UVec");
    if (!points.has_value()) return ftcl::unexpected(points.error());
    return geom_create_uvec_from_cpu(geom::batch_point_in_polygon(*points, *polygon), *device);
}

inline ftclResult cmd_geom_batch_segment_intersect(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "segmentsAHandle segmentsBHandle ?device?");
    if (!chk.has_value()) return chk;
    auto lhs_id = parse_uvec_handle(argv[2]);
    auto rhs_id = parse_uvec_handle(argv[3]);
    if (!lhs_id.has_value()) return ftcl::unexpected(lhs_id.error());
    if (!rhs_id.has_value()) return ftcl::unexpected(rhs_id.error());
    auto device = geom_parse_optional_device(argv, 4);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_two(*lhs_id, *rhs_id, [&](UVec<ftclFloat>& lhs, UVec<ftclFloat>& rhs) {
            if (lhs.size() % 4 != 0 || rhs.size() % 4 != 0) return ftcl_err("segment UVec must contain 4 coordinates per segment");
            if (lhs.size() != rhs.size()) return ftcl_err("segment UVec inputs must have the same number of segments");
            const std::size_t count = lhs.size() / 4;
            auto lhs_ptr = lhs.as_uptr(*device);
            auto rhs_ptr = rhs.as_uptr(*device);
            if (!lhs_ptr.has_value()) return ftcl_err(lhs_ptr.error());
            if (!rhs_ptr.has_value()) return ftcl_err(rhs_ptr.error());
            auto values = UVec<ftclFloat>::uninitialized(count, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::batch_segment_intersect_cuda(*dst, *lhs_ptr, *rhs_ptr, count);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif
    std::vector<ftclFloat> lhs_flat, rhs_flat;
    auto lhs_load = geom_load_uvec_cpu(*lhs_id, *device, lhs_flat);
    if (!lhs_load.has_value()) return lhs_load;
    auto rhs_load = geom_load_uvec_cpu(*rhs_id, *device, rhs_flat);
    if (!rhs_load.has_value()) return rhs_load;
    auto lhs = geom_segments_from_flat(lhs_flat, "segmentsA UVec");
    auto rhs = geom_segments_from_flat(rhs_flat, "segmentsB UVec");
    if (!lhs.has_value()) return ftcl::unexpected(lhs.error());
    if (!rhs.has_value()) return ftcl::unexpected(rhs.error());
    if (lhs->size() != rhs->size()) return ftcl_err("segment UVec inputs must have the same number of segments");
    return geom_create_uvec_from_cpu(geom::batch_segment_intersect(*lhs, *rhs), *device);
}

inline ftclResult cmd_geom_batch_point_segment_distance(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "pointsHandle segmentsHandle ?device?");
    if (!chk.has_value()) return chk;
    auto points_id = parse_uvec_handle(argv[2]);
    auto segments_id = parse_uvec_handle(argv[3]);
    if (!points_id.has_value()) return ftcl::unexpected(points_id.error());
    if (!segments_id.has_value()) return ftcl::unexpected(segments_id.error());
    auto device = geom_parse_optional_device(argv, 4);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_two(*points_id, *segments_id, [&](UVec<ftclFloat>& points, UVec<ftclFloat>& segments) {
            if (points.size() % 2 != 0) return ftcl_err("points UVec must contain an even number of coordinates");
            if (segments.size() % 4 != 0) return ftcl_err("segments UVec must contain 4 coordinates per segment");
            if (points.size() / 2 != segments.size() / 4) return ftcl_err("points and segments UVec inputs must have the same count");
            const std::size_t count = points.size() / 2;
            auto points_ptr = points.as_uptr(*device);
            auto segments_ptr = segments.as_uptr(*device);
            if (!points_ptr.has_value()) return ftcl_err(points_ptr.error());
            if (!segments_ptr.has_value()) return ftcl_err(segments_ptr.error());
            auto values = UVec<ftclFloat>::uninitialized(count, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::batch_point_segment_distance_cuda(*dst, *points_ptr, *segments_ptr, count);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif
    std::vector<ftclFloat> points_flat, segments_flat;
    auto p_load = geom_load_uvec_cpu(*points_id, *device, points_flat);
    if (!p_load.has_value()) return p_load;
    auto s_load = geom_load_uvec_cpu(*segments_id, *device, segments_flat);
    if (!s_load.has_value()) return s_load;
    auto points = geom_points_from_flat(points_flat, "points UVec");
    auto segments = geom_segments_from_flat(segments_flat, "segments UVec");
    if (!points.has_value()) return ftcl::unexpected(points.error());
    if (!segments.has_value()) return ftcl::unexpected(segments.error());
    if (points->size() != segments->size()) return ftcl_err("points and segments UVec inputs must have the same count");
    return geom_create_uvec_from_cpu(geom::batch_point_segment_distance(*points, *segments), *device);
}

inline ftclResult cmd_geom_nearest_point(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "datasetHandle queryHandle ?device?");
    if (!chk.has_value()) return chk;
    auto dataset_id = parse_uvec_handle(argv[2]);
    auto query_id = parse_uvec_handle(argv[3]);
    if (!dataset_id.has_value()) return ftcl::unexpected(dataset_id.error());
    if (!query_id.has_value()) return ftcl::unexpected(query_id.error());
    auto device = geom_parse_optional_device(argv, 4);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_two(*dataset_id, *query_id, [&](UVec<ftclFloat>& dataset, UVec<ftclFloat>& queries) {
            if (dataset.size() % 2 != 0 || queries.size() % 2 != 0) return ftcl_err("point UVec must contain an even number of coordinates");
            if (dataset.size() == 0) return ftcl_err("nearest_point requires a non-empty dataset");
            const std::size_t dataset_count = dataset.size() / 2;
            const std::size_t query_count = queries.size() / 2;
            auto dataset_ptr = dataset.as_uptr(*device);
            auto query_ptr = queries.as_uptr(*device);
            if (!dataset_ptr.has_value()) return ftcl_err(dataset_ptr.error());
            if (!query_ptr.has_value()) return ftcl_err(query_ptr.error());
            auto values = UVec<ftclFloat>::uninitialized(query_count * 2, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::nearest_point_cuda(*dst, *dataset_ptr, *query_ptr, dataset_count, query_count);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif
    std::vector<ftclFloat> dataset_flat, query_flat;
    auto d_load = geom_load_uvec_cpu(*dataset_id, *device, dataset_flat);
    if (!d_load.has_value()) return d_load;
    auto q_load = geom_load_uvec_cpu(*query_id, *device, query_flat);
    if (!q_load.has_value()) return q_load;
    auto dataset = geom_points_from_flat(dataset_flat, "dataset UVec");
    auto queries = geom_points_from_flat(query_flat, "query UVec");
    if (!dataset.has_value()) return ftcl::unexpected(dataset.error());
    if (!queries.has_value()) return ftcl::unexpected(queries.error());
    if (dataset->empty()) return ftcl_err("nearest_point requires a non-empty dataset");
    return geom_create_uvec_from_cpu(geom::nearest_point(*dataset, *queries), *device);
}

inline ftclResult cmd_geom_k_nearest(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 5, 6, "datasetHandle queryHandle k ?device?");
    if (!chk.has_value()) return chk;
    auto dataset_id = parse_uvec_handle(argv[2]);
    auto query_id = parse_uvec_handle(argv[3]);
    auto k = geom_parse_positive_size(argv[4], "k");
    if (!dataset_id.has_value()) return ftcl::unexpected(dataset_id.error());
    if (!query_id.has_value()) return ftcl::unexpected(query_id.error());
    if (!k.has_value()) return ftcl::unexpected(k.error());
    auto device = geom_parse_optional_device(argv, 5);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_two(*dataset_id, *query_id, [&](UVec<ftclFloat>& dataset, UVec<ftclFloat>& queries) {
            if (dataset.size() % 2 != 0 || queries.size() % 2 != 0) return ftcl_err("point UVec must contain an even number of coordinates");
            if (dataset.size() == 0) return ftcl_err("k_nearest requires a non-empty dataset");
            const std::size_t dataset_count = dataset.size() / 2;
            const std::size_t query_count = queries.size() / 2;
            auto dataset_ptr = dataset.as_uptr(*device);
            auto query_ptr = queries.as_uptr(*device);
            if (!dataset_ptr.has_value()) return ftcl_err(dataset_ptr.error());
            if (!query_ptr.has_value()) return ftcl_err(query_ptr.error());
            auto values = UVec<ftclFloat>::uninitialized(query_count * (*k) * 2, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::k_nearest_points_cuda(*dst, *dataset_ptr, *query_ptr, dataset_count, query_count, *k);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif
    std::vector<ftclFloat> dataset_flat, query_flat;
    auto d_load = geom_load_uvec_cpu(*dataset_id, *device, dataset_flat);
    if (!d_load.has_value()) return d_load;
    auto q_load = geom_load_uvec_cpu(*query_id, *device, query_flat);
    if (!q_load.has_value()) return q_load;
    auto dataset = geom_points_from_flat(dataset_flat, "dataset UVec");
    auto queries = geom_points_from_flat(query_flat, "query UVec");
    if (!dataset.has_value()) return ftcl::unexpected(dataset.error());
    if (!queries.has_value()) return ftcl::unexpected(queries.error());
    if (dataset->empty()) return ftcl_err("k_nearest requires a non-empty dataset");
    return geom_create_uvec_from_cpu(geom::k_nearest_points(*dataset, *queries, *k), *device);
}

inline ftclResult cmd_geom_batch_distance(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "pointsHandle queryPoint ?device?");
    if (!chk.has_value()) {
        return chk;
    }
    auto id = parse_uvec_handle(argv[2]);
    if (!id.has_value()) {
        return ftcl::unexpected(id.error());
    }
    auto query = parse_geom_point(argv[3]);
    if (!query.has_value()) {
        return ftcl::unexpected(query.error());
    }

    Device device = Device::cpu();
    if (argv.size() == 5) {
        auto parsed_device = parse_uvec_device(argv[4]);
        if (!parsed_device.has_value()) {
            return ftcl::unexpected(parsed_device.error());
        }
        device = *parsed_device;
    }

    std::vector<ftclFloat> flat;
    UVec<ftclFloat> out;
    bool produced_on_device = false;

    auto loaded = uvec_command_manager().with_vec(*id, [&](UVec<ftclFloat>& vec) {
        if (vec.size() % 2 != 0) {
            return ftcl_err("geom point UVec must contain an even number of coordinates");
        }

#ifdef FTCL_GEOMETRY_CUDA
        if (device.is_cuda()) {
            const std::size_t point_count = vec.size() / 2;
            auto src = vec.as_uptr(device);
            if (!src.has_value()) {
                return ftcl_err(src.error());
            }

            auto distances = UVec<ftclFloat>::uninitialized(point_count, device);
            if (!distances.has_value()) {
                return ftcl_err(distances.error());
            }
            out = std::move(*distances);

            auto dst = out.as_mut_uptr(device);
            if (!dst.has_value()) {
                return ftcl_err(dst.error());
            }

            auto launched = geom::batch_distance_cuda(*dst, *src, *query, point_count);
            if (!launched.has_value()) {
                return ftcl_err(launched.error());
            }

            produced_on_device = true;
            return ftcl_ok();
        }
#endif

        auto synced = vec.as_uptr(device);
        if (!synced.has_value()) {
            return ftcl_err(synced.error());
        }
        const auto& cpu = vec.cpu_vector();
        flat.assign(cpu.begin(), cpu.end());
        return ftcl_ok();
    });
    if (!loaded.has_value()) {
        return loaded;
    }

    if (!produced_on_device) {
        std::vector<ftclFloat> distances;
        distances.reserve(flat.size() / 2);
        for (std::size_t i = 0; i < flat.size(); i += 2) {
            distances.push_back(geom::distance(geom::Point{flat[i], flat[i + 1]}, *query));
        }

        out = UVec<ftclFloat>::from_cpu(std::move(distances));
        if (!device.is_cpu()) {
            auto ptr = out.as_mut_uptr(device);
            if (!ptr.has_value()) {
                return ftcl_err(ptr.error());
            }
        }
    }

    return ftcl_ok(uvec_command_manager().create(std::move(out)));
}


inline ftclResult cmd_geom_range_count_circle(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 5, 6, "pointsHandle centersHandle radius ?device?");
    if (!chk.has_value()) return chk;
    auto points_id = parse_uvec_handle(argv[2]);
    auto centers_id = parse_uvec_handle(argv[3]);
    auto radius = parse_uvec_float(argv[4]);
    if (!points_id.has_value()) return ftcl::unexpected(points_id.error());
    if (!centers_id.has_value()) return ftcl::unexpected(centers_id.error());
    if (!radius.has_value()) return ftcl::unexpected(radius.error());
    if (*radius < 0.0) return ftcl_err("radius must be non-negative");
    auto device = geom_parse_optional_device(argv, 5);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_two(*points_id, *centers_id, [&](UVec<ftclFloat>& points, UVec<ftclFloat>& centers) {
            if (points.size() % 2 != 0 || centers.size() % 2 != 0) return ftcl_err("point UVec must contain an even number of coordinates");
            const std::size_t point_count = points.size() / 2;
            const std::size_t center_count = centers.size() / 2;
            auto points_ptr = points.as_uptr(*device);
            auto centers_ptr = centers.as_uptr(*device);
            if (!points_ptr.has_value()) return ftcl_err(points_ptr.error());
            if (!centers_ptr.has_value()) return ftcl_err(centers_ptr.error());
            auto values = UVec<ftclFloat>::uninitialized(center_count, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::range_count_circle_cuda(*dst, *points_ptr, *centers_ptr, point_count, center_count, *radius);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif
    std::vector<ftclFloat> points_flat, centers_flat;
    auto p_load = geom_load_uvec_cpu(*points_id, *device, points_flat);
    if (!p_load.has_value()) return p_load;
    auto c_load = geom_load_uvec_cpu(*centers_id, *device, centers_flat);
    if (!c_load.has_value()) return c_load;
    auto points = geom_points_from_flat(points_flat, "points UVec");
    auto centers = geom_points_from_flat(centers_flat, "centers UVec");
    if (!points.has_value()) return ftcl::unexpected(points.error());
    if (!centers.has_value()) return ftcl::unexpected(centers.error());
    return geom_create_uvec_from_cpu(geom::range_count_circle(*points, *centers, *radius), *device);
}

inline ftclResult cmd_geom_range_count_rect(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "pointsHandle rectsHandle ?device?");
    if (!chk.has_value()) return chk;
    auto points_id = parse_uvec_handle(argv[2]);
    auto rects_id = parse_uvec_handle(argv[3]);
    if (!points_id.has_value()) return ftcl::unexpected(points_id.error());
    if (!rects_id.has_value()) return ftcl::unexpected(rects_id.error());
    auto device = geom_parse_optional_device(argv, 4);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_two(*points_id, *rects_id, [&](UVec<ftclFloat>& points, UVec<ftclFloat>& rects) {
            if (points.size() % 2 != 0) return ftcl_err("points UVec must contain an even number of coordinates");
            if (rects.size() % 4 != 0) return ftcl_err("rects UVec must contain 4 coordinates per aabb");
            const std::size_t point_count = points.size() / 2;
            const std::size_t rect_count = rects.size() / 4;
            auto points_ptr = points.as_uptr(*device);
            auto rects_ptr = rects.as_uptr(*device);
            if (!points_ptr.has_value()) return ftcl_err(points_ptr.error());
            if (!rects_ptr.has_value()) return ftcl_err(rects_ptr.error());
            auto values = UVec<ftclFloat>::uninitialized(rect_count, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::range_count_rect_cuda(*dst, *points_ptr, *rects_ptr, point_count, rect_count);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif
    std::vector<ftclFloat> points_flat, rects_flat;
    auto p_load = geom_load_uvec_cpu(*points_id, *device, points_flat);
    if (!p_load.has_value()) return p_load;
    auto r_load = geom_load_uvec_cpu(*rects_id, *device, rects_flat);
    if (!r_load.has_value()) return r_load;
    auto points = geom_points_from_flat(points_flat, "points UVec");
    auto rects = geom_aabbs_from_flat(rects_flat, "rects UVec");
    if (!points.has_value()) return ftcl::unexpected(points.error());
    if (!rects.has_value()) return ftcl::unexpected(rects.error());
    return geom_create_uvec_from_cpu(geom::range_count_rect(*points, *rects), *device);
}

inline ftclResult cmd_geom_bbox_reduce(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 4, "pointsHandle ?device?");
    if (!chk.has_value()) return chk;
    auto points_id = parse_uvec_handle(argv[2]);
    if (!points_id.has_value()) return ftcl::unexpected(points_id.error());
    auto device = geom_parse_optional_device(argv, 3);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_vec(*points_id, [&](UVec<ftclFloat>& points) {
            if (points.size() % 2 != 0) return ftcl_err("points UVec must contain an even number of coordinates");
            if (points.size() == 0) return ftcl_err("bbox_reduce requires at least one point");
            auto src = points.as_uptr(*device);
            if (!src.has_value()) return ftcl_err(src.error());
            auto values = UVec<ftclFloat>::uninitialized(4, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::bbox_reduce_cuda(*dst, *src, points.size() / 2);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        const auto& vals = out.cpu_vector();
        ftclList list;
        list.push_back(geom_point_to_value(geom::Point{vals[0], vals[1]}));
        list.push_back(geom_point_to_value(geom::Point{vals[2], vals[3]}));
        return ftcl_ok(Value::from_list(list));
    }
#endif
    std::vector<ftclFloat> flat;
    auto load = geom_load_uvec_cpu(*points_id, *device, flat);
    if (!load.has_value()) return load;
    auto points = geom_points_from_flat(flat, "points UVec");
    if (!points.has_value()) return ftcl::unexpected(points.error());
    auto box = geom::bounding_box(*points);
    if (!box.has_value()) return ftcl_err("bbox_reduce requires at least one point");
    ftclList list;
    list.push_back(geom_point_to_value(box->min));
    list.push_back(geom_point_to_value(box->max));
    return ftcl_ok(Value::from_list(list));
}

inline ftclResult cmd_geom_centroid(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 4, "pointsHandle ?device?");
    if (!chk.has_value()) return chk;
    auto points_id = parse_uvec_handle(argv[2]);
    if (!points_id.has_value()) return ftcl::unexpected(points_id.error());
    auto device = geom_parse_optional_device(argv, 3);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_vec(*points_id, [&](UVec<ftclFloat>& points) {
            if (points.size() % 2 != 0) return ftcl_err("points UVec must contain an even number of coordinates");
            if (points.size() == 0) return ftcl_err("centroid requires at least one point");
            auto src = points.as_uptr(*device);
            if (!src.has_value()) return ftcl_err(src.error());
            auto values = UVec<ftclFloat>::uninitialized(2, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::centroid_cuda(*dst, *src, points.size() / 2);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        const auto& vals = out.cpu_vector();
        return ftcl_ok(geom_point_to_value(geom::Point{vals[0], vals[1]}));
    }
#endif
    std::vector<ftclFloat> flat;
    auto load = geom_load_uvec_cpu(*points_id, *device, flat);
    if (!load.has_value()) return load;
    auto points = geom_points_from_flat(flat, "points UVec");
    if (!points.has_value()) return ftcl::unexpected(points.error());
    auto c = geom::centroid(*points);
    if (!c.has_value()) return ftcl_err("centroid requires at least one point");
    return ftcl_ok(geom_point_to_value(*c));
}

inline ftclResult cmd_geom_transform_points(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 5, 11, "pointsHandle mode args... ?device?");
    if (!chk.has_value()) return chk;
    auto points_id = parse_uvec_handle(argv[2]);
    if (!points_id.has_value()) return ftcl::unexpected(points_id.error());
    const std::string mode = argv[3].as_string();
    ftclFloat m00 = 1.0, m01 = 0.0, m02 = 0.0, m10 = 0.0, m11 = 1.0, m12 = 0.0;
    std::size_t device_index = 0;
    if (mode == "translate") {
        auto argc_chk = check_args(2, argv, 6, 7, "pointsHandle translate dx dy ?device?");
        if (!argc_chk.has_value()) return argc_chk;
        auto dx = parse_uvec_float(argv[4]);
        auto dy = parse_uvec_float(argv[5]);
        if (!dx.has_value()) return ftcl::unexpected(dx.error());
        if (!dy.has_value()) return ftcl::unexpected(dy.error());
        m02 = *dx;
        m12 = *dy;
        device_index = 6;
    } else if (mode == "scale") {
        auto argc_chk = check_args(2, argv, 6, 7, "pointsHandle scale sx sy ?device?");
        if (!argc_chk.has_value()) return argc_chk;
        auto sx = parse_uvec_float(argv[4]);
        auto sy = parse_uvec_float(argv[5]);
        if (!sx.has_value()) return ftcl::unexpected(sx.error());
        if (!sy.has_value()) return ftcl::unexpected(sy.error());
        m00 = *sx;
        m11 = *sy;
        device_index = 6;
    } else if (mode == "rotate") {
        auto argc_chk = check_args(2, argv, 5, 6, "pointsHandle rotate degrees ?device?");
        if (!argc_chk.has_value()) return argc_chk;
        auto degrees = parse_uvec_float(argv[4]);
        if (!degrees.has_value()) return ftcl::unexpected(degrees.error());
        const ftclFloat radians = *degrees * 3.14159265358979323846264338327950288 / 180.0;
        const ftclFloat c = std::cos(radians);
        const ftclFloat s = std::sin(radians);
        m00 = c;
        m01 = -s;
        m10 = s;
        m11 = c;
        device_index = 5;
    } else if (mode == "affine") {
        auto argc_chk = check_args(2, argv, 10, 11, "pointsHandle affine m00 m01 m02 m10 m11 m12 ?device?");
        if (!argc_chk.has_value()) return argc_chk;
        auto a = parse_uvec_float(argv[4]);
        auto b = parse_uvec_float(argv[5]);
        auto c = parse_uvec_float(argv[6]);
        auto d = parse_uvec_float(argv[7]);
        auto e = parse_uvec_float(argv[8]);
        auto f = parse_uvec_float(argv[9]);
        if (!a.has_value()) return ftcl::unexpected(a.error());
        if (!b.has_value()) return ftcl::unexpected(b.error());
        if (!c.has_value()) return ftcl::unexpected(c.error());
        if (!d.has_value()) return ftcl::unexpected(d.error());
        if (!e.has_value()) return ftcl::unexpected(e.error());
        if (!f.has_value()) return ftcl::unexpected(f.error());
        m00 = *a; m01 = *b; m02 = *c; m10 = *d; m11 = *e; m12 = *f;
        device_index = 10;
    } else {
        return ftcl_err("bad transform mode \"" + mode + "\": must be translate, scale, rotate, or affine");
    }
    auto device = geom_parse_optional_device(argv, device_index);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_vec(*points_id, [&](UVec<ftclFloat>& points) {
            if (points.size() % 2 != 0) return ftcl_err("points UVec must contain an even number of coordinates");
            const std::size_t count = points.size() / 2;
            auto src = points.as_uptr(*device);
            if (!src.has_value()) return ftcl_err(src.error());
            auto values = UVec<ftclFloat>::uninitialized(points.size(), *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::transform_points_cuda(*dst, *src, count, m00, m01, m02, m10, m11, m12);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif
    std::vector<ftclFloat> flat;
    auto load = geom_load_uvec_cpu(*points_id, *device, flat);
    if (!load.has_value()) return load;
    auto points = geom_points_from_flat(flat, "points UVec");
    if (!points.has_value()) return ftcl::unexpected(points.error());
    return geom_create_uvec_from_cpu(flatten_geom_points(geom::transform_points(*points, m00, m01, m02, m10, m11, m12)), *device);
}

inline ftclResult cmd_geom_batch_orientation(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 3, 4, "pointTriplesHandle ?device?");
    if (!chk.has_value()) return chk;
    auto id = parse_uvec_handle(argv[2]);
    if (!id.has_value()) return ftcl::unexpected(id.error());
    auto device = geom_parse_optional_device(argv, 3);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_vec(*id, [&](UVec<ftclFloat>& triples) {
            if (triples.size() % 6 != 0) return ftcl_err("pointTriples UVec must contain 3 points per orientation test");
            const std::size_t count = triples.size() / 6;
            auto src = triples.as_uptr(*device);
            if (!src.has_value()) return ftcl_err(src.error());
            auto values = UVec<ftclFloat>::uninitialized(count, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::batch_orientation_cuda(*dst, *src, count);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif
    std::vector<ftclFloat> flat;
    auto load = geom_load_uvec_cpu(*id, *device, flat);
    if (!load.has_value()) return load;
    auto triples = geom_points_from_flat(flat, "pointTriples UVec");
    if (!triples.has_value()) return ftcl::unexpected(triples.error());
    if (triples->size() % 3 != 0) return ftcl_err("pointTriples UVec must contain 3 points per orientation test");
    return geom_create_uvec_from_cpu(geom::batch_orientation(*triples), *device);
}

inline ftclResult cmd_geom_collision_aabb(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "aabbAHandle aabbBHandle ?device?");
    if (!chk.has_value()) return chk;
    auto lhs_id = parse_uvec_handle(argv[2]);
    auto rhs_id = parse_uvec_handle(argv[3]);
    if (!lhs_id.has_value()) return ftcl::unexpected(lhs_id.error());
    if (!rhs_id.has_value()) return ftcl::unexpected(rhs_id.error());
    auto device = geom_parse_optional_device(argv, 4);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_two(*lhs_id, *rhs_id, [&](UVec<ftclFloat>& lhs, UVec<ftclFloat>& rhs) {
            if (lhs.size() % 4 != 0 || rhs.size() % 4 != 0) return ftcl_err("aabb UVec must contain 4 coordinates per box");
            if (lhs.size() != rhs.size()) return ftcl_err("aabb UVec inputs must have the same number of boxes");
            const std::size_t count = lhs.size() / 4;
            auto lhs_ptr = lhs.as_uptr(*device);
            auto rhs_ptr = rhs.as_uptr(*device);
            if (!lhs_ptr.has_value()) return ftcl_err(lhs_ptr.error());
            if (!rhs_ptr.has_value()) return ftcl_err(rhs_ptr.error());
            auto values = UVec<ftclFloat>::uninitialized(count, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::collision_aabb_cuda(*dst, *lhs_ptr, *rhs_ptr, count);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif
    std::vector<ftclFloat> lhs_flat, rhs_flat;
    auto lhs_load = geom_load_uvec_cpu(*lhs_id, *device, lhs_flat);
    if (!lhs_load.has_value()) return lhs_load;
    auto rhs_load = geom_load_uvec_cpu(*rhs_id, *device, rhs_flat);
    if (!rhs_load.has_value()) return rhs_load;
    auto lhs = geom_aabbs_from_flat(lhs_flat, "aabbA UVec");
    auto rhs = geom_aabbs_from_flat(rhs_flat, "aabbB UVec");
    if (!lhs.has_value()) return ftcl::unexpected(lhs.error());
    if (!rhs.has_value()) return ftcl::unexpected(rhs.error());
    if (lhs->size() != rhs->size()) return ftcl_err("aabb UVec inputs must have the same number of boxes");
    return geom_create_uvec_from_cpu(geom::collision_aabb(*lhs, *rhs), *device);
}

inline ftclResult cmd_geom_polygon_batch_area(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "pointsHandle offsetsHandle ?device?");
    if (!chk.has_value()) return chk;
    auto points_id = parse_uvec_handle(argv[2]);
    auto offsets_id = parse_uvec_handle(argv[3]);
    if (!points_id.has_value()) return ftcl::unexpected(points_id.error());
    if (!offsets_id.has_value()) return ftcl::unexpected(offsets_id.error());
    auto device = geom_parse_optional_device(argv, 4);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_two(*points_id, *offsets_id, [&](UVec<ftclFloat>& points, UVec<ftclFloat>& offsets) {
            if (points.size() % 2 != 0) return ftcl_err("points UVec must contain an even number of coordinates");
            if (offsets.size() < 2) return ftcl_err("polygon_batch_area requires at least two offsets");
            const std::size_t point_count = points.size() / 2;
            const std::size_t polygon_count = offsets.size() - 1;
            auto points_ptr = points.as_uptr(*device);
            auto offsets_ptr = offsets.as_uptr(*device);
            if (!points_ptr.has_value()) return ftcl_err(points_ptr.error());
            if (!offsets_ptr.has_value()) return ftcl_err(offsets_ptr.error());
            auto values = UVec<ftclFloat>::uninitialized(polygon_count, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::polygon_batch_area_cuda(*dst, *points_ptr, *offsets_ptr, point_count, polygon_count);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif
    std::vector<ftclFloat> points_flat, offsets;
    auto p_load = geom_load_uvec_cpu(*points_id, *device, points_flat);
    if (!p_load.has_value()) return p_load;
    auto o_load = geom_load_uvec_cpu(*offsets_id, *device, offsets);
    if (!o_load.has_value()) return o_load;
    auto points = geom_points_from_flat(points_flat, "points UVec");
    if (!points.has_value()) return ftcl::unexpected(points.error());
    if (offsets.size() < 2) return ftcl_err("polygon_batch_area requires at least two offsets");
    return geom_create_uvec_from_cpu(geom::polygon_batch_area(*points, offsets), *device);
}

inline ftclResult cmd_geom_distance_to_polyline(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 4, 5, "pointsHandle polyline ?device?");
    if (!chk.has_value()) return chk;
    auto points_id = parse_uvec_handle(argv[2]);
    if (!points_id.has_value()) return ftcl::unexpected(points_id.error());
    auto polyline = parse_geom_points(argv[3]);
    if (!polyline.has_value()) return ftcl::unexpected(polyline.error());
    if (polyline->size() < 2) return ftcl_err("distance_to_polyline requires at least two polyline points");
    auto device = geom_parse_optional_device(argv, 4);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        UVec<ftclFloat> polyline_vec = UVec<ftclFloat>::from_cpu(flatten_geom_points(*polyline));
        auto poly_mut = polyline_vec.as_mut_uptr(*device);
        if (!poly_mut.has_value()) return ftcl_err(poly_mut.error());
        auto poly_ptr = polyline_vec.as_uptr(*device);
        if (!poly_ptr.has_value()) return ftcl_err(poly_ptr.error());
        auto ran = uvec_command_manager().with_vec(*points_id, [&](UVec<ftclFloat>& points) {
            if (points.size() % 2 != 0) return ftcl_err("points UVec must contain an even number of coordinates");
            const std::size_t point_count = points.size() / 2;
            auto src = points.as_uptr(*device);
            if (!src.has_value()) return ftcl_err(src.error());
            auto values = UVec<ftclFloat>::uninitialized(point_count, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::distance_to_polyline_cuda(*dst, *src, *poly_ptr, point_count, polyline->size());
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif
    std::vector<ftclFloat> points_flat;
    auto load = geom_load_uvec_cpu(*points_id, *device, points_flat);
    if (!load.has_value()) return load;
    auto points = geom_points_from_flat(points_flat, "points UVec");
    if (!points.has_value()) return ftcl::unexpected(points.error());
    return geom_create_uvec_from_cpu(geom::distance_to_polyline(*points, *polyline), *device);
}

inline ftclResult cmd_geom_spatial_grid_build(Interp*, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(2, argv, 6, 7, "pointsHandle origin cellSize columns ?device?");
    if (!chk.has_value()) return chk;
    auto points_id = parse_uvec_handle(argv[2]);
    auto origin = parse_geom_point(argv[3]);
    auto cell_size = parse_uvec_float(argv[4]);
    auto columns = geom_parse_positive_size(argv[5], "columns");
    if (!points_id.has_value()) return ftcl::unexpected(points_id.error());
    if (!origin.has_value()) return ftcl::unexpected(origin.error());
    if (!cell_size.has_value()) return ftcl::unexpected(cell_size.error());
    if (*cell_size <= 0.0) return ftcl_err("cellSize must be positive");
    if (!columns.has_value()) return ftcl::unexpected(columns.error());
    auto device = geom_parse_optional_device(argv, 6);
    if (!device.has_value()) return ftcl::unexpected(device.error());
#ifdef FTCL_GEOMETRY_CUDA
    if (device->is_cuda()) {
        UVec<ftclFloat> out;
        auto ran = uvec_command_manager().with_vec(*points_id, [&](UVec<ftclFloat>& points) {
            if (points.size() % 2 != 0) return ftcl_err("points UVec must contain an even number of coordinates");
            const std::size_t point_count = points.size() / 2;
            auto src = points.as_uptr(*device);
            if (!src.has_value()) return ftcl_err(src.error());
            auto values = UVec<ftclFloat>::uninitialized(point_count, *device);
            if (!values.has_value()) return ftcl_err(values.error());
            out = std::move(*values);
            auto dst = out.as_mut_uptr(*device);
            if (!dst.has_value()) return ftcl_err(dst.error());
            auto launched = geom::spatial_grid_build_cuda(*dst, *src, point_count, *origin, *cell_size, *columns);
            if (!launched.has_value()) return ftcl_err(launched.error());
            return ftcl_ok();
        });
        if (!ran.has_value()) return ran;
        return ftcl_ok(uvec_command_manager().create(std::move(out)));
    }
#endif
    std::vector<ftclFloat> flat;
    auto load = geom_load_uvec_cpu(*points_id, *device, flat);
    if (!load.has_value()) return load;
    auto points = geom_points_from_flat(flat, "points UVec");
    if (!points.has_value()) return ftcl::unexpected(points.error());
    return geom_create_uvec_from_cpu(geom::spatial_grid_build(*points, *origin, *cell_size, *columns), *device);
}

inline ftclResult cmd_geom(Interp* interp, ContextID context_id, const std::vector<Value>& argv) {
    std::vector<Subcommand> subs = {
        Subcommand("batch_distance", cmd_geom_batch_distance),
        Subcommand("batch_distance_matrix", cmd_geom_batch_distance_matrix),
        Subcommand("batch_orientation", cmd_geom_batch_orientation),
        Subcommand("batch_point_in_polygon", cmd_geom_batch_point_in_polygon),
        Subcommand("batch_point_segment_distance", cmd_geom_batch_point_segment_distance),
        Subcommand("batch_segment_intersect", cmd_geom_batch_segment_intersect),
        Subcommand("bbox", cmd_geom_bbox),
        Subcommand("bbox_reduce", cmd_geom_bbox_reduce),
        Subcommand("centroid", cmd_geom_centroid),
        Subcommand("closest_pair_distance", cmd_geom_closest_pair_distance),
        Subcommand("collision_aabb", cmd_geom_collision_aabb),
        Subcommand("convex_hull", cmd_geom_convex_hull),
        Subcommand("distance", cmd_geom_distance),
        Subcommand("distance2", cmd_geom_distance2),
        Subcommand("distance_to_polyline", cmd_geom_distance_to_polyline),
        Subcommand("k_nearest", cmd_geom_k_nearest),
        Subcommand("line_intersection", cmd_geom_line_intersection),
        Subcommand("nearest_point", cmd_geom_nearest_point),
        Subcommand("on_segment", cmd_geom_on_segment),
        Subcommand("orient", cmd_geom_orient),
        Subcommand("point_in_polygon", cmd_geom_point_in_polygon),
        Subcommand("point_line_distance", cmd_geom_point_line_distance),
        Subcommand("point_segment_distance", cmd_geom_point_segment_distance),
        Subcommand("polygon_area", cmd_geom_polygon_area),
        Subcommand("polygon_batch_area", cmd_geom_polygon_batch_area),
        Subcommand("polygon_perimeter", cmd_geom_polygon_perimeter),
        Subcommand("polygon_signed_area", cmd_geom_polygon_signed_area),
        Subcommand("range_count_circle", cmd_geom_range_count_circle),
        Subcommand("range_count_rect", cmd_geom_range_count_rect),
        Subcommand("segment_intersect", cmd_geom_segment_intersect),
        Subcommand("spatial_grid_build", cmd_geom_spatial_grid_build),
        Subcommand("transform_points", cmd_geom_transform_points),
        Subcommand("uvec_aabbs", cmd_geom_uvec_aabbs),
        Subcommand("uvec_points", cmd_geom_uvec_points),
        Subcommand("uvec_segments", cmd_geom_uvec_segments),
    };
    return interp->call_subcommand(context_id, argv, 1, subs);
}

inline ftclResult cmd_pdump(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 1, 1, "");
    if (!chk.has_value()) {
        return chk;
    }
    interp->profile_dump();
    return ftcl_ok();
}

inline ftclResult cmd_pclear(Interp* interp, ContextID, const std::vector<Value>& argv) {
    auto chk = check_args(1, argv, 1, 1, "");
    if (!chk.has_value()) {
        return chk;
    }
    interp->profile_clear();
    return ftcl_ok();
}

inline void install_core_commands(Interp& interp) {
    interp.add_command("append", cmd_append);
    interp.add_command("array", cmd_array);
    interp.add_command("assert_eq", cmd_assert_eq);
    interp.add_command("break", cmd_break);
    interp.add_command("catch", cmd_catch);
    interp.add_command("continue", cmd_continue);
    interp.add_command("dict", cmd_dict);
    interp.add_command("error", cmd_error);
    interp.add_command("exit", cmd_exit);
    interp.add_command("expr", cmd_expr);
    interp.add_command("for", cmd_for);
    interp.add_command("geom", cmd_geom);
    interp.add_command("foreach", cmd_foreach);
    interp.add_command("getch", cmd_getch);
    interp.add_command("global", cmd_global);
    interp.add_command("gets", cmd_gets);
    interp.add_command("if", cmd_if);
    interp.add_command("incr", cmd_incr);
    interp.add_command("info", cmd_info);
    interp.add_command("join", cmd_join);
    interp.add_command("lappend", cmd_lappend);
    interp.add_command("lindex", cmd_lindex);
    interp.add_command("list", cmd_list);
    interp.add_command("llength", cmd_llength);
    interp.add_command("pclear", cmd_pclear);
    interp.add_command("pdump", cmd_pdump);
    interp.add_command("proc", cmd_proc);
    interp.add_command("puts", cmd_puts);
    interp.add_command("rename", cmd_rename);
    interp.add_command("return", cmd_return);
    interp.add_command("set", cmd_set);
    interp.add_command("source", cmd_source);
    interp.add_command("string", cmd_string);
    interp.add_command("thread", cmd_thread);
    interp.add_command("throw", cmd_throw);
    interp.add_command("time", cmd_time);
    interp.add_command("uvec", cmd_uvec);
    interp.add_command("sleep", cmd_sleep);
    interp.add_command("unset", cmd_unset);
    interp.add_command("while", cmd_while);
}

inline Interp new_interp_with_stdlib() {
    Interp interp;
    install_core_commands(interp);
    return interp;
}

}  // namespace ftcl
