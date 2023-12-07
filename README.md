## Resources

* [Pidgin development FAQ](https://pidgin.im/development/faq/)
    * [Qt bindings for libpurple](https://github.com/gatlin/QPurple)
        Sadly very old, last commit is 10 years ago
    * [LINE client written in C++](https://github.com/supersonictw/purple-line/tree/master/libpurple)
* Protobufs:
    * [Protobuf tutorial for Python](https://protobuf.dev/getting-started/pythontutorial/)
    * https://dev.to/techschoolguru/protocol-buffer-deep-dive-52d9
	* [Protobuf for TypeScript](https://dev.to/devaddict/use-grpc-with-node-js-and-typescript-3c58)
* Steam
    * **[Node.js implementation of Steam client](https://github.com/DoctorMcKay/node-steam-user)**


## Build

Dependencies:
* CMake
* protobuf
  * protoc

```
buf generate
```

### Protobuf

protoc tooling really sucks
it's even worse for TypeScript

fix: https://github.com/blokur/grpc-ts-demo/blob/master/tsconfig.json
to allow JS files to be copied over:
```
    "allowJs": true,
    "sourceMap": true,
```

generate JS directly (not what we want):
```
~/.../pidgin-steam/nodejs >>> grpc_tools_node_protoc --js_out=import_style=commonjs,binary:../protobufs/comm_protobufs  --grpc_out=../protobufs/comm_protobufs --plugin=protoc-gen-grpc=`which grpc_tools_node_protoc_plugin` -I="../protobufs_src/comm_protobufs" ../protobufs_src/comm_protobufs/*.proto
```

generate only `.js` and `.d.ts` (but not `.ts`):
```
protoc \
-I=../protobufs_src/comm_protobufs \
--plugin=protoc-gen-ts=./node_modules/.bin/protoc-gen-ts \
--plugin=protoc-gen-grpc=./node_modules/grpc-tools/bin/grpc_node_plugin \
--js_out=import_style=commonjs,binary:../protobufs/comm_protobufs \
--grpc_out=../protobufs/comm_protobufs \
--ts_out=service=grpc-node:../protobufs/comm_protobufs \
../protobufs_src/comm_protobufs/*.proto
```

broken plugin: `grpc_tools_node_protoc`
```
--plugin=protoc-gen-grpc=./node_modules/.bin/grpc_tools_node_protoc \
```
https://medium.com/cloud-native-daily/building-high-performance-microservices-with-node-js-grpc-and-typescript-ddef5e0bdb95

works, but generates grpc-web instead of grpc-node: `grpc_node_plugin`
```
--plugin=protoc-gen-grpc=./node_modules/grpc-tools/bin/grpc_node_plugin \
```

### protobuf-es

https://github.com/bufbuild/protobuf-es

doesn't generate files for service

### Protobuf for C++, CMake build

Unfortunately the CMake documentation is also lacking
https://stackoverflow.com/questions/52533396/cmake-cant-find-protobuf-protobuf-generate-cpp?rq=3 \
[Google gRPC example](https://github.com/protocolbuffers/protobuf/blob/main/docs/cmake_protobuf_generate.md#grpc-example)

[protobuf_generate example](https://stackoverflow.com/questions/52533396/cmake-cant-find-protobuf-protobuf-generate-cpp?rq=3)
[another protobuf_generate example](https://github.com/protocolbuffers/protobuf/blob/main/docs/cmake_protobuf_generate.md#grpc-example)

[bazel](https://grpc.io/blog/bazel-rules-protobuf/)

## Notes

### C++

Not that many functions immediately in libpurple plugin instantiation:

```cpp
	steam_list_icon,           /* list_icon */
	steam_list_emblem,         /* list_emblems */
	steam_status_text,         /* status_text */
	steam_tooltip_text,        /* tooltip_text */
	steam_status_types,        /* status_types */
	steam_node_menu,           /* blist_node_menu */
	NULL,//steam_chat_info,           /* chat_info */
	NULL,//steam_chat_info_defaults,  /* chat_info_defaults */
	steam_login,               /* login */
	steam_close,               /* close */
	steam_send_im,             /* send_im */
	steam_send_typing,         /* send_typing */
	NULL,//steam_get_info,            /* get_info */
	steam_set_status,          /* set_status */
	steam_set_idle,            /* set_idle */
	steam_add_buddy,           /* add_buddy */
	steam_buddy_remove,        /* remove_buddy */
	NULL,//steam_fake_join_chat,      /* join_chat */
	NULL,//steam_get_chat_name,       /* get_chat_name */
	NULL,//steam_chat_fake_leave,     /* chat_leave */
	NULL,//steam_chat_send,           /* chat_send */
	steam_fake_group_buddy,    /* group_buddy */
	steam_fake_group_rename,   /* rename_group */
	steam_buddy_free,          /* buddy_free */
	NULL,//steam_conversation_closed, /* convo_closed */
	NULL,//steam_group_remove,        /* remove_group */
	NULL, // steam_get_account_text_table, /* get_account_text_table */
	STEAM_PLUGIN_ID,				/* id */
	"Steam", 					/* name */
	STEAM_PLUGIN_VERSION, 			/* version */
	N_("Steam Protocol Plugin"), 		/* summary */
	N_("Steam Protocol Plugin"), 		/* description */
	"http://pidgin-opensteamworks.googlecode.com/",	/* homepage */
	steam_actions, 					/* actions */
```

Many functions to implement in `libsteam.c`:

```vi
%s/\nsteam/ SADsteam/g
v/SADsteam/d
%s/SAD//
```

```cpp
static const gchar * steam_account_get_access_token(SteamAccount *sa) {
static void steam_account_set_access_token(SteamAccount *sa, const gchar *access_token) {
static const gchar * steam_personastate_to_statustype(gint64 state)
static const gchar * steam_accountid_to_steamid(gint64 accountid)
static const gchar * steam_steamid_to_accountid(const gchar *steamid)
static void steam_friend_action(SteamAccount *sa, const gchar *who, const gchar *action)
static void steam_friend_invite_action(SteamAccount *sa, const gchar *who, const gchar *action)
static void steam_register_game_key_text(SteamAccount *sa, const gchar *game_key)
static void steam_register_game_key(PurplePluginAction *action)
static void steam_fetch_new_sessionid_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
static void steam_fetch_new_sessionid(SteamAccount *sa)
static void steam_captcha_cancel_cb(PurpleConnection *pc, PurpleRequestFields *fields)
static void steam_captcha_ok_cb(PurpleConnection *pc, PurpleRequestFields *fields)
static void steam_captcha_image_cb(PurpleUtilFetchUrlData *url_data, gpointer userdata, const gchar *response, gsize len, const gchar *error_message)
static void steam_get_icon_cb(PurpleUtilFetchUrlData *url_data, gpointer user_data, const gchar *url_text, gsize len, const gchar *error_message)
static void steam_get_icon_now(PurpleBuddy *buddy)
static gboolean steam_get_icon_queuepop(gpointer data)
static void steam_get_icon(PurpleBuddy *buddy)
static void steam_auth_accept_cb(gpointer user_data)
static void steam_auth_reject_cb(gpointer user_data)
void steam_search_results_add_buddy(PurpleConnection *pc, GList *row, void *user_data)
void steam_search_display_results(SteamAccount *sa, JsonObject *obj, gpointer user_data)
void steam_search_users_text_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
void steam_search_users_text(gpointer user_data, const gchar *text)
void steam_search_users(PurplePluginAction *action)
static void steam_get_friend_summaries_internal(SteamAccount *sa, const gchar *who, SteamProxyCallbackFunc callback_func, gpointer user_data)
static void steam_got_friend_state(SteamAccount *sa, JsonObject *obj, gpointer user_data)
static void steam_get_friend_state(SteamAccount *sa, const gchar *who)
static void steam_request_add_user(SteamAccount *sa, JsonObject *obj, gpointer user_data)
static void steam_poll_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
static void steam_poll(SteamAccount *sa, gboolean secure, guint message)
static void steam_got_friend_summaries(SteamAccount *sa, JsonObject *obj, gpointer user_data)
static void steam_get_friend_summaries(SteamAccount *sa, const gchar *who)
static void steam_get_nickname_list_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
static void steam_get_friend_list_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
static void steam_get_friend_list(SteamAccount *sa)
static void steam_get_offline_history_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
static void steam_get_offline_history(SteamAccount *sa, const gchar *who, gint since)
static void steam_get_conversations_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
static void steam_get_conversations(SteamAccount *sa) {
void steam_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
const gchar * steam_list_emblem(PurpleBuddy *buddy)
GList * steam_status_types(PurpleAccount *account)
static void steam_login_access_token_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
static void steam_login_with_access_token_error_cb(SteamAccount *sa, const gchar *data, gssize data_len, gpointer user_data)
static void steam_login_with_access_token(SteamAccount *sa)
static void steam_set_steam_guard_token_cb(gpointer data, const gchar *steam_guard_token)
static void steam_set_two_factor_auth_code_cb(gpointer data, const gchar *twofactorcode)
static void steam_login_cb(SteamAccount *sa, JsonObject *obj, gpointer user_data)
static void steam_login_got_rsakey(SteamAccount *sa, JsonObject *obj, gpointer user_data)
static void steam_get_rsa_key(SteamAccount *sa)

#ifdef USE_GNOME_KEYRING
static void steam_keyring_got_password(GnomeKeyringResult res, const gchar* access_token, gpointer user_data) {
#else
static void steam_keyring_got_password(GObject *source_object, GAsyncResult *res, gpointer user_data) {
#endif

static void steam_login(PurpleAccount *account)
static unsigned int steam_send_typing(PurpleConnection *pc, const gchar *name, PurpleTypingState state)
static void steam_set_status(PurpleAccount *account, PurpleStatus *status)
static void steam_set_idle(PurpleConnection *pc, int time)
void steam_fake_group_buddy(PurpleConnection *pc, const char *who, const char *old_group, const char *new_group)
void steam_fake_group_rename(PurpleConnection *pc, const char *old_name, PurpleGroup *group, GList *moved_buddies)
#if PURPLE_VERSION_CHECK(3, 0, 0) steam_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group, const char* message)
#else steam_add_buddy(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
void steam_buddy_remove(PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group)
void steam_blist_launch_game(PurpleBlistNode *node, gpointer data)
void steam_blist_join_game(PurpleBlistNode *node, gpointer data)
void steam_blist_view_profile(PurpleBlistNode *node, gpointer data)
static GList * steam_node_menu(PurpleBlistNode *node)
```

### Node

```
logOn([details])

	details - An object containing details for this logon
		anonymous - Pass true if you want to log into an anonymous account, omit or pass false if not
		refreshToken - A refresh token, see below
		accountName - If logging into a user account, the account's name
		password - If logging into an account without a login key or a web logon token, the account's password
		loginKey - If logging into an account with a login key, this is the account's login key [DEPRECATED]
		webLogonToken - If logging into an account with a client logon token obtained from the web, this is the token
		steamID - If logging into an account with a client logon token obtained from the web, this is your account's SteamID, as a string or a SteamID object
		authCode - If you have a Steam Guard email code, you can provide it here. You might not need to, see the steamGuard event. (Added in 1.9.0)
		twoFactorCode - If you have a Steam Guard mobile two-factor authentication code, you can provide it here. You might not need to, see the steamGuard event. (Added in 1.9.0)
		rememberPassword - true if you want to get a login key which can be used in lieu of a password for subsequent logins. false or omitted otherwise.
		logonID - A 32-bit integer to identify this login. The official Steam client derives this from your machine's private IP (it's the obfuscated_private_ip field in CMsgClientLogOn). If you try to logon twice to the same account from the same public IP with the same logonID, the first session will be kicked with reason SteamUser.EResult.LogonSessionReplaced. Defaults to 0 if not specified.
			As of v4.13.0, this can also be an IPv4 address as a string, in dotted-decimal notation (e.g. "192.168.1.5")
		machineName - A string containing the name of this machine that you want to report to Steam. This will be displayed on steamcommunity.com when you view your games list (when logged in).
		clientOS - A number to identify your client OS. Auto-detected if you don't provide one.
		dontRememberMachine - If you're providing an authCode but you don't want Steam to remember this sentryfile, pass true here.
```

```
reader.readCredentials().then((credentials) => {
	// Log in with the provided username and password
	client.logOn({
		"accountName": credentials.username,
		"password": credentials.password
	});
});
```