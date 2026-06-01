#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/system/error_code.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace common::process {

enum class ConsoleMode {
    Inherit,
    NewConsole,
};

enum class WindowMode {
    Default,
    Normal,
    Maximized,
    Minimized,
    Hidden,
};

enum class TerminalMode {
    None,
    NewTerminal,
};

struct ProcessOptions {
    std::filesystem::path executable;
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;
    ConsoleMode console_mode{ConsoleMode::Inherit};
    WindowMode window_mode{WindowMode::Default};
    TerminalMode terminal_mode{TerminalMode::None};
    std::string terminal_title;
};

class Process {
public:
    using ExitHandler = std::function<void(const boost::system::error_code&, int)>;

    explicit Process(boost::asio::any_io_executor executor);
    ~Process();

    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    Process(Process&&) noexcept;
    Process& operator=(Process&&) noexcept;

    bool Start(const ProcessOptions& options);
    bool Start(const ProcessOptions& options, boost::system::error_code& ec) noexcept;

    bool IsOpen() const noexcept;
    bool IsRunning();
    bool IsRunning(boost::system::error_code& ec) noexcept;

    std::uint64_t Pid() const noexcept;
    int ExitCode() const noexcept;

    void AsyncWait(ExitHandler handler);

    bool RequestExit(boost::system::error_code& ec) noexcept;
    bool Terminate(boost::system::error_code& ec) noexcept;
    int Wait();
    int Wait(boost::system::error_code& ec) noexcept;

    void Reset() noexcept;

    const std::string& LastError() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace common::process
