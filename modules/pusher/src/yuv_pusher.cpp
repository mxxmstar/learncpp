#include "pusher/yuv_pusher.h"
#include "ffmpeg_opt/ffmpeg_opt.h"
#include "common/log/logmanager.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

#include <cstring>

YuvPusher::YuvPusher() = default;

YuvPusher::~YuvPusher() {
    Stop();
}

bool YuvPusher::Start(const PusherConfig& config, PushCallback cb) {
    if (running_) {
        LOG_MAIN_WARN_AT("YuvPusher: Already running");
        return false;
    }
    
    config_ = config;
    callback_ = std::move(cb);
    
    if (!converter_.Initialize(config_.width, config_.height)) {
        LOG_MAIN_ERROR_AT("YuvPusher: Failed to initialize converter");
        return false;
    }
    
    yuv_buffer_.resize(converter_.GetYuvSize());
    
    std::string ffmpeg_path = FFmpegOpt::GetFFmpegPath();
    if (ffmpeg_path.empty()) {
        LOG_MAIN_ERROR_AT("YuvPusher: FFmpeg not found");
        return false;
    }
    
    std::ostringstream cmd;
    cmd << "\"" << ffmpeg_path << "\" ";
    cmd << "-f rawvideo ";
    cmd << "-pix_fmt bgr24 ";
    cmd << "-s " << config_.width << "x" << config_.height << " ";
    cmd << "-framerate " << config_.fps << " ";
    cmd << "-i - ";
    cmd << "-c:v libx264 ";
    cmd << "-preset ultrafast ";
    cmd << "-tune zerolatency ";
    cmd << "-b:v " << config_.bitrate << "k ";
    cmd << "-g " << config_.gop_size << " ";
    cmd << "-pix_fmt yuv420p ";
    cmd << "-f rtsp ";
    cmd << "-rtsp_transport tcp ";
    cmd << "\"" << config_.url << "\"";
    
    std::string command = cmd.str();
    LOG_MAIN_INFO_AT("YuvPusher: Starting FFmpeg: {}", command);
    
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    HANDLE hStdInWrite = NULL;
    HANDLE hStdOutRead = NULL;
    
    if (!CreatePipe(&hStdOutRead, &hStdInWrite, &sa, 0)) {
        LOG_MAIN_ERROR_AT("YuvPusher: CreatePipe failed");
        return false;
    }
    
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    
    PROCESS_INFORMATION pi;
    STARTUPINFOA si;
    
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hStdInWrite;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    
    std::vector<char> cmd_buffer(command.begin(), command.end());
    cmd_buffer.push_back('\0');
    
    BOOL success = CreateProcessA(
        NULL, cmd_buffer.data(), NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    
    if (!success) {
        LOG_MAIN_ERROR_AT("YuvPusher: CreateProcess failed");
        CloseHandle(hStdOutRead);
        CloseHandle(hStdInWrite);
        return false;
    }
    
    CloseHandle(hStdInWrite);
    stdin_pipe_ = reinterpret_cast<void*>(hStdOutRead);
    ffmpeg_process_ = reinterpret_cast<void*>(pi.hProcess);
    
    LOG_MAIN_INFO_AT("YuvPusher: FFmpeg started with PID: {}", GetProcessId(pi.hProcess));
#else
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        LOG_MAIN_ERROR_AT("YuvPusher: pipe failed");
        return false;
    }
    
    pid_t pid = fork();
    if (pid < 0) {
        LOG_MAIN_ERROR_AT("YuvPusher: fork failed");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return false;
    }
    
    if (pid == 0) {
        close(pipe_fd[1]);
        dup2(pipe_fd[0], STDIN_FILENO);
        close(pipe_fd[0]);
        
        execl(ffmpeg_path.c_str(), "ffmpeg",
            "-f", "rawvideo", "-pix_fmt", "bgr24",
            "-s", std::to_string(config_.width).c_str(), "x", std::to_string(config_.height).c_str(),
            "-framerate", std::to_string(config_.fps).c_str(),
            "-i", "-",
            "-c:v", "libx264", "-preset", "ultrafast", "-tune", "zerolatency",
            "-b:v", std::to_string(config_.bitrate).c_str(), "k",
            "-g", std::to_string(config_.gop_size).c_str(),
            "-pix_fmt", "yuv420p",
            "-f", "rtsp", "-rtsp_transport", "tcp",
            config_.url.c_str(), NULL);
        
        _exit(1);
    }
    
    close(pipe_fd[0]);
    stdin_pipe_ = reinterpret_cast<void*>(static_cast<intptr_t>(pipe_fd[1]));
    ffmpeg_pid_ = pid;
#endif
    
    running_ = true;
    stopped_ = false;
    stats_ = PusherStats();
    
    io_thread_ = std::thread([this]() { runLoop(); });
    
    LOG_MAIN_INFO_AT("YuvPusher: Started successfully");
    return true;
}

