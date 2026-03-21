#pragma once

#include "dict.hpp"
#include "interp.hpp"
#include "list.hpp"
#include "macros.hpp"
#include "util.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
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
