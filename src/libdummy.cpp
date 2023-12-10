// From https://github.com/EionRobb/pidgin-opensteamworks/blob/master/steam-mobile/libsteam.c
#include "libdummy.h"
#include "cppcoro/task.hpp"
#include "cppcoro/sync_wait.hpp"
#include <stdexcept>
#include <json/value.h>
#include <json/reader.h>
#include <json/writer.h>
#include <thread>


static constexpr bool core_is_haze = false;


bool read_last_timestamps(SteamAccount &sa) {
    // Read as JSON mapping of string SteamIDs to int timestamps
    auto rawMappings = purple_account_get_string(sa.account, "last_message_timestamps", nullptr);
    if (rawMappings == nullptr) {
        return false;
    }

    purple_debug_info("dummy", "steam_login read last_message_timestamps %s\n", rawMappings);
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(rawMappings, root)) {
        purple_debug_info("dummy", "steam_login failed to parse last_message_timestamps\n");
    } else {
        for (auto &x: root.getMemberNames()) {
            sa.lastMessageTimestamps[x] = root[x].asInt64();
        }
    }
    return true;
}

void write_last_timestamps(SteamAccount &sa) {
    // Write as JSON mapping of string SteamIDs to int timestamps
    Json::Value root;
    for (auto &x: sa.lastMessageTimestamps) {
        root[x.first] = x.second;
    }
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string rawMappings = Json::writeString(builder, root);
    purple_debug_info("dummy", "steam_login write last_message_timestamps %s\n", rawMappings.c_str());
    purple_account_set_string(sa.account, "last_message_timestamps", rawMappings.c_str());
}

static const char *steam_list_icon(PurpleAccount *account, PurpleBuddy *buddy) {
    purple_debug_info("dummy", "steam_list_icon start\n");
    return "dummy";
}

static gchar *steam_status_text(PurpleBuddy *buddy) {
    purple_debug_info("dummy", "steam_status_text start\n");
    auto *sbuddy = static_cast<SteamBuddy *>(buddy->proto_data);
    if (sbuddy && !sbuddy->gameextrainfo.empty()) {
        if (sbuddy->gameid.has_value()) {
            return g_markup_printf_escaped("In game %s", sbuddy->gameextrainfo.c_str());
        } else {
            return g_markup_printf_escaped("In non-Steam game %s", sbuddy->gameextrainfo.c_str());
        }
    }
    return nullptr;
}

static void steam_close(PurpleConnection *pc) {
    purple_debug_info("dummy", "steam_close start\n");
    auto *sa = static_cast<SteamAccount *>(pc->proto_data);

    sa->cancelTokenSource.request_cancellation();
    sa->client.shutdown();
    std::thread([=]() {
        cppcoro::sync_wait(sa->scope.join());  // TODO: check if this deadlocks
        purple_timeout_remove(sa->poll_callback_id);
        sa->ioService.stop();
        delete sa;
    }).detach();
}

static const gchar *steam_personastate_to_statustype(gint64 state) {
    const char *status_id;
    PurpleStatusPrimitive prim;
    switch (state) {
        default:
        case 0: //Offline
            prim = PURPLE_STATUS_OFFLINE;
            break;
        case 1: //Online
            prim = PURPLE_STATUS_AVAILABLE;
            break;
        case 2: //Busy
            prim = PURPLE_STATUS_UNAVAILABLE;
            break;
        case 3: //Away
            prim = PURPLE_STATUS_AWAY;
            break;
        case 4: //Snoozing
            prim = PURPLE_STATUS_EXTENDED_AWAY;
            break;
        case 5: //Looking to trade
            return "trade";
        case 6: //Looking to play
            return "play";
    }
    status_id = purple_primitive_get_id_from_type(prim);
    return status_id;
}

