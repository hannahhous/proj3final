#ifndef USER_H
#define USER_H

#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <thread>
#include <atomic>

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
    std::mutex userMutex;
    int clientSocket;
    bool isGuest;
    bool isPlaying;
    bool isObserving;
    int gameId;

public:


    User(const std::string& username, const std::string& password, int socket)
        : username(username), password(password), info(""), wins(0), losses(0), rating(1500.0f),
          isQuiet(false), clientSocket(socket), isGuest(username == "guest"),
          isPlaying(false), isObserving(false), gameId(-1) {

          }

    // Getters
    std::string getPassword() const { return password; }
    std::string getUsername() const { return username; }
    std::string getInfo() const { return info; }
    int getWins() const { return wins; }
    int getLosses() const { return losses; }
    float getRating() const { return rating; }
    int getSocket() const { return clientSocket; }
    int getGameId() const { return gameId; }

    // setters
    void setPassword(const std::string& pwd) { password = pwd; }
    void setInfo(const std::string& newInfo) { info = newInfo; }
    void setQuietMode(bool quiet) { isQuiet = quiet; }
    void setSocket(int socket) { clientSocket = socket; }
    void setPlaying(bool playing) { isPlaying = playing; }
    void setObserving(bool observing) { isObserving = observing; }
    void setGameId(int id) { gameId = id; }

    // Checks
    bool isInQuietMode() const { return isQuiet; }
    bool isUserGuest() const { return isGuest; }
    bool isInGame() const { return isPlaying; }
    bool isUserObserving() const { return isObserving; }
    bool checkPassword(const std::string& pwd) const { return password == pwd; }

    // stats functions
    void addWin() { wins++; updateRating(true); }
    void addLoss() { losses++; updateRating(false); }

    // blocking functions
    void blockUser(const std::string& user) {
        std::lock_guard<std::mutex> lock(userMutex);
        blockedUsers.insert(user);
    }

    void unblockUser(const std::string& user) {
        std::lock_guard<std::mutex> lock(userMutex);
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
        // 1500 point rating system with each gaem at 15 pts
        if (won) {
            rating += 15.0f;
        } else {
            rating = std::max(1000.0f, rating - 15.0f);
        }
    }
};

// UserManager class
class UserManager {
private:
    std::unordered_map<std::string, std::shared_ptr<User>> users;
    std::unordered_map<int, std::string> socketToUser;
    std::mutex usersMutex;
    std::thread autosaveThread;
    std::atomic<bool> running;

    UserManager() : running(true) {
        // create guest account
        users["guest"] = std::make_shared<User>("guest", "", -1);

        // Load existing users
        loadUsers();

        // Start autosave thread to run over course of program
        autosaveThread = std::thread(&UserManager::autosaveLoop, this);
        autosaveThread.detach();
    }



    // Autosave function
    void autosaveLoop() {
        const int SAVE_INTERVAL_SECONDS = 300; // Save every 5 minutes

        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(SAVE_INTERVAL_SECONDS));
            std::cout << "Auto-saving user data..." << std::endl;
            saveUsers();
            std::cout << "Auto-save completed" << std::endl;
        }
    }

