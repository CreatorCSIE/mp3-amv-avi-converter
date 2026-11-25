#include "proc.h"

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {
std::wstring to_w(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}
// CreateProcessW 命令行引号规则（MSVC 标准 argv 拼接算法）
std::wstring quote_arg(const std::wstring& a) {
    if (!a.empty() && a.find_first_of(L" \t\n\v\"") == std::wstring::npos) return a;
    std::wstring out = L"\"";
    for (size_t i = 0; i < a.size(); i++) {
        size_t bs = 0;
        while (i < a.size() && a[i] == L'\\') { i++; bs++; }
        if (i == a.size()) { out.append(bs * 2, L'\\'); break; }
        if (a[i] == L'"') { out.append(bs * 2 + 1, L'\\'); out.push_back(L'"'); }
        else { out.append(bs, L'\\'); out.push_back(a[i]); }
    }
    out.push_back(L'"');
    return out;
}
}  // namespace

Process::~Process() { close(); }

bool Process::spawn(const std::vector<std::string>& argv, bool pipe_out, bool pipe_err) {
    if (argv.empty()) return false;
    piped_out_ = pipe_out; piped_err_ = pipe_err;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    auto make_pipe = [&](void*& rd, void*& wr) -> bool {
        HANDLE r, w;
        if (!CreatePipe(&r, &w, &sa, 0)) return false;
        SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);  // 读端仅父进程持有
        rd = r; wr = w;
        return true;
    };
    HANDLE nul_w = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, 0, nullptr);
    HANDLE nul_r = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, 0, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul_r;
    if (pipe_out) { if (!make_pipe(out_r_, child_out_w_)) { CloseHandle(nul_r); CloseHandle(nul_w); return false; } si.hStdOutput = child_out_w_; }
    else si.hStdOutput = nul_w;
    if (pipe_err) { if (!make_pipe(err_r_, child_err_w_)) { if (out_r_) CloseHandle(out_r_); CloseHandle(nul_r); CloseHandle(nul_w); return false; } si.hStdError = child_err_w_; }
    else si.hStdError = nul_w;
    child_in_ = nul_r;

    std::wstring cmd;
    for (auto& a : argv) { if (!cmd.empty()) cmd += L' '; cmd += quote_arg(to_w(a)); }

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    BOOL ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    // 父进程侧立即关闭继承写端，否则子进程退出后读不到 EOF
    if (child_out_w_) { CloseHandle(child_out_w_); child_out_w_ = nullptr; }
    if (child_err_w_) { CloseHandle(child_err_w_); child_err_w_ = nullptr; }
    CloseHandle(nul_r); CloseHandle(nul_w);
    if (!ok) {
        if (out_r_) { CloseHandle(out_r_); out_r_ = nullptr; }
        if (err_r_) { CloseHandle(err_r_); err_r_ = nullptr; }
        return false;
    }
    hproc_ = pi.hProcess; hthread_ = pi.hThread;
    return true;
}

size_t Process::read_out(uint8_t* buf, size_t n) {
    if (!out_r_) return 0;
    DWORD got = 0;
    if (!ReadFile((HANDLE)out_r_, buf, (DWORD)n, &got, nullptr)) return 0;
    return got;
}

size_t Process::read_err(uint8_t* buf, size_t n) {
    if (!err_r_) return 0;
    DWORD got = 0;
    if (!ReadFile((HANDLE)err_r_, buf, (DWORD)n, &got, nullptr)) return 0;
    return got;
}

bool Process::running() {
    if (!hproc_) return false;
    return WaitForSingleObject((HANDLE)hproc_, 0) == WAIT_TIMEOUT;
}

void Process::terminate() {
    if (hproc_) TerminateProcess((HANDLE)hproc_, 1);
}

int Process::wait(int timeout_ms) {
    if (!hproc_) return -1;
    if (WaitForSingleObject((HANDLE)hproc_, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms) != WAIT_OBJECT_0)
        return -1;
    DWORD rc = 1;
    GetExitCodeProcess((HANDLE)hproc_, &rc);
    return (int)rc;
}

