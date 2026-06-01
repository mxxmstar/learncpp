#include "common/process/process.h"

#include <boost/asio/post.hpp>
#include <boost/process/v2/process.hpp>
#include <boost/process/v2/start_dir.hpp>
#include <boost/system/errc.hpp>
#include <boost/system/system_error.hpp>

#ifdef _WIN32
#include <boost/process/v2/windows/creation_flags.hpp>
#include <boost/process/v2/windows/show_window.hpp>
#endif

#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace common::process {
namespace {

namespace bp = boost::process;
using ErrorCode = boost::system::error_code;

ErrorCode MakeError(boost::system::errc::errc_t errc) {
    return boost::system::errc::make_error_code(errc);
}

bp::filesystem::path ToBoostPath(const std::filesystem::path& path) {
    return bp::filesystem::path(path.string());
}

} // namespace

class Process::Impl {
public:
    explicit Impl(boost::asio::any_io_executor executor)
        : executor_(std::move(executor)) {
    }

    bool Start(const ProcessOptions& options, ErrorCode& ec) noexcept {
        if (options.executable.empty()) {
            return SetError(MakeError(boost::system::errc::invalid_argument),
                            "process executable is empty", ec);
        }

        if (IsRunning(ec)) {
            return SetError(MakeError(boost::system::errc::device_or_resource_busy),
                            "process is already running", ec);
        }

        try {
            process_ = Launch(options);
            ec.clear();
            last_error_.clear();
            return true;
        } catch (const boost::system::system_error& e) {
            return SetError(e.code(), e.what(), ec);
        } catch (const std::exception& e) {
            return SetError(MakeError(boost::system::errc::io_error), e.what(), ec);
        } catch (...) {
            return SetError(MakeError(boost::system::errc::io_error),
                            "unknown process start error", ec);
        }
    }

    bool IsOpen() const noexcept {
        return process_ && process_->is_open();
    }

    bool IsRunning() {
        return process_ && process_->running();
    }

    bool IsRunning(ErrorCode& ec) noexcept {
        if (!process_) {
            ec.clear();
            return false;
        }
        return process_->running(ec);
    }

    std::uint64_t Pid() const noexcept {
        if (!process_) {
            return 0;
        }
        return static_cast<std::uint64_t>(process_->id());
    }

    int ExitCode() const noexcept {
        if (!process_) {
            return -1;
        }
        return process_->exit_code();
    }

    void AsyncWait(Process::ExitHandler handler) {
        if (!handler) {
            return;
        }

        auto proc = process_;
        if (!proc) {
            auto executor = executor_;
            boost::asio::post(executor, [handler = std::move(handler)]() mutable {
                handler(MakeError(boost::system::errc::no_such_process), -1);
            });
            return;
        }

        proc->async_wait(
            [proc, handler = std::move(handler)](const ErrorCode& ec, int exit_code) mutable {
                handler(ec, exit_code);
            });
    }

    bool RequestExit(ErrorCode& ec) noexcept {
        if (!process_) {
            return SetError(MakeError(boost::system::errc::no_such_process),
                            "process is not started", ec);
        }

        try {
            process_->request_exit(ec);
            if (ec) {
                last_error_ = ec.message();
            }
            return !ec;
        } catch (const boost::system::system_error& e) {
            return SetError(e.code(), e.what(), ec);
        } catch (const std::exception& e) {
            return SetError(MakeError(boost::system::errc::io_error), e.what(), ec);
        } catch (...) {
            return SetError(MakeError(boost::system::errc::io_error),
                            "unknown process request_exit error", ec);
        }
    }

    bool Terminate(ErrorCode& ec) noexcept {
        if (!process_) {
            return SetError(MakeError(boost::system::errc::no_such_process),
                            "process is not started", ec);
        }

        try {
            process_->terminate(ec);
            if (ec) {
                last_error_ = ec.message();
            }
            return !ec;
        } catch (const boost::system::system_error& e) {
            return SetError(e.code(), e.what(), ec);
        } catch (const std::exception& e) {
            return SetError(MakeError(boost::system::errc::io_error), e.what(), ec);
        } catch (...) {
            return SetError(MakeError(boost::system::errc::io_error),
                            "unknown process terminate error", ec);
        }
    }

    int Wait(ErrorCode& ec) noexcept {
        if (!process_) {
            SetError(MakeError(boost::system::errc::no_such_process),
                     "process is not started", ec);
            return -1;
        }

        try {
            auto exit_code = process_->wait(ec);
            if (ec) {
                last_error_ = ec.message();
            }
            return exit_code;
        } catch (const boost::system::system_error& e) {
            SetError(e.code(), e.what(), ec);
            return -1;
        } catch (const std::exception& e) {
            SetError(MakeError(boost::system::errc::io_error), e.what(), ec);
            return -1;
        } catch (...) {
            SetError(MakeError(boost::system::errc::io_error),
                     "unknown process wait error", ec);
            return -1;
        }
    }

    void Reset() noexcept {
        process_.reset();
    }

    const std::string& LastError() const noexcept {
        return last_error_;
    }

private:
    bool SetError(ErrorCode source, std::string message, ErrorCode& out) noexcept {
        out = source;
        last_error_ = std::move(message);
        if (last_error_.empty()) {
            last_error_ = out.message();
        }
        return false;
    }

    template <typename... Inits>
    std::shared_ptr<bp::process> LaunchWithInits(const ProcessOptions& options,
                                                 Inits&&... inits) {
        auto executable = ToBoostPath(options.executable);
        if (options.working_directory.empty()) {
            return std::make_shared<bp::process>(
                executor_, executable, options.arguments, std::forward<Inits>(inits)...);
        }

        return std::make_shared<bp::process>(
            executor_,
            executable,
            options.arguments,
            bp::process_start_dir{ToBoostPath(options.working_directory)},
            std::forward<Inits>(inits)...);
    }