cppcoro::task<void> receive_messages(SteamAccount &sa) {
    std::optional<int64_t> lastTimestampNs;
    auto friendsList = co_await sa.client.getFriendsList();  // TODO: store in SteamAccount
    int max_iterations = 3;
    for (auto &x: friendsList.buddies) {
        if (!purple_find_buddy(sa.account, x.id.c_str())) {
            purple_debug_info("dummy", "receive_messages %s add buddy %s\n", sa.account->username, x.id.c_str());
            auto buddy = purple_buddy_new(sa.account, x.id.c_str(), nullptr);
            buddy->proto_data = new SteamBuddy{&sa, buddy, x.id, x.nickname, "", "", 0, "", 0};
            purple_blist_add_buddy(buddy, nullptr, purple_find_group("Steam"), nullptr);
        }
        purple_debug_info("dummy", "receive_messages %s update buddy %s %s\n", sa.account->username, x.id.c_str(),
                          x.nickname.c_str());
        purple_serv_got_private_alias(sa.pc, x.id.c_str(), x.nickname.c_str());
        purple_prpl_got_user_status(sa.account, x.id.c_str(), steam_personastate_to_statustype(x.personaState), nullptr);
        auto buddy = static_cast<PurpleBuddy *>(purple_find_buddy(sa.account, x.id.c_str()));
        if (buddy->proto_data == nullptr) {
            buddy->proto_data = new SteamBuddy{&sa, buddy, x.id, x.nickname, "", "", 0, "", 0};
        }
        auto sbuddy = static_cast<SteamBuddy *>(buddy->proto_data);  // TODO
        sbuddy->gameextrainfo = x.gameExtraInfo;
        sbuddy->gameid = x.gameid;

        auto otherId = x.id;
        PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, otherId.c_str(),
                                                                         sa.account);

        for (int i = 0; i < max_iterations; ++i) {
            std::optional<int64_t> startTimestampNs;
            if (auto it = sa.lastMessageTimestamps.find(x.id); it != sa.lastMessageTimestamps.end()) {
                startTimestampNs = it->second;
            }
            auto messages = co_await sa.client.getMessages(x.id, startTimestampNs, lastTimestampNs);
            if (messages.empty()) {
                break;
            }
            for (auto &msg: messages) {
                purple_debug_info("dummy", "receive_messages received %s\n", msg.message.c_str());
                //        if (real_timestamp > sa->last_message_timestamp)
                gchar *text, *html;
                //            const gchar *otherId;
                //            if (g_str_equal(type, "emote") || g_str_equal(type, "my_emote"))
                //            {
                //                text = g_strconcat("/me ", json_object_get_string_member(message, "text"), nullptr);
                //            } else {
                //                text = g_strdup(json_object_get_string_member(message, "text"));
                //            }
                html = purple_markup_escape_text(msg.message.c_str(), -1);  // TODO: input is BBCode
                //            otherId = json_object_get_string_member(message, "steamid_from");
                //        if (g_str_has_prefix(type, "my_")) {
                purple_debug_info("dummy", "receive_messages resolve conv %p\n", conv);
                if (conv == nullptr) {
                    conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, sa.account, otherId.c_str());
                    purple_debug_info("dummy", "receive_messages make new conv %p\n", conv);
                }
                purple_conversation_write(conv, otherId.c_str(), html,
                                          msg.senderId == friendsList.me->id ? PURPLE_MESSAGE_SEND
                                                                             : PURPLE_MESSAGE_RECV,
                                          msg.timestamp_ns / 1000000000LL);
                //        } else {
                //            serv_got_im(sa.pc, otherId, html, PURPLE_MESSAGE_RECV, real_timestamp);
                //        }

                purple_debug_info("dummy", "receive_messages done\n");
                g_free(html);
                //        g_free(text);

                lastTimestampNs = std::min(msg.timestamp_ns, lastTimestampNs.value_or(msg.timestamp_ns));
                sa.lastMessageTimestamps[x.id] = msg.timestamp_ns + 1;
            }
        }
    }
    write_last_timestamps(sa);
    co_return;
}

