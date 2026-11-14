//
// Zuma Deluxe - Android/desktop entry point.
//
// Upstream (Windows) entry was WinMain in Main.cpp; SDL2main provides WinMain on
// Windows and SDLActivity drives this on Android, so a plain main() is correct
// for every target.
//
#include <SDL_main.h>

#include "Zuma/Zuma_Prefix.pch"
#include "Zuma/CircleShootApp.h"
#include "SexyAppFramework/Common.h"

#ifdef __ANDROID__
#include <SDL.h>
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <ucontext.h>
#include <dlfcn.h>

// Diagnostics: capture every framework log line to a file from the very start,
// so a load hang can be read off the device without adb. The Java layer
// (ZumaActivity) mirrors this file into public Downloads as zuma-diag.txt.
static char gDiagLogPath[512] = {0};

// Crash reporter: writes the faulting PC straight to the log with raw syscalls
// so it survives a native crash. Symbolize the pc against the unstripped
// libmain.so to get the exact crash line.
static void Zuma_CrashHandler(int sig, siginfo_t* info, void* uctx)
{
    uintptr_t pc = 0;
#if defined(__aarch64__)
    if (uctx) pc = ((ucontext_t*)uctx)->uc_mcontext.pc;
#elif defined(__arm__)
    if (uctx) pc = ((ucontext_t*)uctx)->uc_mcontext.arm_pc;
#elif defined(__x86_64__)
    if (uctx) pc = ((ucontext_t*)uctx)->uc_mcontext.gregs[REG_RIP];
#endif
    uintptr_t fault = info ? (uintptr_t)info->si_addr : 0;
    uintptr_t base = 0;
    Dl_info dlinfo;
    if (pc && dladdr((const void*)pc, &dlinfo) && dlinfo.dli_fbase)
        base = (uintptr_t)dlinfo.dli_fbase;
    if (gDiagLogPath[0])
    {
        int fd = open(gDiagLogPath, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0)
        {
            char buf[240];
            int n = snprintf(buf, sizeof(buf),
                "[zuma-diag] *** NATIVE CRASH sig=%d fault_addr=0x%lx pc=0x%lx base=0x%lx elf=0x%lx\n",
                sig, (unsigned long)fault, (unsigned long)pc,
                (unsigned long)base, (unsigned long)(pc - base));
            if (n > 0) write(fd, buf, (size_t)n);
            close(fd);
        }
    }
    // Give the Java mirror thread time to push this crash line to Downloads
    // (it polls every ~2s). The crashing thread just stalls here briefly.
    struct timespec ts = { 4, 0 };
    nanosleep(&ts, nullptr);
    _exit(128 + sig);
}

static void Zuma_InstallCrashHandler()
{
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = Zuma_CrashHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
}

static void Zuma_RegisterDiagLog()
{
    if (const char* aExt = SDL_AndroidGetExternalStoragePath())
    {
        std::string aLogPath(aExt);
        if (!aLogPath.empty() && aLogPath.back() != '/') aLogPath += '/';
        aLogPath += "zuma-diag.log";
        std::snprintf(gDiagLogPath, sizeof(gDiagLogPath), "%s", aLogPath.c_str());
        Sexy::RegisterLogFileSink(aLogPath);
        Sexy::LogInfoLn("[zuma-diag] === Zuma diag log start === path={}", aLogPath);
    }
    else
    {
        Sexy::LogErrorLn("[zuma-diag] SDL_AndroidGetExternalStoragePath() returned null");
    }
    Zuma_InstallCrashHandler();
}
#endif

int main(int argc, char** argv)
{
#ifdef __ANDROID__
    Zuma_RegisterDiagLog();
#endif

    Sexy::CircleShootApp* anApp = new Sexy::CircleShootApp();
    // Resource and save folders must be selected before Init reads them.
    anApp->SetArgs(argc, argv);
    anApp->DoParseCmdLine();
    anApp->Init();
    anApp->Start();
    anApp->Shutdown();
    delete anApp;

    return 0;
}
