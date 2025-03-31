#ifndef GAME_H
#define GAME_H

#include <vector>
#include <string>
#include "User.h"

enum class StoneColor { BLACK, WHITE };
enum class GameStatus { WAITING, PLAYING, FINISHED };

class Game {
private:
    int gameId;
    std::shared_ptr<User> blackPlayer;
    std::shared_ptr<User> whitePlayer;
    std::vector<std::vector<char>> board;
    StoneColor currentTurn;
    GameStatus status;
    std::string winner;
    std::vector<int> observers;
    time_t gameStartTime;
    time_t lastMoveTime;
    int timeLimit;
    int blackTimeUsed;
    int whiteTimeUsed;

public:
    Game(int id, std::shared_ptr<User> black, std::shared_ptr<User> white, int timeLimit = 600)
        : gameId(id), blackPlayer(black), whitePlayer(white),
          currentTurn(StoneColor::BLACK), status(GameStatus::PLAYING),
          timeLimit(timeLimit), blackTimeUsed(0), whiteTimeUsed(0)
    {
        // Initialize empty board (15x15)
        board.resize(15, std::vector<char>(15, '.'));

        // Set players' game status
        blackPlayer->setPlaying(true);
        blackPlayer->setGameId(gameId);
        whitePlayer->setPlaying(true);
        whitePlayer->setGameId(gameId);

        // Record game start time
        gameStartTime = time(nullptr);
        lastMoveTime = gameStartTime;

    }

    // Game methods
    void playerDisconnected(std::shared_ptr<User> player);

    bool checkTimeExpired();
    bool makeMove(std::shared_ptr<User> player, int row, int col);
    bool checkWin(int row, int col);
    void resign(std::shared_ptr<User> player);
    void endGame(const std::string& winnerName);

    // Observer methods
    void addObserver(int socket);
    void removeObserver(int socket);
    bool isObserving(int socket) const;
    std::vector<int> getObservers() const;
    bool isPositionEmpty(int row, int col) const;

    // Getters
    int getId() const { return gameId; }
    std::string getBoardString() const;
    GameStatus getStatus() const { return status; }
    StoneColor getCurrentTurn() const { return currentTurn; }
    std::string getWinner() const { return winner; }
    std::shared_ptr<User> getBlackPlayer() const { return blackPlayer; }
    std::shared_ptr<User> getWhitePlayer() const { return whitePlayer; }
};

// GameManager to manage all games
class GameManager {
private:
    std::unordered_map<int, std::shared_ptr<Game>> games;
    int nextGameId;
    std::mutex gamesMutex;

    // Private constructor
    GameManager() : nextGameId(1) {}

public:
    // Get the instance
    static GameManager& getInstance() {
        static GameManager instance;
        return instance;
    }

    // Create a new game
    int createGame(std::shared_ptr<User> blackPlayer, std::shared_ptr<User> whitePlayer, int timeLimit = 600);

    std::shared_ptr<Game> getGame(int gameId);
    std::vector<std::shared_ptr<Game>> getAllGames();
    void cleanupGames();
};

void Game::playerDisconnected(std::shared_ptr<User> player) {
    if (status != GameStatus::PLAYING) {
        return;
    }

    // If the black player disconnected, white wins and vice versa
    if (player->getUsername() == blackPlayer->getUsername()) {
        endGame(whitePlayer->getUsername());
    } else if (player->getUsername() == whitePlayer->getUsername()) {
        endGame(blackPlayer->getUsername());
    }
}

// Check if time has expired periodically
bool Game::checkTimeExpired() {
    if (status != GameStatus::PLAYING) {
        return false;
    }

    // Calculate time since last move
    time_t now = time(nullptr);
    int elapsed = static_cast<int>(now - lastMoveTime);

    // Check current player's time
    if (currentTurn == StoneColor::BLACK) {
        int updatedBlackTime = blackTimeUsed + elapsed;
        if (updatedBlackTime > timeLimit) {
            std::cout << "Black player time expired: " << updatedBlackTime << " seconds" << std::endl;
            endGame(whitePlayer->getUsername());
            return true;
        }
    } else {
        int updatedWhiteTime = whiteTimeUsed + elapsed;
        if (updatedWhiteTime > timeLimit) {
            std::cout << "White player time expired: " << updatedWhiteTime << " seconds" << std::endl;
            endGame(blackPlayer->getUsername());
            return true;
        }
    }

    return false;
}

bool Game::makeMove(std::shared_ptr<User> player, int row, int col) {
    // Check if game is already over
    if (status != GameStatus::PLAYING) {
        return false;
    }

    // Check if it's the player's turn
    bool isBlack = (player->getUsername() == blackPlayer->getUsername());
    bool isWhite = (player->getUsername() == whitePlayer->getUsername());

    if (!isBlack && !isWhite) {
        return false;
    }

    // Check if it's their turn
    if ((isBlack && currentTurn != StoneColor::BLACK) ||
        (isWhite && currentTurn != StoneColor::WHITE)) {
        return false;
    }

    // Check if position is valid and empty
    if (row < 0 || row >= 15 || col < 0 || col >= 15 || board[row][col] != '.') {
        return false;
    }

    // Update time used
    time_t now = time(nullptr);
    int elapsed = static_cast<int>(now - lastMoveTime);

    // Check for time limit
    if (currentTurn == StoneColor::BLACK) {
        blackTimeUsed += elapsed;
        if (blackTimeUsed > timeLimit) {
            endGame(whitePlayer->getUsername());
            return false;
        }
    } else {
        whiteTimeUsed += elapsed;
        if (whiteTimeUsed > timeLimit) {
            endGame(blackPlayer->getUsername());
            return false;
        }
    }

    // Place stone
    board[row][col] = (currentTurn == StoneColor::BLACK) ? 'X' : 'O';

    // Check for win
    if (checkWin(row, col)) {
        if (currentTurn == StoneColor::BLACK) {
            endGame(blackPlayer->getUsername());
        } else {
            endGame(whitePlayer->getUsername());
        }
        return true; // Move was successful, even though it ended the game
    }

    // Only update turn if game isn't over
    if (status == GameStatus::PLAYING) {
        currentTurn = (currentTurn == StoneColor::BLACK) ? StoneColor::WHITE : StoneColor::BLACK;
        lastMoveTime = now;
    }

    return true;
}

