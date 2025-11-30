/**
 * RigidLabeler Launcher
 * 
 * 纯 Windows API 实现 - 无需 Qt 依赖
 * 编译命令: g++ -o RigidLabeler.exe main.cpp -mwindows
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // 获取程序所在目录
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string appDir(exePath);
    size_t lastSlash = appDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        appDir = appDir.substr(0, lastSlash);
    }

    // 可执行文件路径
    // backend 在 rigidlabeler_backend 子目录中
    std::string backendPath = appDir + "\\rigidlabeler_backend\\rigidlabeler_backend.exe";
    std::string backendWorkDir = appDir + "\\rigidlabeler_backend";
    std::string frontendPath = appDir + "\\frontend.exe";

    // 启动后端进程（无窗口）
    STARTUPINFOA siBackend = {};
    siBackend.cb = sizeof(siBackend);
    PROCESS_INFORMATION piBackend = {};
    
    BOOL backendStarted = CreateProcessA(
        backendPath.c_str(),
        NULL,
        NULL, NULL,
        FALSE,
        CREATE_NO_WINDOW,  // 后端无窗口
        NULL,
        backendWorkDir.c_str(),
        &siBackend,
        &piBackend
    );

    if (!backendStarted) {
        MessageBoxA(NULL, "Failed to start backend", "Error", MB_ICONERROR);
        return 1;
    }

    // 等待后端启动
    Sleep(1500);

    // 启动前端进程
    STARTUPINFOA siFrontend = {};
    siFrontend.cb = sizeof(siFrontend);
    PROCESS_INFORMATION piFrontend = {};
    
    BOOL frontendStarted = CreateProcessA(
        frontendPath.c_str(),
        NULL,
        NULL, NULL,
        FALSE,
        0,
        NULL,
        appDir.c_str(),
        &siFrontend,
        &piFrontend
    );

    if (!frontendStarted) {
        TerminateProcess(piBackend.hProcess, 0);
        CloseHandle(piBackend.hProcess);
        CloseHandle(piBackend.hThread);
        MessageBoxA(NULL, "Failed to start frontend", "Error", MB_ICONERROR);
        return 1;
    }

    // 等待前端关闭
    WaitForSingleObject(piFrontend.hProcess, INFINITE);
    CloseHandle(piFrontend.hProcess);
    CloseHandle(piFrontend.hThread);

    // 关闭后端
    TerminateProcess(piBackend.hProcess, 0);
    CloseHandle(piBackend.hProcess);
    CloseHandle(piBackend.hThread);

    return 0;
}
