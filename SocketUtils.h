#ifndef SOCKETUTILS_H
#define SOCKETUTILS_H

#include <sys/fcntl.h>
#include <poll.h>
#include <cstring>
#include <thread>

class SocketUtils
{
public:

    // Set socket to non-blocking mode
    static bool setNonBlocking(int sock)
    {
        int flags = fcntl(sock, F_GETFL, 0);
        if (flags == -1)
        {
            perror("fcntl F_GETFL");
            return false;
        }

        if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            perror("fcntl F_SETFL O_NONBLOCK");
            return false;
        }
        return true;
    }

    // Send data to socket with error handling
    static bool sendData(int sock, const std::string& data)
    {
        // Check for valid socket
        if (sock < 0) {
            return false;
        }

        ssize_t total = 0;
        ssize_t bytesleft = data.length();
        ssize_t n;

        while (total < static_cast<ssize_t>(data.length()))
        {
            n = send(sock, data.c_str() + total, bytesleft, 0);
            if (n == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // Would block, try again later
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                else
                {
                    // Log the error but don't use perror here
                    return false;
                }
            }
            total += n;
            bytesleft -= n;
        }
        return true;
    }

    // Receive data from socket with timeout
    static std::string receiveData(int sock, int timeout_ms = 1000)
    {
        char buffer[4096];
        std::string received;
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, timeout_ms);
        if (ret > 0)
        {
            if (pfd.revents & POLLIN)
            {
                memset(buffer, 0, sizeof(buffer));
                ssize_t nbytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
                if (nbytes > 0)
                {
                    received = std::string(buffer, nbytes);
                }
            }
        }
        else if (ret == 0)
        {
            // Timeout
        }
        else
        {
            perror("poll");
        }
        return received;
    }
};

#endif //SOCKETUTILS_H