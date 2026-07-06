#include <Windows.h>
#include <stdexcept>
#include <vector>
#include <string>

#include "util/Utils.h"
#include "Application.h"

int WINAPI WinMain(
    _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    try {

        Application app{};
        app.run(hInstance, nShowCmd);

    } catch (const std::exception& e) {

        MessageBoxA(nullptr, (std::string("Fatal error:\n") + e.what()).c_str(), "Error", MB_OK | MB_ICONERROR);
        return -1;

    }

    return 0;
}
