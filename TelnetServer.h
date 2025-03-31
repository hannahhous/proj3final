#ifndef TELNETSERVER_H
#define TELNETSERVER_H

#include <cstdio>
#include <iostream>
#include <ostream>
#include <sys/socket.h>
#include <thread>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <mutex>

#include "SocketUtils.h"
#include "TelnetClientHandler.h"
#include "Game.h"

class TelnetServer
{
public:
    TelnetServer() : serverSocket(-1), running(false) {}

    bool start(int port)
    {
        // Create a socket.
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0)
        {
            perror("socket");
            return false;
        }

        // Set socket options
        int opt = 1;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        {
            perror("setsockopt");
            close(serverSocket);
            return false;
        }

        // Socket binding address info
        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;  // IPv4
        serverAddr.sin_addr.s_addr = INADDR_ANY;  // Listen on all available interfaces
        serverAddr.sin_port = htons(port);  // Convert from host byte order to network byte order

        // Bind the socket to a local address and port number
        if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
        {
            // If failed, close socket and return error
            perror("bind");
            close(serverSocket);
            return false;
        }

        if (listen(serverSocket, 10) < 0) {
            perror("listen");
            close(serverSocket);
            return false;
        }

        // Set the server to non-blocking mode
        if (!SocketUtils::setNonBlocking(serverSocket))
        {
            close(serverSocket);
            return false;
        }

        running = true;

        // Start the thread to accept new connections
        acceptThread = std::thread(&TelnetServer::acceptConnections, this);

        // Start the game cleanup thread
        cleanupThread = std::thread(&TelnetServer::cleanupGames, this);

        // Start the game timeout checking thread
        gameTimeoutThread = std::thread(&TelnetServer::checkGameTimeouts, this);

        std::cout << "Gomoku server started on port " << port << std::endl;
        return true;
    }

    void stop()
    {
        running = false;

        if (acceptThread.joinable())
        {
            acceptThread.join();
        }

        if (cleanupThread.joinable())
        {
            cleanupThread.join();
        }

        if (gameTimeoutThread.joinable())
        {
            gameTimeoutThread.join();
        }

        {
            std::lock_guard<std::mutex> lock(mutex);

            // Disconnect all clients
            for (auto& client : clients)
            {
                client->disconnect();
            }
            clients.clear();
        }

        // Save user data before stopping
        std::cout << "Saving user data before server shutdown" << std::endl;
        UserManager::getInstance().saveUsers();
        std::cout << "User data saved successfully" << std::endl;

        std::cout << "Saving message data before server shutdown" << std::endl;
        MessageManager::getInstance().saveMessages();
        std::cout << "Message data saved successfully" << std::endl;

        if (serverSocket >= 0)
        {
            close(serverSocket);
            serverSocket = -1;
        }

        std::cout << "Server stopped" << std::endl;
    }

    void broadcastMessage(const std::string& msg, const std::string& excludeUsername = "")
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& client : clients)
        {
            if (client->isLoggedIn() && client->getUsername() != excludeUsername)
            {
                // Check if user is in quiet mode
                auto user = UserManager::getInstance().getUserByUsername(client->getUsername());
                if (user && !user->isInQuietMode())
                {
                    client->sendMessage(msg);
                }
            }
        }
    }

private:
    void acceptConnections()
    {
        while (running)
        {
            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);

            // non-blocking accept
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);

            if (clientSocket < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // No pending connections so sleep a bit
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }
                else
                {
                    perror("accept");
                    continue;
                }
            }

            // Set to non-blocking mode
            if (!SocketUtils::setNonBlocking(clientSocket))
            {
                close(clientSocket);
                continue;
            }

            // Create client handler
            std::lock_guard<std::mutex> lock(mutex);
            clients.push_back(std::make_shared<TelnetClientHandler>(clientSocket));

            // Log connection
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
            std::cout << "New connection from " << clientIP << ":" << ntohs(clientAddr.sin_port) << std::endl;
        }
    }

    void cleanupGames()
    {
        // Save messages every 5 minutes (300 seconds)
        const int SAVE_INTERVAL = 300;
        time_t lastSaveTime = time(nullptr);
        while (running)
        {
            time_t currentTime = time(nullptr);

            // Clean up finished games
            GameManager::getInstance().cleanupGames();

            // Check for disconnected clients
            {
                std::lock_guard<std::mutex> lock(mutex);
                auto it = clients.begin();
                while (it != clients.end())
                {
                    if (!(*it)->isConnected())
                    {
                        it = clients.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            // Periodically save messages
            if (currentTime - lastSaveTime >= SAVE_INTERVAL) {
                std::cout << "Periodic message save..." << std::endl;
                MessageManager::getInstance().saveMessages();
                lastSaveTime = currentTime;
            }

            // Sleep for a while
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }

    void checkGameTimeouts()
    {
        while (running)
        {
            auto games = GameManager::getInstance().getAllGames();

            for (auto& game : games)
            {
                if (game->getStatus() == GameStatus::PLAYING)
                {
                    if (game->checkTimeExpired())
                    {
                        // A game has ended due to timeout
                        std::cout << "Game " << game->getId() << " ended due to timeout" << std::endl;

                        // Notify players
                        std::string timeoutMsg = "Game ended: " + game->getWinner() + " wins due to timeout.";

                        auto blackPlayer = game->getBlackPlayer();
                        auto whitePlayer = game->getWhitePlayer();

                        if (blackPlayer->getSocket() != -1)
                        {
                            SocketUtils::sendData(blackPlayer->getSocket(), timeoutMsg + "\r\n");
                        }

                        if (whitePlayer->getSocket() != -1)
                        {
                            SocketUtils::sendData(whitePlayer->getSocket(), timeoutMsg + "\r\n");
                        }

                        // Notify observers
                        for (int observerSocket : game->getObservers())
                        {
                            SocketUtils::sendData(observerSocket, timeoutMsg + "\r\n");
                        }
                    }
                }
            }

            // Check every second
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    int serverSocket;
    std::atomic<bool> running;
    std::thread acceptThread;
    std::thread cleanupThread;
    std::thread gameTimeoutThread;
    std::vector<std::shared_ptr<TelnetClientHandler>> clients;
    std::mutex mutex;
};

#endif //TELNETSERVER_H
