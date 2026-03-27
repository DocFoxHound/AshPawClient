#include <exception>
#include <filesystem>
#include <iostream>

#include "ashpaw/client/ClientApp.hpp"

int main(int argc, char** argv) {
    const auto configPath = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(ASHPAW_DEFAULT_CONFIG_PATH);

    try {
        ashpaw::client::ClientApp app;
        app.Initialize(configPath);
        const auto exitCode = app.Run();
        app.Shutdown();
        return exitCode;
    } catch (const std::exception& exception) {
        std::cerr << "AshPaw Client failed to start: " << exception.what() << '\n';
        return 1;
    }
}
