#ifndef LIBSTEAM_H
#define LIBSTEAM_H

/* Maximum number of simultaneous connections to a server */
#define STEAM_MAX_CONNECTIONS 16

#include <glib.h>

#include <cerrno>
#include <cstring>
#include <glib/gi18n.h>
#include <sys/types.h>
#ifdef __GNUC__
#include <unistd.h>
#endif

#ifndef G_GNUC_NULL_TERMINATED
#	if __GNUC__ >= 4
#		define G_GNUC_NULL_TERMINATED __attribute__((__sentinel__))
#	else
#		define G_GNUC_NULL_TERMINATED
#	endif /* __GNUC__ >= 4 */
#endif /* G_GNUC_NULL_TERMINATED */

#ifdef _WIN32
#	include "win32dep.h"
#	define dlopen(a,b) LoadLibrary(a)
#	define RTLD_LAZY
#	define dlsym(a,b) GetProcAddress(a,b)
#	define dlclose(a) FreeLibrary(a)
#else
#	include <arpa/inet.h>
#	include <dlfcn.h>
#	include <netinet/in.h>
#	include <sys/socket.h>
#endif

//#include <json-glib/json-glib.h>
//
//#define json_object_get_int_member(JSON_OBJECT, MEMBER) \
//	(json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_int_member(JSON_OBJECT, MEMBER) : 0)
//#define json_object_get_string_member(JSON_OBJECT, MEMBER) \
//	(json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_string_member(JSON_OBJECT, MEMBER) : NULL)
//#define json_object_get_array_member(JSON_OBJECT, MEMBER) \
//	(json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_array_member(JSON_OBJECT, MEMBER) : NULL)
//#define json_object_get_object_member(JSON_OBJECT, MEMBER) \
//	(json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_object_member(JSON_OBJECT, MEMBER) : NULL)
//#define json_object_get_boolean_member(JSON_OBJECT, MEMBER) \
//	(json_object_has_member(JSON_OBJECT, MEMBER) ? json_object_get_boolean_member(JSON_OBJECT, MEMBER) : FALSE)

#ifndef PURPLE_PLUGINS
#	define PURPLE_PLUGINS
#endif

#include "accountopt.h"
#include "blist.h"
#include "core.h"
#include "connection.h"
#include "debug.h"
#include "dnsquery.h"
#include "proxy.h"
#include "prpl.h"
#include "request.h"
#include "savedstatuses.h"
#include "sslconn.h"
#include "version.h"


#include <fcntl.h>
#include <type_traits>
#include <string>
#include <optional>
#include <stdexcept>
#include <sys/stat.h>

#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 12
#	define atoll(a) g_ascii_strtoll(a, NULL, 0)
#endif

#define FB_MAX_MSG_RETRY 2

#define STEAM_PLUGIN_ID "prpl-steam-websockets"
#define STEAM_PLUGIN_VERSION "1.7"

#define STEAM_CAPTCHA_URL "https://steamcommunity.com/public/captcha.php?gid=%s"

#ifdef ORIGINAL_PIDGIN_IMPLEMENTATION
struct SteamAccount {
    PurpleAccount *account;
    PurpleConnection *pc;
    GSList *conns; /**< A list of all active SteamConnections */
    GQueue *waiting_conns; /**< A list of all SteamConnections waiting to process */
    GSList *dns_queries;
    GHashTable *cookie_table;
    GHashTable *hostname_ip_cache;

    GHashTable *sent_messages_hash;
    guint poll_timeout;

    gchar *umqid;
    guint message;
    gchar *steamid;
    gchar *sessionid;
    gint idletime;
    guint last_message_timestamp;
    gchar *cached_access_token;

    guint watchdog_timeout;

    gchar *captcha_gid;
    gchar *captcha_text;
    gchar *twofactorcode;
};

struct SteamBuddy {
    SteamAccount *sa;
    PurpleBuddy *buddy;

    gchar *steamid;
    gchar *personaname;
    gchar *realname;
    gchar *profileurl;
    guint lastlogoff;
    gchar *avatar;
    guint personastateflags;

    gchar *gameid;
    gchar *gameextrainfo;
    gchar *gameserversteamid;
    gchar *lobbysteamid;
    gchar *gameserverip;
};
#else
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


struct SteamAccount {
    // libpurple compatibility
    PurpleAccount *account;
    PurpleConnection *pc;

    // authentication
    std::string username, password;
    std::optional<std::string> steamGuardCode;
    std::optional<std::string> accessToken, refreshToken;

    // messaging state for websocket connection
    int last_message_timestamp;

    // for stub implementation
    MessagePipe messagePipe;
    guint poll_callback_id;

    // custom memory allocator
    static void *operator new(size_t size) {
        return g_malloc0(size);
    }

    static void operator delete(void *ptr) {
        g_free(ptr);
    }
};
#endif

#define STEAMID_IS_GROUP(id) G_UNLIKELY(((g_ascii_strtoll((id), NULL, 10) >> 52) & 0x0F) == 7)

typedef void (*SteamFunc)(SteamAccount *sa);

#endif /* LIBSTEAM_H */