// Function to check if a position is empty
bool Game::isPositionEmpty(int row, int col) const {
    if (row < 0 || row >= 15 || col < 0 || col >= 15) {
        return false;
    }
    return board[row][col] == '.';
}

bool Game::checkWin(int row, int col) {
    char stone = board[row][col];
    const int directions[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}}; // horizontal, vertical, diagonal \, diagonal /

    for (int d = 0; d < 4; d++) {
        int count = 1;

        // Check in positive direction
        for (int i = 1; i < 5; i++) {
            int newRow = row + i * directions[d][0];
            int newCol = col + i * directions[d][1];

            if (newRow < 0 || newRow >= 15 || newCol < 0 || newCol >= 15 || board[newRow][newCol] != stone) {
                break;
            }
            count++;
        }

        // Check in negative direction
        for (int i = 1; i < 5; i++) {
            int newRow = row - i * directions[d][0];
            int newCol = col - i * directions[d][1];

            if (newRow < 0 || newRow >= 15 || newCol < 0 || newCol >= 15 || board[newRow][newCol] != stone) {
                break;
            }
            count++;
        }

        if (count >= 5) {
            return true;
        }
    }

    return false;
}

void Game::resign(std::shared_ptr<User> player) {
    if (status != GameStatus::PLAYING) {
        return;
    }

    if (player->getUsername() == blackPlayer->getUsername()) {
        endGame(whitePlayer->getUsername());
    } else if (player->getUsername() == whitePlayer->getUsername()) {
        endGame(blackPlayer->getUsername());
    }
}

void Game::endGame(const std::string& winnerName) {
    status = GameStatus::FINISHED;
    winner = winnerName;

    // Update player stats
    if (winner == blackPlayer->getUsername()) {
        blackPlayer->addWin();
        whitePlayer->addLoss();
    } else {
        whitePlayer->addWin();
        blackPlayer->addLoss();
    }

    // Reset player statuses
    blackPlayer->setPlaying(false);
    blackPlayer->setGameId(-1);
    whitePlayer->setPlaying(false);
    whitePlayer->setGameId(-1);
}

// Observer methods
void Game::addObserver(int socket) {
    // Check if already observing
    for (int observer : observers) {
        if (observer == socket) {
            return;
        }
    }
    observers.push_back(socket);
}

void Game::removeObserver(int socket) {
    auto it = std::find(observers.begin(), observers.end(), socket);
    if (it != observers.end()) {
        observers.erase(it);
    }
}

bool Game::isObserving(int socket) const {
    return std::find(observers.begin(), observers.end(), socket) != observers.end();
}

std::vector<int> Game::getObservers() const {
    return observers;
}

std::string Game::getBoardString() const {
    std::string result = "   A B C D E F G H I J K L M N O\n";
    for (int i = 0; i < 15; i++) {
        result += (i < 9 ? " " : "") + std::to_string(i + 1) + " ";
        for (int j = 0; j < 15; j++) {
            result += board[i][j];
            result += " ";
        }
        result += "\n";
    }

    result += "\nCurrent turn: " + std::string(currentTurn == StoneColor::BLACK ? "Black" : "White");

    result += "\nBlack time used: " + std::to_string(blackTimeUsed) + " seconds";
    result += "\nWhite time used: " + std::to_string(whiteTimeUsed) + " seconds";

    return result;
}

int GameManager::createGame(std::shared_ptr<User> blackPlayer, std::shared_ptr<User> whitePlayer, int timeLimit) {
    std::lock_guard<std::mutex> lock(gamesMutex);

    int gameId = nextGameId++;
    games[gameId] = std::make_shared<Game>(gameId, blackPlayer, whitePlayer, timeLimit);

    return gameId;
}

std::shared_ptr<Game> GameManager::getGame(int gameId) {
    std::lock_guard<std::mutex> lock(gamesMutex);

    auto it = games.find(gameId);
    if (it != games.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<Game>> GameManager::getAllGames() {
    std::lock_guard<std::mutex> lock(gamesMutex);

    std::vector<std::shared_ptr<Game>> result;
    for (const auto& pair : games) {
        result.push_back(pair.second);
    }
    return result;
}

void GameManager::cleanupGames() {
    std::lock_guard<std::mutex> lock(gamesMutex);

    auto it = games.begin();
    while (it != games.end()) {
        if (it->second->getStatus() == GameStatus::FINISHED) {
            it = games.erase(it);
        } else {
            ++it;
        }
    }
}
#endif // GAME_H