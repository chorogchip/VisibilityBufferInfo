#include <Windows.h>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

#include "util/Logger.h"
#include "util/Utils.h"
#include "Application.h"

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd) {

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    try {
        Application app{};
        app.run(hInstance, nShowCmd);
        util::Logger::g_logger.flush();
    }
    catch (const std::exception& exception) {
        util::Logger::g_logger <<
            "Unhandled application exception: " <<
            exception.what() << '\n';
        util::Logger::g_logger.flush();
        return EXIT_FAILURE;
    }
    catch (...) {
        util::Logger::g_logger <<
            "Unhandled application exception: unknown exception\n";
        util::Logger::g_logger.flush();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
