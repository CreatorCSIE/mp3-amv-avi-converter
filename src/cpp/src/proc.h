#pragma once
#include <cstdint>
#include <string>
#include <vector>

// 最小跨平台子进程：argv 数组、可选 stdout/stderr 管道、terminate。
// Windows: CreateProcessW（UTF-8→UTF-16，无控制台窗）；POSIX: fork/exec。
// 取代原 system/_popen 方案（GUI 阶段需要 terminate 语义 + 中文路径 UTF-8 直通）。
class Process {
public:
    Process() = default;
    ~Process();
    Process(const Process&) = delete;
    Process& operator=(const Process&) = delete;

    // 启动；pipe_out/pipe_err 决定对应通道是否接管道（未接管通道重定向 NUL）
    bool spawn(const std::vector<std::string>& argv, bool pipe_out, bool pipe_err);
    size_t read_out(uint8_t* buf, size_t n);   // 阻塞读 stdout 管道，0=EOF
    size_t read_err(uint8_t* buf, size_t n);   // 阻塞读 stderr 管道，0=EOF
    std::string read_err_all();                // 读到 EOF（子进程退出后 stderr 有界时安全）
    bool running();
    void terminate();                          // 硬杀
    int wait(int timeout_ms);                  // 退出码；超时/-1 失败
    void close();                              // 回收句柄/进程（可重复调用）

private:
    bool piped_out_ = false, piped_err_ = false;
#ifdef _WIN32
    void* hproc_ = nullptr, *hthread_ = nullptr;
    void* out_r_ = nullptr, *err_r_ = nullptr;
    void* child_out_w_ = nullptr, *child_err_w_ = nullptr, *child_in_ = nullptr;
#else
    int pid_ = -1, out_r_ = -1, err_r_ = -1;
#endif
};
