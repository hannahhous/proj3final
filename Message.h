#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <vector>
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
    Message(int id, const std::string& sender, const std::string& recipient,
        const std::string& title, const std::string& content, time_t timestamp, bool isRead = false)
    : id(id), sender(sender), recipient(recipient),
      title(title), content(content), timestamp(timestamp), read(isRead) {
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

    MessageManager() : nextMessageId(1) {loadMessages();}
    ~MessageManager() {saveMessages();}

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

        // Save messages
        saveMessages();
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
                saveMessages(); // Save after deletion
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
    void markMessageAsRead(const std::string& username, int messageId) {
        std::lock_guard<std::mutex> lock(messagesMutex);

        for (auto& message : userMessages[username]) {
            if (message->getId() == messageId && !message->isRead()) {
                message->markAsRead();
                saveMessages(); // Save after marking as read
                break;
            }
        }
    }
    // Save all messages to a file
    bool saveMessages() {


        if (!messagesMutex.try_lock()) {
            return false;
        }
        try {
            // Open file for writing
            std::ofstream file("messages_data.txt");
            if (!file.is_open()) {
                std::cerr << "Failed to open messages_data.txt for writing" << std::endl;
                return false;
            }

            int messageCount = 0;

            // Write each message
            for (const auto& pair : userMessages) {
                const std::string& recipient = pair.first;
                const auto& messages = pair.second;

                for (const auto& message : messages) {
                    file << "MESSAGE_BEGIN\n";
                    file << "id=" << message->getId() << "\n";
                    file << "sender=" << message->getSender() << "\n";
                    file << "recipient=" << message->getRecipient() << "\n";
                    file << "title=" << message->getTitle() << "\n";
                    file << "timestamp=" << message->getTimestamp() << "\n";
                    file << "read=" << (message->isRead() ? "1" : "0") << "\n";
                    file << "content_begin\n";
                    file << message->getContent();
                    if (!message->getContent().empty() && message->getContent().back() != '\n') {
                        file << "\n";
                    }
                    file << "content_end\n";
                    file << "MESSAGE_END\n";

                    messageCount++;
                }
            }

            file.flush();
            file.close();
            std::cout << "Message data saved successfully: " << messageCount << " messages written" << std::endl;
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error saving messages: " << e.what() << std::endl;
            return false;
        }
    }

    // Load all messages from file
    void loadMessages() {
        std::lock_guard<std::mutex> lock(messagesMutex);

        try {
            // Try to open the file
            std::ifstream file("messages_data.txt");
            if (!file.is_open()) {
                std::cout << "No message data file found." << std::endl;
                return;
            }

            std::string line;
            int id = 0;
            std::string sender, recipient, title, content;
            time_t timestamp = 0;
            bool read = false;
            bool inContentSection = false;
            bool inMessageSection = false;

            int highestId = 0;

            while (std::getline(file, line)) {
                if (line == "MESSAGE_BEGIN") {
                    inMessageSection = true;
                    id = 0;
                    sender = recipient = title = content = "";
                    timestamp = 0;
                    read = false;
                    inContentSection = false;
                    continue;
                }
                else if (line == "MESSAGE_END") {
                    if (inMessageSection && id > 0 && !sender.empty() && !recipient.empty()) {
                        // Create message
                        auto message = std::make_shared<Message>(id, sender, recipient, title, content);

                        // Set read status and timestamp
                        if (read) {
                            message->markAsRead();
                        }

                        // Add to the recipient's messages
                        userMessages[recipient].push_back(message);

                        highestId = std::max(highestId, id);

                        std::cout << "Loaded message " << id << " from " << sender << " to " << recipient << std::endl;
                    }
                    inMessageSection = false;
                    continue;
                }

                if (inMessageSection) {
                    if (line == "content_begin") {
                        inContentSection = true;
                        content = "";
                        continue;
                    }
                    else if (line == "content_end") {
                        inContentSection = false;
                        continue;
                    }

                    if (inContentSection) {
                        content += line + "\n";
                    }
                    else {
                        size_t equalPos = line.find('=');
                        if (equalPos != std::string::npos) {
                            std::string key = line.substr(0, equalPos);
                            std::string value = line.substr(equalPos + 1);

                            if (key == "id") {
                                try { id = std::stoi(value); }
                                catch (...) { id = 0; }
                            }
                            else if (key == "sender") sender = value;
                            else if (key == "recipient") recipient = value;
                            else if (key == "title") title = value;
                            else if (key == "timestamp") {
                                try { timestamp = std::stoll(value); }
                                catch (...) { timestamp = 0; }
                            }
                            else if (key == "read") read = (value == "1");
                        }
                    }
                }
            }

            file.close();

            if (highestId >= nextMessageId) {
                nextMessageId = highestId + 1;
            }

            int totalMessages = 0;
            for (const auto& pair : userMessages) {
                totalMessages += pair.second.size();
            }

            std::cout << "Loaded " << totalMessages << " messages for "
                      << userMessages.size() << " users." << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Error loading messages: " << e.what() << std::endl;
        }
    }
};
#endif // MESSAGE_H