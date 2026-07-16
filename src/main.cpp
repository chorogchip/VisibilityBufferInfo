#include <Windows.h>
#include <stdexcept>
#include <vector>
#include <string>

#include "util/Utils.h"
#include "Application.h"

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd) {

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    Application app{};
    app.run(hInstance, nShowCmd);

    return 0;
}
