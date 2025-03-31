#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <unordered_set>


// User class to store individual user information
class User {
private:
    std::string username;
    std::string password;
    std::string info;
    int wins;
    int losses;
    float rating;
    bool isQuiet;
    std::unordered_set<std::string> blockedUsers;
    std::atomic<int> clientSocket;
    bool isGuest;
    bool isPlaying;
    bool isObserving;
    int gameId;

public:
    User(const std::string& username, const std::string& password, int socket = -1)
        : username(username), password(password), info(""), wins(0), losses(0), rating(1500.0f),
          isQuiet(false), clientSocket(socket), isGuest(username == "guest"),
          isPlaying(false), isObserving(false), gameId(-1) {}

    // Basic getters and setters
    std::string getUsername() const { return username; }
    bool checkPassword(const std::string& pwd) const { return password == pwd; }
    void setPassword(const std::string& pwd) { password = pwd; }
    void setInfo(const std::string& newInfo) { info = newInfo; }
    std::string getInfo() const { return info; }
    int getWins() const { return wins; }
    int getLosses() const { return losses; }
    float getRating() const { return rating; }
    bool isInQuietMode() const { return isQuiet; }
    void setQuietMode(bool quiet) { isQuiet = quiet; }
    int getSocket() const { return clientSocket; }
    void setSocket(int socket) { clientSocket = socket; }
    bool isUserGuest() const { return isGuest; }
    bool isInGame() const { return isPlaying; }
    void setPlaying(bool playing) { isPlaying = playing; }
    bool isUserObserving() const { return isObserving; }
    void setObserving(bool observing) { isObserving = observing; }
    int getGameId() const { return gameId; }
    void setGameId(int id) { gameId = id; }

    // Game statistics methods
    void addWin() {
        wins++;
        updateRating(true);
    }

    void addLoss() {
        losses++;
        updateRating(false);
    }

    // User blocking methods
    void blockUser(const std::string& user) {
        blockedUsers.insert(user);
    }

    void unblockUser(const std::string& user) {
        blockedUsers.erase(user);
    }

    bool isBlocked(const std::string& user) const {
        return blockedUsers.find(user) != blockedUsers.end();
    }

    std::vector<std::string> getBlockedUsers() const {
        std::vector<std::string> result;
        for (const auto& user : blockedUsers) {
            result.push_back(user);
        }
        return result;
    }

private:
    void updateRating(bool won) {
        // Basic ELO-like rating adjustment
        if (won) {
            rating += 15.0f;
        } else {
            rating = std::max(1000.0f, rating - 15.0f);
        }
    }
};

// UserManager singleton to manage all users
class UserManager {
private:
    std::unordered_map<std::string, std::shared_ptr<User>> users;
    std::unordered_map<int, std::string> socketToUser;
    std::mutex usersMutex;

    // Private constructor for singleton
    UserManager() {
        // Create default guest account
        users["guest"] = std::make_shared<User>("guest", "");
    }

public:
    // Get the singleton instance
    static UserManager& getInstance() {
        static UserManager instance;
        return instance;
    }

    // User registration
    bool registerUser(const std::string& username, const std::string& password, int socket) {
        std::lock_guard<std::mutex> lock(usersMutex);

        // Validate username (alphanumeric only)
        if (!isValidUsername(username)) {
            return false;
        }

        // Check if username already exists
        if (users.find(username) != users.end()) {
            return false;
        }

        // Create new user
        users[username] = std::make_shared<User>(username, password, socket);
        socketToUser[socket] = username;

        return true;
    }

    // User login
    bool loginUser(const std::string& username, const std::string& password, int socket) {
        std::lock_guard<std::mutex> lock(usersMutex);

        auto it = users.find(username);
        if (it == users.end() || !it->second->checkPassword(password)) {
            return false;
        }

        // Check if user is already logged in
        if (it->second->getSocket() != -1) {
            return false; // Already logged in elsewhere
        }

        // Update socket and track connection
        it->second->setSocket(socket);
        socketToUser[socket] = username;

        return true;
    }

    // Guest login
    bool loginGuest(int socket) {
        std::lock_guard<std::mutex> lock(usersMutex);
        socketToUser[socket] = "guest";
        return true;
    }

