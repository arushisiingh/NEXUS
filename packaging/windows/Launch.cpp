#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t module_path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, module_path, MAX_PATH);
    const std::filesystem::path root = std::filesystem::path(module_path).parent_path();
    const std::filesystem::path backend = root / L"bin" / L"nexus-api.exe";
    const std::filesystem::path frontend = root / L"frontend" / L"dist";
    if (!std::filesystem::exists(backend) || !std::filesystem::exists(frontend / L"index.html")) {
        MessageBoxW(nullptr, L"NEXUS package is incomplete: backend or frontend assets are missing.", L"NEXUS", MB_ICONERROR | MB_OK);
        return 2;
    }
    SetEnvironmentVariableW(L"NEXUS_STATIC_ROOT", frontend.wstring().c_str());
    std::wstring command = L"\"" + backend.wstring() + L"\" 8081";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, root.wstring().c_str(), &startup, &process)) {
        MessageBoxW(nullptr, L"NEXUS backend could not be started.", L"NEXUS", MB_ICONERROR | MB_OK);
        return 3;
    }
    CloseHandle(process.hThread);
    ShellExecuteW(nullptr, L"open", L"http://127.0.0.1:8081/", nullptr, nullptr, SW_SHOWNORMAL);
    WaitForSingleObject(process.hProcess, INFINITE);
    CloseHandle(process.hProcess);
    return 0;
}
#else
int main() { return 1; }
#endif
