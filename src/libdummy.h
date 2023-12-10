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
#include "grpc_client_wrapper.h"
#include "grpc_client_wrapper_async.h"
#include "cppcoro/async_scope.hpp"
#include "cppcoro/io_service.hpp"
#include "cppcoro/cancellation_source.hpp"
#include <sys/stat.h>


#include <fcntl.h>
#include <type_traits>
#include <string>
#include <map>
#include <optional>
#include <stdexcept>

#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 12
#	define atoll(a) g_ascii_strtoll(a, NULL, 0)
#endif

#define FB_MAX_MSG_RETRY 2

#define STEAM_PLUGIN_ID "prpl-steam-websockets"
#define STEAM_PLUGIN_VERSION "1.7"

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


struct SteamAccount {
    // libpurple compatibility
    PurpleAccount *account;
    PurpleConnection *pc;

    // authentication
    std::string username, password;
    std::optional<std::string> steamGuardCode;
    std::optional<std::string> refreshToken;

//    std::map<std::string, std::string> cookies;

    // messaging state for websocket connection
    std::map<std::string, int64_t> lastMessageTimestamps;  // TODO: refactor to per-buddy state

    // for stub implementation
    SteamClient::AsyncClientWrapper client{"localhost:8080"};
    guint poll_callback_id;
    cppcoro::cancellation_source cancelTokenSource;
    cppcoro::cancellation_token cancelToken;
    cppcoro::async_scope scope;
    cppcoro::io_service ioService;

    // custom memory allocator
    static void *operator new(size_t size) {
        return g_malloc0(size);
    }

    static void operator delete(void *ptr) {
        g_free(ptr);
    }
};

struct SteamBuddy {
    SteamAccount *sa;
    PurpleBuddy *buddy;

    std::string steamid;
    std::string personaname;
    std::string realname;
    std::string profileurl;
    guint lastlogoff;
    std::string avatar;
    guint personastateflags;

    std::optional<int> gameid;
    std::string gameextrainfo;
    std::string gameserversteamid;
    std::string lobbysteamid;
    std::string gameserverip;
};
#endif

#define STEAMID_IS_GROUP(id) G_UNLIKELY(((g_ascii_strtoll((id), NULL, 10) >> 52) & 0x0F) == 7)

typedef void (*SteamFunc)(SteamAccount *sa);

#endif /* LIBSTEAM_H */
