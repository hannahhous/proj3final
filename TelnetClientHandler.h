#ifndef TELNETCLIENTHANDLER_H
#define TELNETCLIENTHANDLER_H
#include <thread>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
//#include "UserManager.h"
#include "Game.h"
#include "Message.h"
#include <regex>
#include <iostream>
#include <fstream>  // Add this line to include ofstream


class TelnetClientHandler {
private:
    int clientSocket;
    std::atomic<bool> running;
    std::thread handlerThread;
    std::string username; // To track logged-in user

public:
    // Add to TelnetClientHandler.h in the public section
    bool isLoggedIn() const
    {
        return !username.empty();
    }

    // Add this getter for the username
    std::string getUsername() const
    {
        return username;
    }
    // Add to TelnetClientHandler.h in the public section
    bool isConnected() const
    {
        return running && clientSocket >= 0;
    }
    TelnetClientHandler(int socket)
        : clientSocket(socket), running(true), username("")
    {
        // Start the handler thread
        handlerThread = std::thread(&TelnetClientHandler::handleClient, this);
        handlerThread.detach();
    }

    ~TelnetClientHandler()
    {
        running = false;
    }

    bool sendMessage(const std::string& message) const
    {
        if (clientSocket >= 0) {
            return SocketUtils::sendData(clientSocket, message + "\r\n");
        }
        return false;    }

    void disconnect()
    {
        if (running) {
            running = false;

            // Handle game abandonment if the user is in a game
            if (!username.empty()) {
                auto currentUser = UserManager::getInstance().getUserByUsername(username);
                if (currentUser && currentUser->isInGame()) {
                    int gameId = currentUser->getGameId();
                    auto game = GameManager::getInstance().getGame(gameId);
                    if (game) {
                        // Handle player disconnection in the game
                        handlePlayerDisconnection(game, currentUser);
                    }
                }

                // Log out user
                UserManager::getInstance().logoutUser(clientSocket);
                username = "";
            }

            // Close the socket if it's valid
            if (clientSocket >= 0) {
                close(clientSocket);
                clientSocket = -1;
            }
        }
    }

    // Helper method to handle player disconnection during a game
    void handlePlayerDisconnection(std::shared_ptr<Game> game, std::shared_ptr<User> player) {
        // Get the opponent
        std::shared_ptr<User> opponent;
        if (player->getUsername() == game->getBlackPlayer()->getUsername()) {
            opponent = game->getWhitePlayer();
        } else {
            opponent = game->getBlackPlayer();
        }

        // Notify the opponent and observers that this player disconnected
        std::string disconnectMsg = player->getUsername() + " has disconnected. " +
                                    opponent->getUsername() + " wins by default.";

        if (opponent->getSocket() != -1) {
            SocketUtils::sendData(opponent->getSocket(), disconnectMsg + "\r\n");
        }

        // Notify observers
        for (int observerSocket : game->getObservers()) {
            SocketUtils::sendData(observerSocket, disconnectMsg + "\r\n");
        }

        // End the game with the opponent as winner
        game->playerDisconnected(player);
    }

private:
    // Process a command and return the response


