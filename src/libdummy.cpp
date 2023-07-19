// From https://github.com/EionRobb/pidgin-opensteamworks/blob/master/steam-mobile/libsteam.c
#include "libdummy.h"

static const char *steam_list_icon(PurpleAccount *account, PurpleBuddy *buddy) {
    purple_debug_info("dummy", "steam_list_icon start\n");
    return "dummy";
}

static gchar *steam_status_text(PurpleBuddy *buddy) {
    purple_debug_info("dummy", "steam_status_text start\n");
//    SteamBuddy *sbuddy = buddy->proto_data;
//    if (sbuddy && sbuddy->gameextrainfo) {
//        if (sbuddy->gameid && *(sbuddy->gameid)) {
//            return g_markup_printf_escaped("In game %s", sbuddy->gameextrainfo);
//        } else {
//            return g_markup_printf_escaped("In non-Steam game %s", sbuddy->gameextrainfo);
//        }
//    }
    return nullptr;
}

static void steam_close(PurpleConnection *pc) {
    purple_debug_info("dummy", "steam_close start\n");
}

static gint steam_send_im(PurpleConnection *pc, const gchar *who, const gchar *msg,
                          PurpleMessageFlags flags) {
    purple_debug_info("dummy", "steam_send_im start\n");
    return 1;
}

static void steam_buddy_free(PurpleBuddy *buddy) {
    purple_debug_info("dummy", "steam_buddy_free start\n");
}

#if PURPLE_VERSION_CHECK(3, 0, 0)
void steam_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group, const char* message)
#else

void steam_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
#endif
{
    purple_debug_info("dummy", "steam_add_buddy start\n");
//    SteamAccount *sa = pc->proto_data;
//
//    if (g_ascii_strtoull(buddy->name, nullptr, 10)) {
//        steam_friend_action(sa, buddy->name, "add");
//    } else {
//        purple_blist_remove_buddy(buddy);
//        purple_notify_warning(pc, "Invalid friend id", "Invalid friend id",
//                              "Friend ID's must be numeric.\nTry searching from the account menu.");
//    }
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
    purple_debug_info("dummy", "steam_search_users start\n");
}

void steam_register_game_key(PurplePluginAction *action) {
    purple_debug_info("dummy", "steam_register_game_key start\n");
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
}

const gchar *steam_list_emblem(PurpleBuddy *buddy) {
    purple_debug_info("dummy", "steam_list_emblem start\n");
}

static GList *steam_node_menu(PurpleBlistNode *node) {
    purple_debug_info("dummy", "steam_node_menu start\n");
}

static unsigned int steam_send_typing(PurpleConnection *pc, const gchar *name, PurpleTypingState state) {
    purple_debug_info("dummy", "steam_send_typing start\n");
}

static void steam_login(PurpleAccount *account) {
    purple_debug_info("dummy", "steam_login start\n");
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
    PURPLE_INIT_PLUGIN(dummy, plugin_init, info);
}