gboolean poll_for_messages(PurpleConnection *pc) {
    purple_debug_info("dummy", "poll_for_messages start %p\n", pc);
    SteamAccount &sa = *static_cast<SteamAccount *>(pc->proto_data);
    sa.ioService.process_pending_events();
    purple_debug_info("dummy", "poll_for_messages end %p\n", pc);
    return G_SOURCE_CONTINUE;
}

cppcoro::task<int> send_message(PurpleConnection *pc, SteamAccount &sa, const std::string &who, const std::string &msg) {
    purple_debug_info("dummy", "send_message with %s %s\n", who.c_str(), msg.c_str());
    switch (co_await sa.client.sendMessage(who, msg)) {
        case SteamClient::SEND_SUCCESS:
            co_return 0;
        case SteamClient::SEND_INVALID_SESSION_KEY:
            purple_connection_error_reason(pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
                                           "Invalid session key");
            co_return -ENOTCONN;
        case SteamClient::SEND_INVALID_TARGET_ID:
            purple_connection_error_reason(pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
                                           "Invalid target ID");
            co_return -1;
        case SteamClient::SEND_INVALID_MESSAGE:
            purple_connection_error_reason(pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
                                           "Invalid message");
            co_return -2;
        case SteamClient::SEND_UNKNOWN_FAILURE:
            purple_connection_error_reason(pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
                                           "Unknown error");
            co_return -3;
    }
}

static gint steam_send_im(PurpleConnection *pc, const gchar *who, const gchar *msg,
                          PurpleMessageFlags flags) {
    purple_debug_info("dummy", "steam_send_im start\n");
    SteamAccount &sa = *static_cast<SteamAccount *>(pc->proto_data);
    sa.scope.spawn(send_message(pc, sa, std::string{who}, std::string{msg}));
    return 1;
}

static void steam_buddy_free(PurpleBuddy *buddy) {
    purple_debug_info("dummy", "steam_buddy_free start\n");
    delete static_cast<SteamBuddy *>(buddy->proto_data);
    buddy->proto_data = nullptr;
}

#if PURPLE_VERSION_CHECK(3, 0, 0)
void steam_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group, const char* message)
#else
void steam_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
#endif
{
    purple_debug_info("dummy", "steam_add_buddy start\n");
    purple_debug_info("dummy", "steam_add_buddy with %s\n", buddy->name);

//    SteamAccount *sa = pc->proto_data;
//
//    if (g_ascii_strtoull(buddy->name, nullptr, 10)) {
//        steam_friend_action(sa, buddy->name, "add");
//    } else {
//        purple_blist_remove_buddy(buddy);
//        purple_notify_warning(pc, "Invalid friend id", "Invalid friend id",
//                              "Friend ID's must be numeric.\nTry searching from the account menu.");
//    }

    if (std::string(buddy->name) == "other") {
        purple_debug_info("dummy", "steam_add_buddy with %s -> OK\n", buddy->name);
    } else {
        purple_blist_remove_buddy(buddy);
        purple_notify_warning(pc, "Invalid friend id", "Invalid friend id",
                              "Friend ID's must be numeric.\nTry searching from the account menu.");
    }
}

static gboolean plugin_load(PurplePlugin *plugin) {
    purple_debug_info("dummy", "plugin_load start\n");
    return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin) {
    purple_debug_info("dummy", "plugin_unload start\n");
//#ifdef G_OS_UNIX
//#ifdef USE_GNOME_KEYRING
//    if (gnome_keyring_lib) {
//		dlclose(gnome_keyring_lib);
//		gnome_keyring_lib = nullptr;
//	}
//#else // !USE_GNOME_KEYRING
//    if (secret_lib) {
//        dlclose(secret_lib);
//        secret_lib = nullptr;
//    }
//#endif // USE_GNOME_KEYRING
//#endif
    return TRUE;
}

void steam_search_users(PurplePluginAction *action) {
    purple_debug_info("dummy", "steam_search_users start\n");  // TODO
}