    // User logout
    void logoutUser(int socket) {
        std::lock_guard<std::mutex> lock(usersMutex);

        auto it = socketToUser.find(socket);
        if (it != socketToUser.end()) {
            std::string username = it->second;
            if (username != "guest" && users.find(username) != users.end()) {
                users[username]->setSocket(-1); // Mark as disconnected
            }
            socketToUser.erase(it);
        }
    }

    // Get username by socket
    std::string getUsernameBySocket(int socket) {
        std::lock_guard<std::mutex> lock(usersMutex);

        auto it = socketToUser.find(socket);
        if (it != socketToUser.end()) {
            return it->second;
        }
        return "";
    }

    // Get user by username
    std::shared_ptr<User> getUserByUsername(const std::string& username) {
        std::lock_guard<std::mutex> lock(usersMutex);

        auto it = users.find(username);
        if (it != users.end()) {
            return it->second;
        }
        return nullptr;
    }

    // Get user by socket
    std::shared_ptr<User> getUserBySocket(int socket) {
        std::string username = getUsernameBySocket(socket);
        if (!username.empty()) {
            return getUserByUsername(username);
        }
        return nullptr;
    }

    // Get all online users
    std::vector<std::shared_ptr<User>> getOnlineUsers() {
        std::lock_guard<std::mutex> lock(usersMutex);

        std::vector<std::shared_ptr<User>> result;
        for (const auto& pair : socketToUser) {
            std::string username = pair.second;
            if (username != "guest" && users.find(username) != users.end()) {
                result.push_back(users[username]);
            }
        }
        return result;
    }

    // Check if a user is online
    bool isUserOnline(const std::string& username) {
        auto user = getUserByUsername(username);
        return user && user->getSocket() != -1;
    }

    // Get a list of online usernames (for "who" command)
    std::string getOnlineUsersList() {
        std::lock_guard<std::mutex> lock(usersMutex);

        // Count regular users
        std::vector<std::shared_ptr<User>> onlineRegularUsers;
        for (const auto& pair : socketToUser) {
            std::string username = pair.second;
            if (username != "guest" && users.find(username) != users.end()) {
                onlineRegularUsers.push_back(users[username]);
            }
        }

        // Count guest connections
        int guestCount = 0;
        for (const auto& pair : socketToUser) {
            if (pair.second == "guest") {
                guestCount++;
            }
        }

        // If no one is online (neither guests nor regular users), return a message
        if (onlineRegularUsers.empty() && guestCount == 0) {
            return "No users online.";
        }

        std::string result = "Online users:\n";

        // Add regular users to the list
        for (const auto& user : onlineRegularUsers) {
            result += "- " + user->getUsername();
            if (user->isInGame()) {
                result += " (playing in game " + std::to_string(user->getGameId()) + ")";
            } else if (user->isUserObserving()) {
                result += " (observing game " + std::to_string(user->getGameId()) + ")";
            }
            result += "\n";
        }

        // Add guests to the list
        if (guestCount > 0) {
            if (guestCount == 1) {
                result += "- 1 guest\n";
            } else {
                result += "- " + std::to_string(guestCount) + " guests\n";
            }
        }

        return result;
    }

    // Change user password
    bool changePassword(const std::string& username, const std::string& newPassword) {
        auto user = getUserByUsername(username);
        if (!user) {
            return false;
        }

        user->setPassword(newPassword);
        return true;
    }

    // Update user info
    bool updateUserInfo(const std::string& username, const std::string& info) {
        auto user = getUserByUsername(username);
        if (!user) {
            return false;
        }

        user->setInfo(info);
        return true;
    }

    // Save to disk (you'll implement this later)
    void saveUsers() {
        // TODO: Implement persistence
    }

    // Load from disk (you'll implement this later)
    void loadUsers() {
        // TODO: Implement persistence
    }

private:
    // Validate username (alphanumeric only)
    bool isValidUsername(const std::string& username) {
        if (username.empty() || username == "guest") {
            return false;
        }

        return std::all_of(username.begin(), username.end(), [](char c) {
            return std::isalnum(c) || c == '_';
        });
    }
};

#endif // USER_MANAGER_H