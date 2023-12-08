//
// Created by vincent on 23/7/23.
//

#ifndef PIDGIN_STEAM_IPC_PIPE_H
#define PIDGIN_STEAM_IPC_PIPE_H

#include <sys/stat.h>
#include <stdexcept>
#include <optional>
#include <string>
#include <type_traits>
#include <fcntl.h>
#include "version.h"
#include "sslconn.h"
#include "savedstatuses.h"
#include "request.h"
#include "prpl.h"
#include "proxy.h"
#include "dnsquery.h"
#include "debug.h"
#include "connection.h"
#include "core.h"
#include "blist.h"
#include "accountopt.h"
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <dlfcn.h>
#	include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <glib/gi18n.h>
#include <cstring>
#include <cerrno>
#include <glib.h>

template<typename Impl, typename Send, typename Receive>
class NamedPipe {
    std::string recvFilePath_;
public:
    NamedPipe(std::string sendFilePath="", std::string recvFilePath="") : fdSend_(-1) {
        // if (mkfifo(filePath_.c_str(), 0666) < 0) {
        //     throw std::runtime_error("Failed to create named pipe");
        // }
        if (!std::is_void<Send>::value) {
            fdSend_ = open(sendFilePath.c_str(), O_RDWR);
            if (fdSend_ < 0) {
                throw std::runtime_error("Failed to open named pipe");
            }
        }
        recvFilePath_ = recvFilePath;
//        if (!std::is_void<Receive>::value) {
//            fdRecv_ = open(recvFilePath.c_str(), O_RDWR);
//            if (fdRecv_ < 0) {
//                throw std::runtime_error("Failed to open named pipe");
//            }
//        }
    }

    ~NamedPipe() {
        if (fdSend_ >= 0) close(fdSend_);
//        if (fdRecv_ >= 0) close(fdRecv_);
//        unlink(filePath_.c_str());
    }

    template<typename T=Send>
    typename std::enable_if<!std::is_void<T>::value>::type send(const Send& data) {
        std::string serialized = Impl::serialize(data);
        uint32_t len = serialized.size();
        if (ssize_t n = write(fdSend_, &len, sizeof(len)); n < 0) {
            throw std::runtime_error("Failed to write to named pipe");
        }
        if (ssize_t n = write(fdSend_, serialized.c_str(), len); n < 0) {
            throw std::runtime_error("Failed to write to named pipe");
        }
        return;
    }

    bool canReceive() {
        // check if recvFilePath_ has non-zero size with stat
        struct stat st;
        fstatat(AT_FDCWD, recvFilePath_.c_str(), &st, 0);
        auto res = st.st_size > 0;
         // std::cout << "canReceive" << st.st_size << " " << res << "\n";
        return res;

//        if (fdRecv_ < 0) return false;
//        if (fcntl(fdRecv_, F_GETFL, 0) == -1) {
//            return false;
//        }

//        fd_set fds;
//        FD_ZERO(&fds);
//        FD_SET(fdRecv_, &fds);
//        timeval tv = {0, 0};
//        return select(fdRecv_ + 1, &fds, NULL, NULL, &tv) > 0;

        // Check if there's data available in the pipe.

        // If there's data available, read it.
//        if (available & O_NONBLOCK) {
//            char buffer[1024];
//            int bytes_read = read(fd, buffer, sizeof(buffer));
//            if (bytes_read > 0) {
//                printf("Read %d bytes from the pipe: %s\n", bytes_read, buffer);
//            }
//        }
//
//        close(fd);
//        return 0;
    }

    template<typename T=Receive>
    typename std::enable_if<!std::is_void<T>::value, Receive>::type receive() {
        auto fdRecv_ = open(recvFilePath_.c_str(), O_RDWR);
         // std::cout << "open " << fdRecv_ << "\n";
        uint32_t len;
        char buf[65536];
        if (ssize_t n = read(fdRecv_, &len, sizeof(len)); n > sizeof(buf)) {
            throw std::runtime_error("Received message too long");
        }
         // std::cout << "len=" << len << "\n";
        if (ssize_t n = read(fdRecv_, buf, len); n < 0) {
            throw std::runtime_error("Failed to read from named pipe");
        }
         // std::cout << "read bytes\n";
        std::string serialized(buf, len);
        auto res = Impl::deserialize(serialized);
         // std::cout << "trunc\n";

        // now empty the file
        ftruncate(fdRecv_, 0);
         // std::cout << "close\n";
        close(fdRecv_);
         // std::cout << "done\n";
        return res;
    }

protected:
    /*
    std::string serialize(const Send& data) {
        static_assert(!std::is_void<Send>::value);
    }
    Receive deserialize(const std::string& serialized) {
        static_assert(!std::is_void<Receive>::value);
    }
     */

private:
    int fdSend_;
    // int fdRecv_;
};

struct MessagePipe : NamedPipe<MessagePipe, std::string, std::string> {
    MessagePipe() : NamedPipe("/tmp/basic_steam_send.pipe", "/tmp/basic_steam_recv.txt") {}

    static std::string serialize(const std::string& data) {
        return data;
    }

    static std::string deserialize(const std::string& serialized) {
        return serialized;
    }
};

#endif //PIDGIN_STEAM_IPC_PIPE_H