void steam_register_game_key(PurplePluginAction *action) {
    purple_debug_info("dummy", "steam_register_game_key start\n");  // TODO
}

static GList *steam_actions(PurplePlugin *plugin, gpointer context) {
    purple_debug_info("dummy", "steam_actions start\n");
    GList *m = nullptr;
    PurplePluginAction *act;
    act = purple_plugin_action_new(_("Search for friends..."), steam_search_users);
    m = g_list_append(m, act);
    act = purple_plugin_action_new(_("Redeem game key..."), steam_register_game_key);
    m = g_list_append(m, act);
    return m;
}


GList *steam_status_types(PurpleAccount *account) {
    purple_debug_info("dummy", "steam_status_types start\n");
    GList *types = nullptr;
    PurpleStatusType *status;

    purple_debug_info("steam", "status_types\n");

    status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, nullptr, "Online", TRUE, TRUE, FALSE);
    types = g_list_append(types, status);
    status = purple_status_type_new_full(PURPLE_STATUS_OFFLINE, nullptr, "Offline", TRUE, TRUE, FALSE);
    types = g_list_append(types, status);
    status = purple_status_type_new_full(PURPLE_STATUS_UNAVAILABLE, nullptr, "Busy", TRUE, TRUE, FALSE);
    types = g_list_append(types, status);
    status = purple_status_type_new_full(PURPLE_STATUS_AWAY, nullptr, "Away", TRUE, TRUE, FALSE);
    types = g_list_append(types, status);
    status = purple_status_type_new_full(PURPLE_STATUS_EXTENDED_AWAY, nullptr, "Snoozing", TRUE, TRUE, FALSE);
    types = g_list_append(types, status);

//    status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, "trade", "Looking to Trade", TRUE, FALSE, FALSE);
//    types = g_list_append(types, status);
//    status = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE, "play", "Looking to Play", TRUE, FALSE, FALSE);
//    types = g_list_append(types, status);

    if (core_is_haze) {
        // Telepathy-Haze only displays status_text if the status has a "message" attr
        GList *iter;
        for (iter = types; iter; iter = iter->next) {
            purple_status_type_add_attr(static_cast<PurpleStatusType *>(iter->data), "message", "Game Title",
                                        purple_value_new(PURPLE_TYPE_STRING));
        }
    }

    // Independent, unsettable status for being in-game
    status = purple_status_type_new_with_attrs(PURPLE_STATUS_TUNE,
                                               "ingame", nullptr, FALSE, FALSE, TRUE,
                                               "game", "Game Title", purple_value_new(PURPLE_TYPE_STRING),
                                               nullptr);
    types = g_list_append(types, status);

    return types;
}

const gchar *steam_list_emblem(PurpleBuddy *buddy) {
    purple_debug_info("dummy", "steam_list_emblem start\n");
    return nullptr;
}

static GList *steam_node_menu(PurpleBlistNode *node) {
    purple_debug_info("dummy", "steam_node_menu start\n");
    return nullptr;
}

static unsigned int steam_send_typing(PurpleConnection *pc, const gchar *name, PurpleTypingState state) {
    purple_debug_info("dummy", "steam_send_typing start\n");
    return 1;
}