    static std::string MakeTerminalTitle(const ProcessOptions& options) {
        if (!options.terminal_title.empty()) {
            return options.terminal_title;
        }

        auto filename = options.executable.filename().string();
        return filename.empty() ? options.executable.string() : filename;
    }

#if !defined(_WIN32)
    static std::vector<std::string> MakeTerminalCommand(const ProcessOptions& options) {
        std::vector<std::string> command;
        command.push_back(options.executable.string());
        command.insert(command.end(), options.arguments.begin(), options.arguments.end());
        return command;
    }

    ProcessOptions ResolvePosixTerminalOptions(const ProcessOptions& options) {
        ProcessOptions terminal_options = options;
        auto command = MakeTerminalCommand(options);
        auto title = MakeTerminalTitle(options);

        if (std::filesystem::exists("/usr/bin/xterm")) {
            terminal_options.executable = "/usr/bin/xterm";
            terminal_options.arguments = {"-T", title, "-e"};
            terminal_options.arguments.insert(
                terminal_options.arguments.end(), command.begin(), command.end());
            terminal_options.terminal_mode = TerminalMode::None;
            return terminal_options;
        }

        if (std::filesystem::exists("/usr/bin/gnome-terminal")) {
            terminal_options.executable = "/usr/bin/gnome-terminal";
            terminal_options.arguments = {"--title", title, "--"};
            terminal_options.arguments.insert(
                terminal_options.arguments.end(), command.begin(), command.end());
            terminal_options.terminal_mode = TerminalMode::None;
            return terminal_options;
        }

        if (std::filesystem::exists("/usr/bin/konsole")) {
            terminal_options.executable = "/usr/bin/konsole";
            terminal_options.arguments = {"--title", title, "-e"};
            terminal_options.arguments.insert(
                terminal_options.arguments.end(), command.begin(), command.end());
            terminal_options.terminal_mode = TerminalMode::None;
            return terminal_options;
        }

        throw boost::system::system_error(
            MakeError(boost::system::errc::no_such_file_or_directory),
            "no suitable terminal emulator found");
    }
#endif

#ifdef _WIN32
    template <typename... Inits>
    std::shared_ptr<bp::process> LaunchWithWindow(const ProcessOptions& options,
                                                  Inits&&... inits) {
        switch (options.window_mode) {
        case WindowMode::Normal:
            return LaunchWithInits(options, std::forward<Inits>(inits)...,
                                   bp::windows::show_window_normal);
        case WindowMode::Maximized:
            return LaunchWithInits(options, std::forward<Inits>(inits)...,
                                   bp::windows::show_window_maximized);
        case WindowMode::Minimized:
            return LaunchWithInits(options, std::forward<Inits>(inits)...,
                                   bp::windows::show_window_minimized);
        case WindowMode::Hidden:
            return LaunchWithInits(options, std::forward<Inits>(inits)...,
                                   bp::windows::show_window_hide);
        case WindowMode::Default:
        default:
            return LaunchWithInits(options, std::forward<Inits>(inits)...);
        }
    }

    std::shared_ptr<bp::process> Launch(const ProcessOptions& options) {
        if (options.terminal_mode == TerminalMode::NewTerminal ||
            options.console_mode == ConsoleMode::NewConsole) {
            return LaunchWithWindow(options, bp::windows::create_new_console);
        }
        return LaunchWithWindow(options);
    }
#else
    std::shared_ptr<bp::process> Launch(const ProcessOptions& options) {
        if (options.terminal_mode == TerminalMode::NewTerminal) {
            return LaunchWithInits(ResolvePosixTerminalOptions(options));
        }
        return LaunchWithInits(options);
    }
#endif

    boost::asio::any_io_executor executor_;
    std::shared_ptr<bp::process> process_;
    std::string last_error_;
};

Process::Process(boost::asio::any_io_executor executor)
    : impl_(std::make_unique<Impl>(std::move(executor))) {
}

Process::~Process() = default;

Process::Process(Process&&) noexcept = default;

Process& Process::operator=(Process&&) noexcept = default;

bool Process::Start(const ProcessOptions& options) {
    ErrorCode ec;
    return Start(options, ec);
}

bool Process::Start(const ProcessOptions& options, ErrorCode& ec) noexcept {
    return impl_->Start(options, ec);
}

bool Process::IsOpen() const noexcept {
    return impl_->IsOpen();
}

bool Process::IsRunning() {
    return impl_->IsRunning();
}

bool Process::IsRunning(ErrorCode& ec) noexcept {
    return impl_->IsRunning(ec);
}

std::uint64_t Process::Pid() const noexcept {
    return impl_->Pid();
}

int Process::ExitCode() const noexcept {
    return impl_->ExitCode();
}

void Process::AsyncWait(ExitHandler handler) {
    impl_->AsyncWait(std::move(handler));
}

bool Process::RequestExit(ErrorCode& ec) noexcept {
    return impl_->RequestExit(ec);
}

bool Process::Terminate(ErrorCode& ec) noexcept {
    return impl_->Terminate(ec);
}

int Process::Wait() {
    ErrorCode ec;
    auto exit_code = Wait(ec);
    if (ec) {
        throw boost::system::system_error(ec, "process wait failed");
    }
    return exit_code;
}

int Process::Wait(ErrorCode& ec) noexcept {
    return impl_->Wait(ec);
}

void Process::Reset() noexcept {
    impl_->Reset();
}

const std::string& Process::LastError() const noexcept {
    return impl_->LastError();
}

} // namespace common::process