public:

    ~UserManager() {
        running = false;
        saveUsers(); // Final save on shutdown
    }

    // Get the instance
    static UserManager& getInstance() {
        static UserManager instance;
        return instance;
    }

    // User registration
    bool registerUser(const std::string& username, const std::string& password, int socket) {
        std::lock_guard<std::mutex> lock(usersMutex);

        // Check if username already exists
        if (users.find(username) != users.end()) {
            return false;
        }

        // Create new user
        users[username] = std::make_shared<User>(username, password, socket);
        socketToUser[socket] = username;

        // Save changes
        saveUsers();

        return true;
    }

    // User login
    bool loginUser(const std::string& username, const std::string& password, int socket) {
        std::lock_guard<std::mutex> lock(usersMutex);

        auto it = users.find(username);
        if (it == users.end() || !it->second->checkPassword(password)) {
            return false;
        }

        // Update socket
        it->second->setSocket(socket);
        socketToUser[socket] = username;

        return true;
    }

    bool loginGuest(int socket) {
        std::lock_guard<std::mutex> lock(usersMutex);
        socketToUser[socket] = "guest";
        return true;
    }

    void logoutUser(int socket) {
        std::lock_guard<std::mutex> lock(usersMutex);

        auto it = socketToUser.find(socket);
        if (it != socketToUser.end()) {
            std::string username = it->second;
            if (username != "guest" && users.find(username) != users.end()) {
                users[username]->setSocket(-1); // Mark user as disconnected
            }
            socketToUser.erase(it);
        }
    }

    std::string getUsernameBySocket(int socket) {
        std::lock_guard<std::mutex> lock(usersMutex);

        auto it = socketToUser.find(socket);
        if (it != socketToUser.end()) {
            return it->second;
        }
        return "";
    }

    std::shared_ptr<User> getUserByUsername(const std::string& username) {
        std::lock_guard<std::mutex> lock(usersMutex);

        auto it = users.find(username);
        if (it != users.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::shared_ptr<User> getUserBySocket(int socket) {
        std::string username = getUsernameBySocket(socket);
        if (!username.empty()) {
            return getUserByUsername(username);
        }
        return nullptr;
    }

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


    bool saveUsers()
    {
        std::cout << "Attempting to save users data..." << std::endl;

        // had problems with getting lock so use try_lock
        if (!usersMutex.try_lock()) {
            return false;
        }

        try {
            // Open file for writing
            std::ofstream file("users_data.txt");
            if (!file.is_open()) {
                std::cerr << "Failed to open users_data.txt for writing" << std::endl;
                usersMutex.unlock();
                return false;
            }

            int userCount = 0;
            // Write each user
            for (const auto& pair : users) {
                const auto& user = pair.second;

                // Skip guest accounts
                if (user->getUsername() == "guest") {
                    continue;
                }

                userCount++;
                file << "USER_BEGIN\n";
                file << "username=" << user->getUsername() << "\n";
                file << "password=" << user->getPassword() << "\n";
                file << "info=" << user->getInfo() << "\n";
                file << "wins=" << user->getWins() << "\n";
                file << "losses=" << user->getLosses() << "\n";
                file << "rating=" << user->getRating() << "\n";
                file << "quiet=" << (user->isInQuietMode() ? "1" : "0") << "\n";

                // Write blocked users
                file << "blocked_begin\n";
                for (const auto& blockedUser : user->getBlockedUsers()) {
                    file << blockedUser << "\n";
                }
                file << "blocked_end\n";

                file << "USER_END\n";
            }

            file.flush();
            file.close();
            std::cout << "User data saved successfully: " << userCount << " users written" << std::endl;
            usersMutex.unlock();
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error saving users: " << e.what() << std::endl;
            usersMutex.unlock();
            return false;
        }
    }

    void loadUsers() {
        std::lock_guard<std::mutex> lock(usersMutex);

        try {
            // Try to open the file
            std::ifstream file("users_data.txt");
            if (!file.is_open()) {
                std::cout << "No user data file found. Starting with fresh user database." << std::endl;
                return;
            }

            std::string line;
            std::string username, password, info;
            int wins = 0, losses = 0;
            float rating [[maybe_unused]] = 1500.0f;    // Used AI to find this compiler flag because I was getting problems
            bool isQuiet = false;
            std::vector<std::string> blockedUsers;
            bool inBlockedSection = false;
            bool inUserSection = false;

            while (std::getline(file, line)) {
                if (line == "USER_BEGIN") {
                    inUserSection = true;
                    username = password = info = "";
                    wins = losses = 0;
                    rating = 1500.0f;
                    isQuiet = false;
                    blockedUsers.clear();
                    continue;
                }
                else if (line == "USER_END") {
                    if (inUserSection) {
                        // Create user
                        auto user = std::make_shared<User>(username, password, -1);
                        user->setInfo(info);

                        // Set wins and losses
                        for (int i = 0; i < wins; i++) {
                            user->addWin();
                        }
                        for (int i = 0; i < losses; i++) {
                            user->addLoss();
                        }

                        user->setQuietMode(isQuiet);

                        // Add blocked users
                        for (const auto& blockedUser : blockedUsers) {
                            user->blockUser(blockedUser);
                        }

                        users[username] = user;
                        std::cout << "Loaded user: " << username << std::endl;
                    }
                    inUserSection = false;
                    continue;
                }

                if (inUserSection) {
                    if (line == "blocked_begin") {
                        inBlockedSection = true;
                        continue;
                    }
                    else if (line == "blocked_end") {
                        inBlockedSection = false;
                        continue;
                    }

                    if (inBlockedSection) {
                        blockedUsers.push_back(line);
                    }
                    else {
                        size_t equalPos = line.find('=');
                        if (equalPos != std::string::npos) {
                            std::string key = line.substr(0, equalPos);
                            std::string value = line.substr(equalPos + 1);

                            if (key == "username") username = value;
                            else if (key == "password") password = value;
                            else if (key == "info") info = value;
                            else if (key == "wins") {
                                try { wins = std::stoi(value); }
                                catch (...) { wins = 0; }
                            }
                            else if (key == "losses") {
                                try { losses = std::stoi(value); }
                                catch (...) { losses = 0; }
                            }
                            else if (key == "rating") {
                                try { rating = std::stof(value); }
                                catch (...) { rating = 1500.0f; }
                            }
                            else if (key == "quiet") isQuiet = (value == "1");
                        }
                    }
                }
            }

            file.close();
            std::cout << "Loaded " << (users.size() - 1) << " user accounts from save." << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error loading users: " << e.what() << std::endl;
        }
    }

    bool updateUserInfo(const std::string& username, const std::string& info) {
        auto user = getUserByUsername(username);
        if (!user) {
            return false;
        }

        user->setInfo(info);
        saveUsers(); // Save after updating info
        return true;
    }

    bool changePassword(const std::string& username, const std::string& newPassword) {
        auto user = getUserByUsername(username);
        if (!user) {
            return false;
        }

        user->setPassword(newPassword);

        saveUsers(); // Save after changing password
        return true;
    }

    std::string getOnlineUsersList() {
        std::lock_guard<std::mutex> lock(usersMutex);

        // regular users
        std::vector<std::shared_ptr<User>> onlineRegularUsers;
        for (const auto& pair : socketToUser) {
            std::string username = pair.second;
            if (username != "guest" && users.find(username) != users.end()) {
                onlineRegularUsers.push_back(users[username]);
            }
        }

        // guests
        int guestCount = 0;
        for (const auto& pair : socketToUser) {
            if (pair.second == "guest") {
                guestCount++;
            }
        }

        // If no one online
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
};

#endif // USER_H