cppcoro::task<void> attempt_login(PurpleConnection *pc, SteamAccount &sa) {
    purple_debug_info("debug", "steam_login with creds %s %s\n", sa.username.c_str(), sa.password.c_str());

    purple_connection_set_state(pc, PURPLE_CONNECTING);
    purple_connection_update_progress(pc, _("Connecting"), 1, 3);

    SteamClient::AuthResponseState res;
    if (sa.client.shouldReset()) {
        sa.client.resetSessionKey();
    }

    for (int i = 0; i < 2; ++i) {
        purple_debug_info("dummy", "steam_login authenticate attempt %d\n", i);
        // TODO: verify auth flow
        // TODO: wait for Steam Guard code (since Steam will send an email with a new code for each login attempt)
        res = co_await sa.client.authenticate(sa.username, sa.password, sa.steamGuardCode);
        switch (res) {
            case SteamClient::AUTH_SUCCESS:
                purple_debug_info("dummy", "steam_login authenticate success\n");
                purple_connection_set_state(pc, PURPLE_CONNECTED);
                purple_connection_update_progress(pc, _("Connected"), 2, 3);
                co_return;
            case SteamClient::AUTH_INVALID_CREDENTIALS:
                purple_debug_info("dummy", "steam_login authenticate invalid credentials\n");
                purple_connection_error_reason(pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
                                               "Invalid username or password");
                co_return;
            case SteamClient::AUTH_PENDING_STEAM_GUARD_CODE:
                purple_debug_info("dummy", "steam_login authenticate pending Steam Guard code\n");
                purple_connection_update_progress(pc, _("Pending Steam Guard code"), 1, 3);
//                purple_connection_error_reason(pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
//                                               "Steam Guard code required");
                break;
            case SteamClient::AUTH_UNKNOWN_FAILURE:
                purple_debug_info("dummy", "steam_login authenticate unknown failure\n");
                purple_connection_error_reason(pc, PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
                                               "Unknown error");
                co_return;
        }
    }
    co_return;
}

static void steam_login(PurpleAccount *account) {
    purple_debug_info("dummy", "steam_login start\n");
    PurpleConnection *pc = purple_account_get_connection(account);
    if (pc->proto_data != nullptr) {
        purple_connection_error_reason(pc, PURPLE_CONNECTION_ERROR_INVALID_USERNAME,
                                       "Already logged in");
        return;
    }
    auto *p_sa = new SteamAccount();
    pc->proto_data = p_sa;
    SteamAccount &sa = *p_sa;

    if (!purple_ssl_is_supported()) {
        purple_connection_error_reason(pc,
                                       PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT,
                                       "Server requires TLS/SSL for login.  No TLS/SSL support found.");
        return;
    }

    purple_debug_info("dummy", "steam_login check\n");

    sa.account = account;
    sa.pc = pc;
    sa.username = sa.account->username;
    sa.password = sa.account->password;
    sa.poll_callback_id = purple_timeout_add(100, (GSourceFunc) poll_for_messages, pc);

    // sa->hostname_ip_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    // sa->sent_messages_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, nullptr);
    // sa->waiting_conns = g_queue_new();
//    sa.last_message_timestamp = purple_account_get_int(sa.account, "last_message_timestamp", 0);
    read_last_timestamps(sa);

    if (const char *x = purple_account_get_string(account, "refreshToken", nullptr)) {
        sa.refreshToken = x;
        // steam_login_with_access_token(sa);
    } else {
        sa.refreshToken = {};
        // steam_get_rsa_key(sa);
    }

    sa.cancelToken = sa.cancelTokenSource.token();

    sa.scope.spawn(sa.client.run_cq(reinterpret_cast<SteamAccount*>(pc->proto_data)->ioService));
    sa.scope.spawn(attempt_login(pc, sa));
    sa.scope.spawn([](SteamAccount &sa) -> cppcoro::task<void> {
        while (!sa.cancelToken.is_cancellation_requested()) {
            co_await sa.ioService.schedule_after(std::chrono::milliseconds(200));
            if (sa.client.isSessionKeySet()) {
                co_await receive_messages(sa);
            }
        }
    }(*reinterpret_cast<SteamAccount*>(pc->proto_data)));
}

//static void steam_login_access_token_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data) { }
//static void steam_login_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data) { }
//static void steam_login_got_rsakey(SteamAccount *sa, JsonObject *obj, gpointer user_data) { }
//static void steam_login_with_access_token(SteamAccount *sa) { }
//static void steam_login_with_access_token_error_cb(SteamAccount *sa, const gchar *data, gssize data_len, gpointer user_data) { }
static void steam_set_idle(PurpleConnection *pc, int time) {
    purple_debug_info("dummy", "steam_set_idle start\n");
}