void YuvPusher::Stop() {
    if (!running_) return;
    
    LOG_MAIN_INFO_AT("YuvPusher: Stopping...");
    running_ = false;
    stopped_ = true;
    
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
#ifdef _WIN32
    if (stdin_pipe_) {
        CloseHandle(stdin_pipe_);
        stdin_pipe_ = nullptr;
    }
    if (ffmpeg_process_) {
        TerminateProcess(ffmpeg_process_, 0);
        CloseHandle(ffmpeg_process_);
        ffmpeg_process_ = nullptr;
    }
#else
    if (stdin_pipe_ != -1) {
        close(stdin_pipe_);
        stdin_pipe_ = -1;
    }
    if (ffmpeg_pid_ > 0) {
        kill(ffmpeg_pid_, SIGTERM);
        waitpid(ffmpeg_pid_, NULL, 0);
        ffmpeg_pid_ = -1;
    }
#endif
    
    LOG_MAIN_INFO_AT("YuvPusher: Stopped. Stats: sent={}, failed={}, rate={:.1f}%",
                    stats_.frames_sent, stats_.frames_failed, stats_.getSuccessRate());
}

bool YuvPusher::PushFrame(const cv::Mat& bgr_frame, int64_t pts) {
    if (!running_ || stopped_) return false;
    
    if (bgr_frame.empty()) return false;
    
    if (bgr_frame.cols != config_.width || bgr_frame.rows != config_.height) {
        LOG_MAIN_WARN_AT("YuvPusher: Frame size mismatch: {}x{} vs {}x{}",
                        bgr_frame.cols, bgr_frame.rows, config_.width, config_.height);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(queue_mutex_);
    frame_queue_.push({bgr_frame.clone(), pts});
    return true;
}

void YuvPusher::runLoop() {
    LOG_MAIN_INFO_AT("YuvPusher: IO thread started");
    
    const size_t frame_size = config_.width * config_.height * 3;
    std::vector<uint8_t> bgr_buffer(frame_size);
    
    while (running_ && !stopped_) {
        std::pair<cv::Mat, int64_t> item;
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (frame_queue_.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            item = std::move(frame_queue_.front());
            frame_queue_.pop();
        }
        
        const cv::Mat& frame = item.first;
        if (frame.empty()) continue;
        
        std::memcpy(bgr_buffer.data(), frame.data, frame_size);
        
        if (writeToStdin(bgr_buffer.data(), bgr_buffer.size())) {
            stats_.frames_sent++;
            stats_.bytes_sent += frame_size;
            stats_.last_pts = item.second;
        } else {
            stats_.frames_failed++;
            LOG_MAIN_ERROR_AT("YuvPusher: Failed to write frame");
            
            if (stats_.frames_failed > 10 && callback_) {
                callback_(false, "Too many write failures", stats_);
            }
        }
    }
    
    LOG_MAIN_INFO_AT("YuvPusher: IO thread ended");
}

bool YuvPusher::writeToStdin(const uint8_t* data, size_t size) {
#ifdef _WIN32
    DWORD written = 0;
    BOOL result = WriteFile(stdin_pipe_, data, static_cast<DWORD>(size), &written, NULL);
    return result && written == size;
#else
    ssize_t written = write(static_cast<int>(reinterpret_cast<intptr_t>(stdin_pipe_)), data, size);
    return written == static_cast<ssize_t>(size);
#endif
}

std::unique_ptr<IPusher> IPusher::Create() {
    return std::make_unique<YuvPusher>();
}