    // Login as a user
    std::string loginUser(const std::string& username, const std::string& password)
    {
        // If already logged in, log out first
        if (!this->username.empty()) {
            UserManager::getInstance().logoutUser(clientSocket);
            this->username = "";
        }

        if (UserManager::getInstance().loginUser(username, password, clientSocket)) {
            this->username = username;
            return "Login successful. Welcome, " + username + "!";
        } else {
            return "Login failed. Invalid username or password.";
        }
    }
    // Help command
    std::string showHelp()
    {
        std::string help = "Available commands:\n";
        help += "who                     # List all online users\n";
        help += "stats [name]            # Display user information\n";
        help += "game                    # list all current games\n";
        help += "observe <game_num>      # Observe a game\n";
        help += "unobserve               # Unobserve a game\n";
        help += "match <name> <b|w> [t]  # Try to start a game\n";
        help += "<A|B|...|O><1|2|...|15> # Make a move in a game\n";
        help += "resign                  # Resign a game\n";
        help += "refresh                 # Refresh a game\n";
        help += "shout <msg>             # shout <msg> to every one online\n";
        help += "tell <name> <msg>       # tell user <name> message\n";
        help += "kibitz <msg>            # Comment on a game when observing\n";
        help += "' <msg>                 # Comment on a game\n";
        help += "quiet                   # Quiet mode, no broadcast messages\n";
        help += "nonquiet                # Non-quiet mode\n";
        help += "block <id>              # No more communication from <id>\n";
        help += "unblock <id>            # Allow communication from <id>\n";
        help += "listmail                # List the header of the mails\n";
        help += "readmail <msg_num>      # Read the particular mail\n";
        help += "deletemail <msg_num>    # Delete the particular mail\n";
        help += "mail <id> <title>       # Send id a mail\n";
        help += "info <msg>              # change your information to <msg>\n";
        help += "passwd <new>            # change password\n";
        help += "exit                    # quit the system\n";
        help += "quit                    # quit the system\n";
        help += "help                    # print this message\n";
        help += "?                       # print this message\n";
        help += "register <name> <pwd>   # register a new user\n";

        return help;
    }

    // Who command - list online users
    std::string listOnlineUsers()
    {
        // For now, just return a placeholder response
        return "Online users: \n- guest";
    }