static void steam_set_status(PurpleAccount *account, PurpleStatus *status) {
    purple_debug_info("dummy", "steam_set_status start\n");
}

void steam_buddy_remove(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group) {
    purple_debug_info("dummy", "steam_buddy_remove start\n");
}

void steam_fake_group_buddy(PurpleConnection *pc, const char *who, const char *old_group, const char *new_group) {
    purple_debug_info("dummy", "steam_fake_group_buddy start\n");
}

void steam_fake_group_rename(PurpleConnection *pc, const char *old_name, PurpleGroup *group, GList *moved_buddies) {
    purple_debug_info("dummy", "steam_fake_group_rename start\n");
}

void steam_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full) {
    purple_debug_info("dummy", "steam_tooltip_text start\n");
}


static void plugin_init(PurplePlugin *plugin) {
    purple_debug_info("dummy", "plugin_init start\n");

    PurpleAccountOption *option;
    PurplePluginInfo *info = plugin->info;
    auto *prpl_info = static_cast<PurplePluginProtocolInfo *>(info->extra_info);
    GList *ui_mode_list = nullptr;
    PurpleKeyValuePair *kvp;

    option = purple_account_option_string_new(
            "Steam Guard Code",
            "steam_guard_code", "");
    prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

    option = purple_account_option_bool_new(
            "Always use HTTPS",
            "always_use_https", TRUE);
    prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

    option = purple_account_option_bool_new(
            "Change status when in-game",
            "change_status_to_game", FALSE);
    prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

    option = purple_account_option_bool_new(
            "Download offline history",
            "download_offline_history", TRUE);
    prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

    kvp = g_new0(PurpleKeyValuePair, 1);
    kvp->key = g_strdup(_("Mobile"));
    kvp->value = g_strdup("mobile");
    ui_mode_list = g_list_append(ui_mode_list, kvp);

    kvp = g_new0(PurpleKeyValuePair, 1);
    kvp->key = g_strdup(_("Web"));
    kvp->value = g_strdup("web");
    ui_mode_list = g_list_append(ui_mode_list, kvp);

    option = purple_account_option_list_new("Identify as", "ui_mode", ui_mode_list);
    prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

    purple_debug_info("dummy", "plugin_init OK\n");
}


