#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <regex>
#include "User.h"
#include "Game.h"
#include "Message.h"

class CommandHandler {
private:
    int clientSocket;
    std::string username;

public:
    CommandHandler(int socket) : clientSocket(socket), username("") {}

    void setUsername(const std::string& name) {
        username = name;
    }

    std::string getUsername() const {
        return username;
    }

    bool isLoggedIn() const {
        return !username.empty();
    }

    std::string processCommand(const std::string& command) {
        // Split the command and arguments
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;

        // Convert command to lowercase for case-insensitive matching
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        // Get the current user
        std::shared_ptr<User> currentUser = nullptr;
        if (!username.empty()) {
            currentUser = UserManager::getInstance().getUserByUsername(username);
        }

        // Process login or guest login first if not logged in
        if (username.empty()) {
            if (cmd == "login") {
                std::string user, pass;
                iss >> user >> pass;

                if (user.empty() || pass.empty()) {
                    return "Usage: login <username> <password>";
                }

                if (UserManager::getInstance().loginUser(user, pass, clientSocket)) {
                    username = user;

                    // Check for unread messages
                    int unreadCount = MessageManager::getInstance().countUnreadMessages(username);
                    if (unreadCount > 0) {
                        return "Login successful. You have " + std::to_string(unreadCount) + " unread messages.";
                    }
                    return "Login successful.";
                } else {
                    return "Login failed. Invalid username or password.";
                }
            } else if (cmd == "guest") {
                // Login as guest
                UserManager::getInstance().loginGuest(clientSocket);
                username = "guest";
                return "Logged in as guest. You can register a new account using 'register <username> <password>'.";
            } else if (cmd == "register") {
                // Only allow registration if logged in as guest
                return "Please login as guest first to register.";
            } else {
                return "Please login first using 'login <username> <password>' or 'guest'.";
            }
        }

        // Process commands for logged in users
        if (cmd == "register") {
            // Only allow registration for guest users
            if (username != "guest") {
                return "You must be logged in as guest to register.";
            }

            std::string newUser, newPass;
            iss >> newUser >> newPass;

            if (newUser.empty() || newPass.empty()) {
                return "Usage: register <username> <password>";
            }

            if (UserManager::getInstance().registerUser(newUser, newPass, clientSocket)) {
                username = newUser;
                return "Registration successful. You are now logged in as " + newUser;
            } else {
                return "Registration failed. Username already exists.";
            }
        } else if (cmd == "who") {
            return listOnlineUsers();
        } else if (cmd == "stats") {
            std::string targetUser;
            iss >> targetUser;

            if (targetUser.empty()) {
                targetUser = username;
            }

            return displayUserStats(targetUser);
        } else if (cmd == "game") {
            return listCurrentGames