    void handleClient()
    {
        int timeout_ms = 10000;

        // Send welcome message
        sendMessage("Welcome to Gomoku Server!");
        sendMessage("Type 'help' or '?' for a list of commands.");

        // Main command loop
        while (running)
        {
            std::string rawData = SocketUtils::receiveData(clientSocket, timeout_ms);

            // Strip telnet control sequences and control characters
            std::string result;
            for (char c : rawData)
            {
                if (c >= 32 && c < 127)
                { // Printable ASCII
                    result += c;
                }
                else if (c == '\r' || c == '\n')
                {
                    result += c;
                }
            }

            // Extract the first line
            size_t pos = result.find("\r\n");
            if (pos != std::string::npos)
            {
                result = result.substr(0, pos);
            }

            if (result.empty())
            {
                // Socket might be closed or timed out
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Process the command
            std::string response = processCommand(result);
            sendMessage(response);

            // Handle exit command
            if (result == "exit" || result == "quit") {
                disconnect();
                running = false;
                break;
            }
        }
        // Make sure socket is closed when thread ends
        if (clientSocket >= 0) {
            close(clientSocket);
            clientSocket = -1;
        }
    }

    // List all current games
std::string listCurrentGames() {
    auto games = GameManager::getInstance().getAllGames();
    if (games.empty()) {
        return "No games in progress.";
    }

    std::string result = "Current games:\n";
    for (const auto& game : games) {
        result += std::to_string(game->getId()) + ": " +
                  game->getBlackPlayer()->getUsername() + " (Black) vs " +
                  game->getWhitePlayer()->getUsername() + " (White)";

        if (game->getStatus() == GameStatus::FINISHED) {
            result += " [FINISHED - Winner: " + game->getWinner() + "]";
        } else {
result += std::string(" [") + (game->getCurrentTurn() == StoneColor::BLACK ? "Black" : "White") + " to move]";        }

        result += "\n";
    }

    return result;
}

// Initiate a match with another player
// Initiate a match with another player
std::string initiateMatch(const std::string& opponentName, const std::string& colorStr, int timeLimit) {
    if (username == "guest") {
        return "Guests cannot play games. Please register an account.";
    }

    // Prevent matching with yourself
    if (username == opponentName) {
        return "You cannot play against yourself.";
    }

    if (colorStr != "b" && colorStr != "w") {
        return "Color must be 'b' for black or 'w' for white.";
    }

    auto currentUser = UserManager::getInstance().getUserByUsername(username);
    if (currentUser->isInGame()) {
        return "You are already in a game.";
    }

    auto opponent = UserManager::getInstance().getUserByUsername(opponentName);
    if (!opponent) {
        return "User not found: " + opponentName;
    }

    if (opponent->isInGame()) {
        return opponent->getUsername() + " is already in a game.";
    }

    if (opponent->getSocket() == -1) {
        return opponent->getUsername() + " is not online.";
    }

    // Determine which player is black and which is white
    std::shared_ptr<User> blackPlayer = (colorStr == "b") ? currentUser : opponent;
    std::shared_ptr<User> whitePlayer = (colorStr == "b") ? opponent : currentUser;

    // Create the game
    int gameId = GameManager::getInstance().createGame(blackPlayer, whitePlayer, timeLimit);

    // Get the game board
    auto game = GameManager::getInstance().getGame(gameId);
    std::string gameBoard = game->getBoardString();

    // Prepare notification message
    std::string gameStartMsg = "Game " + std::to_string(gameId) + " started: " +
                               blackPlayer->getUsername() + " (Black) vs " +
                               whitePlayer->getUsername() + " (White)";

    // Send notification and board to opponent
    SocketUtils::sendData(opponent->getSocket(), gameStartMsg + "\r\n\n" + gameBoard + "\r\n");

    // Return notification and board to current user
    return gameStartMsg + "\n\n" + gameBoard;
}
// Resign from the current game
std::string resignGame() {
    auto currentUser = UserManager::getInstance().getUserByUsername(username);
    if (!currentUser->isInGame()) {
        return "You are not in a game.";
    }

    int gameId = currentUser->getGameId();
    auto game = GameManager::getInstance().getGame(gameId);
    if (!game) {
        currentUser->setPlaying(false);
        currentUser->setGameId(-1);
        return "Error: Game not found.";
    }

    game->resign(currentUser);

    // Send notification to opponent
    std::shared_ptr<User> opponent;
    if (game->getBlackPlayer()->getUsername() == username) {
        opponent = game->getWhitePlayer();
    } else {
        opponent = game->getBlackPlayer();
    }

    std::string resignMsg = username + " has resigned the game.";
    SocketUtils::sendData(opponent->getSocket(), resignMsg + "\r\n");

    // Notify observers
    for (int observerSocket : game->getObservers()) {
        SocketUtils::sendData(observerSocket, resignMsg + "\r\n");
    }

    return "You have resigned the game.";
}

// Refresh the current game board
std::string refreshGame() {
    auto currentUser = UserManager::getInstance().getUserByUsername(username);
    if (!currentUser->isInGame() && !currentUser->isUserObserving()) {
        return "You are not in or observing a game.";
    }

    int gameId = currentUser->getGameId();
    auto game = GameManager::getInstance().getGame(gameId);
    if (!game) {
        currentUser->setPlaying(false);
        currentUser->setObserving(false);
        currentUser->setGameId(-1);
        return "Error: Game not found.";
    }

    return game->getBoardString();
}

// Observe a game
std::string observeGame(int gameId) {
    auto currentUser = UserManager::getInstance().getUserByUsername(username);
    if (currentUser->isInGame()) {
        return "You cannot observe while playing a game.";
    }

    auto game = GameManager::getInstance().getGame(gameId);
    if (!game) {
        return "Game not found: " + std::to_string(gameId);
    }

    // If already observing a different game, unobserve first
    if (currentUser->isUserObserving()) {
        auto oldGame = GameManager::getInstance().getGame(currentUser->getGameId());
        if (oldGame) {
            oldGame->removeObserver(clientSocket);
        }
    }

    // Add as observer
    game->addObserver(clientSocket);
    currentUser->setObserving(true);
    currentUser->setGameId(gameId);

    return "You are now observing game " + std::to_string(gameId) + ".\n\n" + game->getBoardString();
}

// Stop observing a game
std::string unobserveGame() {
    auto currentUser = UserManager::getInstance().getUserByUsername(username);
    if (!currentUser->isUserObserving()) {
        return "You are not observing any game.";
    }

    int gameId = currentUser->getGameId();
    auto game = GameManager::getInstance().getGame(gameId);
    if (game) {
        game->removeObserver(clientSocket);
    }

    currentUser->setObserving(false);
    currentUser->setGameId(-1);

    return "You are no longer observing the game.";
}

    std::string makeMove(int row, int col) {
    auto currentUser = UserManager::getInstance().getUserByUsername(username);
    if (!currentUser->isInGame()) {
        return "You are not in a game.";
    }

    int gameId = currentUser->getGameId();
    auto game = GameManager::getInstance().getGame(gameId);
    if (!game) {
        currentUser->setPlaying(false);
        currentUser->setGameId(-1);
        return "Error: Game not found.";
    }

    // Check if game is already finished
    if (game->getStatus() == GameStatus::FINISHED) {
        return "This game is already over. The winner was " + game->getWinner() + ".";
    }

    // Check if it's this player's turn before trying to make a move
    bool isBlack = (currentUser->getUsername() == game->getBlackPlayer()->getUsername());
    bool isWhite = (currentUser->getUsername() == game->getWhitePlayer()->getUsername());
    bool isBlackTurn = (game->getCurrentTurn() == StoneColor::BLACK);

    if ((isBlack && !isBlackTurn) || (isWhite && isBlackTurn)) {
        return "It's not your turn to move. Please wait for your opponent.";
    }

    // Check if the position is already occupied
    if (!game->isPositionEmpty(row, col)) {
        return "Invalid move: that position is already occupied.";
    }

    // Now try to make the move
    if (!game->makeMove(currentUser, row, col)) {
        return "Invalid move: an unexpected error occurred.";
    }

    // Get the opponent
    std::shared_ptr<User> opponent;
    if (game->getBlackPlayer()->getUsername() == username) {
        opponent = game->getWhitePlayer();
    } else {
        opponent = game->getBlackPlayer();
    }

    // Create move notification message
    char colChar = 'A' + col;
    std::string moveMsg = username + " played at " + colChar + std::to_string(row + 1);
    std::string boardStr = game->getBoardString();

    // Check if the game ended
    if (game->getStatus() == GameStatus::FINISHED) {
        std::string winMsg = game->getWinner() + " has won the game!";
        moveMsg += "\n" + winMsg;

        // Send notification with win message to opponent
        SocketUtils::sendData(opponent->getSocket(), moveMsg + "\r\n\n" + boardStr + "\r\n");

        // Notify observers
        for (int observerSocket : game->getObservers()) {
            SocketUtils::sendData(observerSocket, moveMsg + "\r\n\n" + boardStr + "\r\n");
        }

        return boardStr + "\n" + winMsg;
    }

    // Game continues - notify opponent about the move
    SocketUtils::sendData(opponent->getSocket(), moveMsg + "\r\n\n" + boardStr + "\r\n");

    // Notify observers
    for (int observerSocket : game->getObservers()) {
        SocketUtils::sendData(observerSocket, moveMsg + "\r\n\n" + boardStr + "\r\n");
    }

    return boardStr;
}

    // Add these methods to your TelnetClientHandler class

// Broadcast a message to all online users
std::string shoutMessage(const std::string& message) {
    if (username == "guest") {
        return "Guests cannot shout messages. Please register an account.";
    }

    // Format: [Shout] <username>: <message>
    std::string formattedMsg = "[Shout] " + username + ": " + message;

    // Send to all online users except those in quiet mode or who blocked this user
    auto onlineUsers = UserManager::getInstance().getOnlineUsers();
    for (const auto& user : onlineUsers) {
        if (user->getUsername() != username &&
            user->getSocket() != -1 &&
            !user->isInQuietMode() &&
            !user->isBlocked(username)) {
            SocketUtils::sendData(user->getSocket(), formattedMsg + "\r\n");
        }
    }

    return "Message sent.";
}

// Send a private message to a specific user
std::string tellMessage(const std::string& recipient, const std::string& message) {
    if (username == "guest") {
        return "Guests cannot send private messages. Please register an account.";
    }

    auto recipientUser = UserManager::getInstance().getUserByUsername(recipient);
    if (!recipientUser) {
        return "User not found: " + recipient;
    }

    if (recipientUser->isBlocked(username)) {
        return recipient + " has blocked messages from you.";
    }

    // Format: [Tell] <username>: <message>
    std::string formattedMsg = "[Tell] " + username + ": " + message;

    // Send to recipient if online
    if (recipientUser->getSocket() != -1) {
        SocketUtils::sendData(recipientUser->getSocket(), formattedMsg + "\r\n");
        return "Message sent to " + recipient + ".";
    } else {
        return recipient + " is offline.";
    }
}

// Comment on a game being observed
std::string kibitzMessage(const std::string& message) {
    auto currentUser = UserManager::getInstance().getUserByUsername(username);
    if (!currentUser->isUserObserving()) {
        return "You are not observing a game.";
    }

    int gameId = currentUser->getGameId();
    auto game = GameManager::getInstance().getGame(gameId);
    if (!game) {
        currentUser->setObserving(false);
        currentUser->setGameId(-1);
        return "Error: Game not found.";
    }

    // Format: [Kibitz] <username>: <message>
    std::string formattedMsg = "[Kibitz] " + username + ": " + message;

    // Send to all observers of this game
    for (int observerSocket : game->getObservers()) {
        if (observerSocket != clientSocket) {
            auto observerUser = UserManager::getInstance().getUserBySocket(observerSocket);
            if (observerUser && !observerUser->isInQuietMode() && !observerUser->isBlocked(username)) {
                SocketUtils::sendData(observerSocket, formattedMsg + "\r\n");
            }
        }
    }

    // Also send to the players if they're not in quiet mode and haven't blocked the user
    auto blackPlayer = game->getBlackPlayer();
    if (!blackPlayer->isInQuietMode() && !blackPlayer->isBlocked(username)) {
        SocketUtils::sendData(blackPlayer->getSocket(), formattedMsg + "\r\n");
    }

    auto whitePlayer = game->getWhitePlayer();
    if (!whitePlayer->isInQuietMode() && !whitePlayer->isBlocked(username)) {
        SocketUtils::sendData(whitePlayer->getSocket(), formattedMsg + "\r\n");
    }

    return "Comment sent.";
}
    // Set quiet mode (no broadcast messages)
    std::string setQuietMode(bool quiet) {
        auto currentUser = UserManager::getInstance().getUserByUsername(username);
        if (!currentUser) {
            return "Error: User not found.";
        }

        currentUser->setQuietMode(quiet);

        return quiet ? "Quiet mode enabled. You will not receive broadcast messages."
                     : "Quiet mode disabled. You will receive broadcast messages.";
    }

    // Block a user
    std::string blockUser(const std::string& targetUsername) {
        if (username == "guest") {
            return "Guests cannot block users. Please register an account.";
        }

        auto currentUser = UserManager::getInstance().getUserByUsername(username);
        if (!currentUser) {
            return "Error: User not found.";
        }

        auto targetUser = UserManager::getInstance().getUserByUsername(targetUsername);
        if (!targetUser) {
            return "User not found: " + targetUsername;
        }

        // Check if already blocked
        if (currentUser->isBlocked(targetUsername)) {
            return targetUsername + " is already blocked.";
        }

        // Block the user
        currentUser->blockUser(targetUsername);

        return "Blocked all communication from " + targetUsername + ".";
    }

    // Unblock a user
    std::string unblockUser(const std::string& targetUsername) {
        if (username == "guest") {
            return "Guests cannot unblock users. Please register an account.";
        }

        auto currentUser = UserManager::getInstance().getUserByUsername(username);
        if (!currentUser) {
            return "Error: User not found.";
        }

        // Check if actually blocked
        if (!currentUser->isBlocked(targetUsername)) {
            return targetUsername + " is not blocked.";
        }

        // Unblock the user
        currentUser->unblockUser(targetUsername);

        return "Unblocked communication from " + targetUsername + ".";
    }
    // List mail headers
std::string listMail() {
    if (username == "guest") {
        return "Guests cannot use mail. Please register an account.";
    }

    auto messages = MessageManager::getInstance().getMessages(username);

    if (messages.empty()) {
        return "Your mailbox is empty.";
    }

    std::string result = "Mail messages:\n";
    for (const auto& message : messages) {
        result += message->getFormattedHeader() + "\n";
    }

    return result;
}

// Read a specific mail
std::string readMail(int messageId) {
    if (username == "guest") {
        return "Guests cannot use mail. Please register an account.";
    }

    auto message = MessageManager::getInstance().getMessage(username, messageId);

    if (!message) {
        return "Message not found.";
    }

    message->markAsRead();

    std::string result = "From: " + message->getSender() + "\n";
    result += "Title: " + message->getTitle() + "\n";
    result += "---\n";
    result += message->getContent() + "\n";
    result += "---\n";

    return result;
}

// Delete a mail
std::string deleteMail(int messageId) {
    if (username == "guest") {
        return "Guests cannot use mail. Please register an account.";
    }

    if (MessageManager::getInstance().deleteMessage(username, messageId)) {
        return "Message deleted.";
    } else {
        return "Message not found.";
    }
}

// Send a mail
std::string sendMail(const std::string& recipient, const std::string& title) {
    if (username == "guest") {
        return "Guests cannot use mail. Please register an account.";
    }

    auto recipientUser = UserManager::getInstance().getUserByUsername(recipient);
    if (!recipientUser) {
        return "User not found: " + recipient;
    }

    sendMessage("Enter your message. End with a line containing only a period (.)");

    std::string content;
    std::string line;
    while (true) {
        line = SocketUtils::receiveData(clientSocket, 60000); // 1 minute timeout

        // Clean up line endings
        if (!line.empty() && line.back() == '\n') line.pop_back();
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line == ".") {
            break;
        }

        content += line + "\n";
    }

    MessageManager::getInstance().sendMessage(username, recipient, title, content);

    // Notify recipient if online
    if (recipientUser->getSocket() != -1) {
        std::string notifyMsg = "You have received a new mail from " + username;
        SocketUtils::sendData(recipientUser->getSocket(), notifyMsg + "\r\n");
    }

    return "Mail sent to " + recipient;
}
    // Update user info
    std::string setUserInfo(const std::string& info) {
        if (username == "guest") {
            return "Guests cannot set personal information. Please register an account.";
        }

        auto currentUser = UserManager::getInstance().getUserByUsername(username);
        if (!currentUser) {
            return "Error: User not found.";
        }

        currentUser->setInfo(info);
        return "Your information has been updated.";
    }
    std::string processCommand(const std::string& command)
    {
        // Split the command into tokens
        std::istringstream iss(command);
        std::vector<std::string> tokens;
        std::string token;
        std::regex moveAttemptPattern("^([A-Za-z])([0-9]+)$");
        std::smatch moveAttemptMatches;

        while (iss >> token) {
            tokens.push_back(token);
        }

        if (tokens.empty()) {
            return "Empty command";
        }

        // Get the base command (first token)
        std::string cmd = tokens[0];
        // Convert to lowercase for case-insensitive comparison
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        std::string cmdStr = cmd;  // Assuming cmd is already a std::string
        if (std::regex_match(cmdStr, moveAttemptMatches, moveAttemptPattern)) {
            auto currentUser = UserManager::getInstance().getUserByUsername(username);
            if (!currentUser->isInGame()) {
                return "You are not in a game. Join a game first to make moves.";
            }
            char colChar = toupper(moveAttemptMatches[1].str()[0]);
            int row;

            try {
                row = std::stoi(moveAttemptMatches[2].str());
            } catch (...) {
                return "Invalid move format. Moves should be in the format 'A1' to 'O15'.";
            }

            // Check if the move is within board bounds
            if (colChar < 'A' || colChar > 'O' || row < 1 || row > 15) {
                return "Invalid move: out of bounds. The board is 15x15 (A1 to O15).";
            }

            // If we get here, it's a valid format move, process it
            row = row - 1; // Convert to 0-based
            int col = colChar - 'A';

            return makeMove(row, col);
        }

        // Process login-related commands regardless of login status
        if (cmd == "login") {
            if (tokens.size() < 3) {
                return "Usage: login <username> <password>";
            }
            return loginUser(tokens[1], tokens[2]);
        }
        else if (cmd == "testsave") {
            std::cout << "Testing save functionality" << std::endl;
            std::ofstream testFile("/tmp/test_save.txt");
            if (testFile.is_open()) {
                testFile << "Test save at " << time(nullptr) << std::endl;
                testFile.close();
                std::cout << "Test file written successfully" << std::endl;
                return "Test save successful. Check for test_save.txt";
            } else {
                std::cout << "Failed to open test file" << std::endl;
                return "Test save failed. Check server permissions.";
            }
            testFile.close();
        }
        else if (cmd == "quiet") {
            return setQuietMode(true);
        }
        else if (cmd == "info") {
            // Extract everything after "info "
            size_t cmdPos = command.find("info");
            if (cmdPos == std::string::npos || cmdPos + 5 >= command.length()) {
                return "Usage: info <message>";
            }

            std::string infoText = command.substr(cmdPos + 5);
            return setUserInfo(infoText);
        }
        else if (cmd == "nonquiet") {
            return setQuietMode(false);
        }
        else if (cmd == "listmail") {
            return listMail();
        }
        else if (cmd == "readmail") {
            if (tokens.size() < 2) {
                return "Usage: readmail <msg_num>";
            }

            int messageId;
            try {
                messageId = std::stoi(tokens[1]);
            } catch (...) {
                return "Invalid message number.";
            }

            return readMail(messageId);
        }
        else if (cmd == "deletemail") {
            if (tokens.size() < 2) {
                return "Usage: deletemail <msg_num>";
            }

            int messageId;
            try {
                messageId = std::stoi(tokens[1]);
            } catch (...) {
                return "Invalid message number.";
            }

            return deleteMail(messageId);
        }
        else if (cmd == "mail") {
            if (tokens.size() < 3) {
                return "Usage: mail <id> <title>";
            }

            std::string recipient = tokens[1];

            // Extract title (everything after the recipient)
            std::string title = command.substr(command.find(recipient) + recipient.length() + 1);

            return sendMail(recipient, title);
        }
        else if (cmd == "guest") {
            return loginGuest();
        }
        else if (cmd == "block") {
            if (tokens.size() < 2) {
                return "Usage: block <id>";
            }
            return blockUser(tokens[1]);
        }
        else if (cmd == "unblock") {
            if (tokens.size() < 2) {
                return "Usage: unblock <id>";
            }
            return unblockUser(tokens[1]);
        }
        else if (cmd == "register") {
            if (tokens.size() < 3) {
                return "Usage: register <username> <password>";
            }
            return registerUser(tokens[1], tokens[2]);
        }
        else if (cmd == "exit" || cmd == "quit") {
            return "Goodbye!";
        }
        else if (cmd == "help" || cmd == "?") {
            return showHelp();
        }
        // Game-related commands
        else if (cmd == "game") {
            return listCurrentGames();
        }
        else if (cmd == "match") {
            if (tokens.size() < 3) {
                return "Usage: match <name> <b|w> [t]";
            }
            std::string opponentName = tokens[1];
            std::string colorStr = tokens[2];
            int timeLimit = 600; // Default 10 minutes

            if (tokens.size() > 3) {
                try {
                    timeLimit = std::stoi(tokens[3]);
                } catch (...) {
                    return "Invalid time limit. Using default (600 seconds).";
                }
            }

            return initiateMatch(opponentName, colorStr, timeLimit);
        }
        else if (cmd == "resign") {
            return resignGame();
        }
        else if (cmd == "refresh") {
            return refreshGame();
        }
        else if (cmd == "observe") {
            if (tokens.size() < 2) {
                return "Usage: observe <game_num>";
            }
            int gameId;
            try {
                gameId = std::stoi(tokens[1]);
            } catch (...) {
                return "Invalid game number.";
            }
            return observeGame(gameId);
        }
        else if (cmd == "unobserve") {
            return unobserveGame();
        }

        // For all other commands, check if user is logged in
        if (username.empty()) {
            return "Please login first using 'login <username> <password>' or 'guest'.";
        }

        // Process commands for logged-in users
        if (cmd == "who") {
            return UserManager::getInstance().getOnlineUsersList();
        }
        // In processCommand method, add:
        else if (cmd == "shout") {
            std::string message = command.substr(command.find(' ') + 1);

            if (message.empty()) {
                return "Usage: shout <message>";
            }

            return shoutMessage(message);
        }
        else if (cmd == "tell") {
            // Check if there's anything after "tell"
            size_t cmdPos = command.find("tell");
            if (cmdPos == std::string::npos || cmdPos + 5 >= command.length()) {
                return "Usage: tell <name> <message>";
            }

            // Extract everything after "tell "
            std::string rest = command.substr(cmdPos + 5);

            // Find the first space after the recipient name
            size_t spacePos = rest.find_first_of(" \t");
            if (spacePos == std::string::npos) {
                return "Usage: tell <name> <message>";
            }

            // Extract recipient and message
            std::string recipient = rest.substr(0, spacePos);
            std::string message = rest.substr(spacePos + 1);

            if (recipient.empty() || message.empty()) {
                return "Usage: tell <name> <message>";
            }

            return tellMessage(recipient, message);
        }
        else if (cmd == "kibitz" || cmd == "'") {
            std::string message = command.substr(command.find(' ') + 1);

            if (message.empty()) {
                return "Usage: kibitz <message> or ' <message>";
            }

            return kibitzMessage(message);
        }
        else if (cmd == "stats") {
            if (tokens.size() > 1) {
                return showUserStats(tokens[1]);
            } else {
                return showUserStats(username);
            }
        }
        else if (cmd == "info") {
            // Combine remaining tokens into the info string
            std::string info;
            for (size_t i = 1; i < tokens.size(); i++) {
                info += tokens[i] + " ";
            }
            return updateUserInfo(info);
        }
        else if (cmd == "passwd") {
            if (tokens.size() < 2) {
                return "Usage: passwd <new>";
            }
            return changePassword(tokens[1]);
        }
        // Other command handlers will be added here...

        // Unknown command
        return "Unknown command: " + cmd + ". Type 'help' or '?' for a list of commands.";
    }


