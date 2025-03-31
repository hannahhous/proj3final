#include <iostream>
#include <signal.h>

#include "TelnetServer.h"

volatile sig_atomic_t shouldExit = 0;

void signalHandler(int signal)
{
    shouldExit = 1;
}

int main()
{
    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    int port = 8023;

    TelnetServer server;
    if (!server.start(port))
    {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    std::cout << "Server running. Press Ctrl+C to stop." << std::endl;

    // Main loop
    while (!shouldExit)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Shutting down..." << std::endl;
    server.stop();    return 0;
}