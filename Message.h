#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <ctime>
#include <unordered_map>

class Message {
private:
    int id;
    std::string sender;
    std::string recipient;
    std::string title;
    std::string content;
    time_t timestamp;
    bool read;

public:
    Message(int id, const std::string& sender, const std::string& recipient,
            const std::string& title, const std::string& content)
        : id(id), sender(sender), recipient(recipient),
          title(title), content(content), read(false) {
        timestamp = std::time(nullptr);
    }

    int getId() const { return id; }
    std::string getSender() const { return sender; }
    std::string getRecipient() const { return recipient; }
    std::string getTitle() const { return title; }
    std::string getContent() const { return content; }
    time_t getTimestamp() const { return timestamp; }
    bool isRead() const { return read; }

    void markAsRead() { read = true; }

    std::string getFormattedHeader() const {
        char timeStr[100];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", std::localtime(&timestamp));

        return std::to_string(id) + ". " +
               (read ? "" : "[NEW] ") +
               "From: " + sender +
               ", Title: " + title +
               ", Date: " + timeStr;
    }
};

class MessageManager {
private:
    std::unordered_map<std::string, std::vector<std::shared_ptr<Message>>> userMessages;
    int nextMessageId;
    std::mutex messagesMutex;

    MessageManager() : nextMessageId(1) {}

public:
    static MessageManager& getInstance() {
        static MessageManager instance;
        return instance;
    }

    void sendMessage(const std::string& sender, const std::string& recipient,
                    const std::string& title, const std::string& content) {
        std::lock_guard<std::mutex> lock(messagesMutex);

        auto message = std::make_shared<Message>(
            nextMessageId++, sender, recipient, title, content);

        userMessages[recipient].push_back(message);
    }

    std::vector<std::shared_ptr<Message>> getMessages(const std::string& username) {
        std::lock_guard<std::mutex> lock(messagesMutex);

        return userMessages[username];
    }

    std::shared_ptr<Message> getMessage(const std::string& username, int messageId) {
        std::lock_guard<std::mutex> lock(messagesMutex);

        for (auto& message : userMessages[username]) {
            if (message->getId() == messageId) {
                return message;
            }
        }

        return nullptr;
    }

    bool deleteMessage(const std::string& username, int messageId) {
        std::lock_guard<std::mutex> lock(messagesMutex);

        auto& messages = userMessages[username];
        for (auto it = messages.begin(); it != messages.end(); ++it) {
            if ((*it)->getId() == messageId) {
                messages.erase(it);
                return true;
            }
        }

        return false;
    }

    int countUnreadMessages(const std::string& username) {
        std::lock_guard<std::mutex> lock(messagesMutex);

        int count = 0;
        for (auto& message : userMessages[username]) {
            if (!message->isRead()) {
                count++;
            }
        }

        return count;
    }
};
#endif // MESSAGE_H