    // Login as guest
    std::string loginGuest()
    {
        // If already logged in, log out first
        if (!this->username.empty()) {
            UserManager::getInstance().logoutUser(clientSocket);
            this->username = "";
        }

        UserManager::getInstance().loginGuest(clientSocket);
        this->username = "guest";
        return "Logged in as guest. You can register a new account using 'register <username> <password>'.";
    }

    // Register a new user
    std::string registerUser(const std::string& username, const std::string& password)
    {
        // Only allow registration if logged in as guest
        if (this->username != "guest") {
            return "You must be logged in as guest to register.";
        }

        if (UserManager::getInstance().registerUser(username, password, clientSocket)) {
            this->username = username;
            return "Registration successful. You are now logged in as " + username + ".";
        } else {
            return "Registration failed. Username already exists or is invalid.";
        }
    }

    // Show user statistics
    std::string showUserStats(const std::string& targetUser)
    {
        std::string userToShow = targetUser.empty() ? username : targetUser;

        auto user = UserManager::getInstance().getUserByUsername(userToShow);
        if (!user) {
            return "User not found: " + userToShow;
        }

        std::string result = "Statistics for " + userToShow + ":\n";
        result += "Wins: " + std::to_string(user->getWins()) + "\n";
        result += "Losses: " + std::to_string(user->getLosses()) + "\n";
        result += "Rating: " + std::to_string(static_cast<int>(user->getRating())) + "\n";

        if (!user->getInfo().empty()) {
            result += "Info: " + user->getInfo() + "\n";
        }

        return result;
    }

    // Update user info
    std::string updateUserInfo(const std::string& info)
    {
        if (username == "guest") {
            return "Guests cannot set info. Please register an account.";
        }

        if (UserManager::getInstance().updateUserInfo(username, info)) {
            return "Information updated.";
        } else {
            return "Failed to update information.";
        }
    }

    // Change password
    std::string changePassword(const std::string& newPassword) {
        if (username == "guest") {
            return "Guests cannot change password. Please register an account.";
        }

        auto currentUser = UserManager::getInstance().getUserByUsername(username);
        if (!currentUser) {
            return "Error: User not found.";
        }

        currentUser->setPassword(newPassword);
        UserManager::getInstance().saveUsers(); // Explicitly save after password change
        return "Your password has been changed.";
    }


};

#endif //TELNETCLIENTHANDLER_H