void Process::close() {
    if (hproc_) { WaitForSingleObject((HANDLE)hproc_, 2000); CloseHandle((HANDLE)hproc_); hproc_ = nullptr; }
    if (hthread_) { CloseHandle((HANDLE)hthread_); hthread_ = nullptr; }
    if (out_r_) { CloseHandle((HANDLE)out_r_); out_r_ = nullptr; }
    if (err_r_) { CloseHandle((HANDLE)err_r_); err_r_ = nullptr; }
    if (child_out_w_) { CloseHandle((HANDLE)child_out_w_); child_out_w_ = nullptr; }
    if (child_err_w_) { CloseHandle((HANDLE)child_err_w_); child_err_w_ = nullptr; }
    if (child_in_) { CloseHandle((HANDLE)child_in_); child_in_ = nullptr; }
}

#else  // ---------------- POSIX ----------------
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>

Process::~Process() { close(); }

bool Process::spawn(const std::vector<std::string>& argv, bool pipe_out, bool pipe_err) {
    if (argv.empty()) return false;
    piped_out_ = pipe_out; piped_err_ = pipe_err;
    int op[2] = {-1, -1}, ep[2] = {-1, -1};
    if (pipe_out && pipe(op) != 0) return false;
    if (pipe_err && pipe(ep) != 0) { if (op[0] >= 0) close(op[0]), close(op[1]); return false; }
    std::vector<char*> cargv;
    for (auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);
    pid_t pid = fork();
    if (pid < 0) {
        if (op[0] >= 0) close(op[0]), close(op[1]);
        if (ep[0] >= 0) close(ep[0]), close(ep[1]);
        return false;
    }
    if (pid == 0) {
        int nul = open("/dev/null", O_RDWR);
        if (pipe_out) { dup2(op[0], 0); dup2(op[1], 1); } else dup2(nul, 1);
        if (pipe_err) dup2(ep[1], 2); else dup2(nul, 2);
        dup2(nul, 0);
        if (op[0] >= 0) { close(op[0]); close(op[1]); }
        if (ep[0] >= 0) { close(ep[0]); close(ep[1]); }
        if (nul > 2) close(nul);
        execvp(cargv[0], cargv.data());
        _exit(127);
    }
    if (op[1] >= 0) close(op[1]);
    if (ep[1] >= 0) close(ep[1]);
    out_r_ = pipe_out ? op[0] : -1;
    err_r_ = pipe_err ? ep[0] : -1;
    pid_ = pid;
    return true;
}

size_t Process::read_out(uint8_t* buf, size_t n) {
    if (out_r_ < 0) return 0;
    ssize_t r = ::read(out_r_, buf, n);
    return r > 0 ? (size_t)r : 0;
}
size_t Process::read_err(uint8_t* buf, size_t n) {
    if (err_r_ < 0) return 0;
    ssize_t r = ::read(err_r_, buf, n);
    return r > 0 ? (size_t)r : 0;
}
bool Process::running() {
    if (pid_ <= 0) return false;
    int st;
    pid_t r = waitpid(pid_, &st, WNOHANG);
    if (r == 0) return true;
    return false;  // 已退出；wait() 再收割（僵尸短暂存在无碍）
}
void Process::terminate() { if (pid_ > 0) kill(pid_, SIGTERM); }
int Process::wait(int timeout_ms) {
    if (pid_ <= 0) return -1;
    int st = 0;
    if (timeout_ms < 0) { waitpid(pid_, &st, 0); }
    else {
        int waited = 0;
        while (waitpid(pid_, &st, WNOHANG) == 0) {
            if (waited >= timeout_ms) return -1;
            usleep(10000); waited += 10;
        }
    }
    pid_ = -1;
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}
void Process::close() {
    if (out_r_ >= 0) { close(out_r_); out_r_ = -1; }
    if (err_r_ >= 0) { close(err_r_); err_r_ = -1; }
    if (pid_ > 0) { terminate(); wait(1000); }
}
#endif

std::string Process::read_err_all() {
    std::string out;
    uint8_t buf[4096];
    size_t n;
    while ((n = read_err(buf, sizeof(buf))) > 0) out.append((const char*)buf, n);
    return out;
}