static PurplePluginProtocolInfo prpl_info = {
#if PURPLE_VERSION_CHECK(3, 0, 0)
        sizeof(PurplePluginProtocolInfo),	/* struct_size */
#endif
        /* options */
        OPT_PROTO_MAIL_CHECK,

        nullptr,                   /* user_splits */
        nullptr,                   /* protocol_options */
        /* NO_BUDDY_ICONS */    /* icon_spec */
        {const_cast<char *>("png,jpeg"), 0, 0, 64, 64, 0, PURPLE_ICON_SCALE_DISPLAY}, /* icon_spec */
        steam_list_icon,           /* list_icon */
        steam_list_emblem,         /* list_emblems */
        steam_status_text,         /* status_text */
        steam_tooltip_text,        /* tooltip_text */
        steam_status_types,        /* status_types */
        steam_node_menu,           /* blist_node_menu */
        nullptr,//steam_chat_info,           /* chat_info */
        nullptr,//steam_chat_info_defaults,  /* chat_info_defaults */
        steam_login,               /* login */
        steam_close,               /* close */
        steam_send_im,             /* send_im */
        nullptr,                      /* set_info */
        steam_send_typing,         /* send_typing */
        nullptr,//steam_get_info,            /* get_info */
        steam_set_status,          /* set_status */
        steam_set_idle,            /* set_idle */
        nullptr,                   /* change_passwd */
        steam_add_buddy,           /* add_buddy */
        nullptr,                   /* add_buddies */
        steam_buddy_remove,        /* remove_buddy */
        nullptr,                   /* remove_buddies */
        nullptr,                   /* add_permit */
        nullptr,                   /* add_deny */
        nullptr,                   /* rem_permit */
        nullptr,                   /* rem_deny */
        nullptr,                   /* set_permit_deny */
        nullptr,//steam_fake_join_chat,      /* join_chat */
        nullptr,                   /* reject chat invite */
        nullptr,//steam_get_chat_name,       /* get_chat_name */
        nullptr,                   /* chat_invite */
        nullptr,//steam_chat_fake_leave,     /* chat_leave */
        nullptr,                   /* chat_whisper */
        nullptr,//steam_chat_send,           /* chat_send */
        nullptr,                   /* keepalive */
        nullptr,                   /* register_user */
        nullptr,                   /* get_cb_info */
#if !PURPLE_VERSION_CHECK(3, 0, 0)
        nullptr,                   /* get_cb_away */
#endif
        nullptr,                   /* alias_buddy */
        steam_fake_group_buddy,    /* group_buddy */
        steam_fake_group_rename,   /* rename_group */
        steam_buddy_free,          /* buddy_free */
        nullptr,//steam_conversation_closed, /* convo_closed */
        purple_normalize_nocase,/* normalize */
        nullptr,                   /* set_buddy_icon */
        nullptr,//steam_group_remove,        /* remove_group */
        nullptr,                   /* get_cb_real_name */
        nullptr,                   /* set_chat_topic */
        nullptr,                   /* find_blist_chat */
        nullptr,                   /* roomlist_get_list */
        nullptr,                   /* roomlist_cancel */
        nullptr,                   /* roomlist_expand_category */
        nullptr,                   /* can_receive_file */
        nullptr,                   /* send_file */
        nullptr,                   /* new_xfer */
        nullptr,                   /* offline_message */
        nullptr,                   /* whiteboard_prpl_ops */
        nullptr,                   /* send_raw */
        nullptr,                   /* roomlist_room_serialize */
        nullptr,                   /* unregister_user */
        nullptr,                   /* send_attention */
        nullptr,                   /* attention_types */
#if (PURPLE_MAJOR_VERSION == 2 && PURPLE_MINOR_VERSION >= 5) || PURPLE_MAJOR_VERSION > 2
#if PURPLE_MAJOR_VERSION == 2 && PURPLE_MINOR_VERSION >= 5
        sizeof(PurplePluginProtocolInfo), /* struct_size */
#endif
        nullptr, // steam_get_account_text_table, /* get_account_text_table */
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
#else
        (gpointer) sizeof(PurplePluginProtocolInfo)
#endif
};

static PurplePluginInfo info = {
        PURPLE_PLUGIN_MAGIC,
        PURPLE_MAJOR_VERSION,                /* major_version */
        PURPLE_MINOR_VERSION,                /* minor version */
        PURPLE_PLUGIN_PROTOCOL,            /* type */
        nullptr,                        /* ui_requirement */
        0,                        /* flags */
        nullptr,                        /* dependencies */
        PURPLE_PRIORITY_DEFAULT,            /* priority */
        const_cast<char *>(STEAM_PLUGIN_ID),                /* id */
        const_cast<char *>("Dummy"),                    /* name */
        const_cast<char *>(STEAM_PLUGIN_VERSION),            /* version */
        const_cast<char *>("Dummy Plugin"),        /* summary */
        const_cast<char *>("Dummy Plugin"),        /* description */
        const_cast<char *>("Vincent"),        /* author */
        const_cast<char *>("https://localhost:8080/"),    /* homepage */
        plugin_load,                    /* load */
        plugin_unload,                    /* unload */
        nullptr,                        /* destroy */
        nullptr,                        /* ui_info */
        &prpl_info,                    /* extra_info */
        nullptr,                        /* prefs_info */
        steam_actions,                    /* actions */

        /* padding */
        nullptr,
        nullptr,
        nullptr,
        nullptr
};

extern "C" {
PURPLE_INIT_PLUGIN(dummy, plugin_init, info) ;
}
