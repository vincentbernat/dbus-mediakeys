/* -*- mode: c; c-file-style: "openbsd" -*- */
/*
 * Copyright (C) 2001-2003 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2006-2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (c) 2015 Vincent Bernat <bernat@luffy.cx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "mediakeys.h"

#include <stdio.h>
#include <unistd.h>
#include <gio/gio.h>
#include <glib-unix.h>

static gboolean debug = FALSE;

static const gchar dbus_bus_name[] = "org.gnome.SettingsDaemon.MediaKeys";
static const gchar dbus_object_path[] = "/org/gnome/SettingsDaemon/MediaKeys";
static const gchar dbus_interface[] = "org.gnome.SettingsDaemon.MediaKeys";

static const gchar introspection_xml[] =
    "<node name='/org/gnome/SettingsDaemon/MediaKeys'>"
    "  <interface name='org.gnome.SettingsDaemon.MediaKeys'>"
    "	 <method name='GrabMediaPlayerKeys'>"
    "	   <arg name='application' direction='in' type='s'/>"
    "	   <arg name='time' direction='in' type='u'/>"
    "	 </method>"
    "	 <method name='ReleaseMediaPlayerKeys'>"
    "	   <arg name='application' direction='in' type='s'/>"
    "	 </method>"
    "	 <signal name='MediaPlayerKeyPressed'>"
    "	   <arg name='application' type='s'/>"
    "	   <arg name='key' type='s'/>"
    "	 </signal>"
    "	 <!-- Not part of the \"spec\", but useful to interact with us -->"
    "	 <method name='PressMediaKey'>"
    "	   <arg name='key' direction='in' type='s'/>"
    "	 </method>"
    "  </interface>"
    "</node>";

struct cfg {
	GDBusNodeInfo *introspection_data;
	GMainLoop *loop;
	GList *players;
	GDBusConnection *connection;
};

static gboolean
on_term_signal(gpointer user_data)
{
	struct cfg *cfg = user_data;
        g_debug ("Received SIGTERM - shutting down");
        g_main_loop_quit(cfg->loop);

        return FALSE;
}

static void
name_vanished_handler(GDBusConnection *connection,
    const gchar *name,
    gpointer user_data)
{
	struct cfg *cfg = user_data;
	GList *iter;

	iter = g_list_find_custom(cfg->players,
	    name,
	    player_find_by_name);

	if (iter != NULL) {
		struct player *player;
		player = iter->data;
		g_debug("Deregistering vanished %s (dbus_name: %s)",
		    player_get_application(player),
		    player_get_dbus_name(player));
		player_free(player);
		cfg->players = g_list_delete_link(cfg->players, iter);
	}
}

static void
grab_media_player_keys(struct cfg *cfg, const gchar *application,
    const gchar *dbus_name, guint32 time)
{
	GList *iter;
	struct player *player;
	guint watch_id;

	if (time == 0L) {
		GTimeVal tv;
		g_get_current_time(&tv);
		time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	}

	iter = g_list_find_custom(cfg->players,
	    application,
	    player_find_by_application);

	if (iter != NULL) {
		if (player_get_time(iter->data) < time) {
			struct player *player = iter->data;
			player_free(player);
			cfg->players = g_list_delete_link(cfg->players, iter);
		} else {
			return;
		}
	}

	watch_id = g_bus_watch_name(G_BUS_TYPE_SESSION,
	    dbus_name,
	    G_BUS_NAME_WATCHER_FLAGS_NONE,
	    NULL,
	    (GBusNameVanishedCallback)name_vanished_handler,
	    cfg,
	    NULL);

	g_debug("Registering %s at %u", application, time);
	player = player_new(application, dbus_name, time, watch_id);

	cfg->players = g_list_insert_sorted(cfg->players,
	    player,
	    player_find_by_time);
}

static void
release_media_player_keys(struct cfg *cfg,
    const gchar *application,
    const gchar *name)
{
	GList *iter = NULL;

	g_return_if_fail(application != NULL || name != NULL);

	if (application != NULL) {
		iter = g_list_find_custom(cfg->players,
		    application,
		    player_find_by_application);
	}

	if (iter == NULL && name != NULL) {
		iter = g_list_find_custom(cfg->players,
		    name,
		    player_find_by_name);
	}

	if (iter != NULL) {
		struct player *player;

		player = iter->data;
		g_debug("Deregistering %s (dbus_name: %s)",
		    application, player_get_dbus_name(player));
		player_free(player);
		cfg->players = g_list_delete_link(cfg->players, iter);
	}
}


static void
press_media_key(struct cfg *cfg,
    const gchar *key, const gchar *sender)
{
	const char *application;
	gboolean have_listeners;
	GError *error = NULL;
	struct player *player;

	if (key == NULL) return;

	have_listeners = (cfg->players != NULL);
	if (!have_listeners) {
		g_debug("Media key '%s' pressed but no listeners", key);
		return;
	}

	player = cfg->players->data; /* Get the first application */
	application = player_get_application(player);

	g_debug("Media key '%s' pressed and sent to %s", key, application);

	if (g_dbus_connection_emit_signal(cfg->connection,
		player_get_dbus_name(player),
		dbus_object_path,
		dbus_interface,
		"MediaPlayerKeyPressed",
		g_variant_new("(ss)", application ? application : "", key),
		&error) == FALSE) {
		g_debug("Error emitting signal: %s", error->message);
		g_error_free(error);
	}

	return;
}

static void
handle_method_call(GDBusConnection *connection,
    const gchar *sender,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *method_name,
    GVariant *parameters,
    GDBusMethodInvocation *invocation,
    gpointer user_data)
{
	struct cfg *cfg = user_data;
	g_debug("Calling method '%s' for media-keys", method_name);

	if (g_strcmp0(method_name, "ReleaseMediaPlayerKeys") == 0) {
		const gchar *app_name;

		g_variant_get(parameters, "(&s)", &app_name);
		release_media_player_keys(cfg, app_name, sender);
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (g_strcmp0(method_name, "GrabMediaPlayerKeys") == 0) {
		const gchar *app_name;
		guint32 time;

		g_variant_get(parameters, "(&su)", &app_name, &time);
		grab_media_player_keys(cfg, app_name, sender, time);
		g_dbus_method_invocation_return_value(invocation, NULL);
	} else if (g_strcmp0(method_name, "PressMediaKey") == 0) {
		const gchar *key;

		g_variant_get(parameters, "(&s)", &key);
		press_media_key(cfg, key, sender);
		g_dbus_method_invocation_return_value(invocation, NULL);
	}
}

static const GDBusInterfaceVTable interface_vtable = {
	handle_method_call,
	NULL,
	NULL
};

static void
on_bus_acquired(GDBusConnection *connection,
    const gchar *name,
    gpointer user_data)
{
	struct cfg *cfg = user_data;
	g_dbus_connection_register_object(connection,
	    dbus_object_path,
	    cfg->introspection_data->interfaces[0],
	    &interface_vtable,
	    user_data,
	    NULL,
	    NULL);
}

static void
on_name_acquired(GDBusConnection *connection,
    const gchar *name,
    gpointer user_data)
{
	struct cfg *cfg = user_data;
	g_debug("DBus name %s acquired", name);
	cfg->connection = connection;
}

static void
on_name_lost(GDBusConnection *connection,
    const gchar *name,
    gpointer user_data)
{
	struct cfg *cfg = user_data;
	g_warning("Connection to %s lost", name);
	g_main_loop_quit(cfg->loop);
}

static void
log_default_handler(const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *message,
    gpointer unused_data)
{
	if ((log_level & G_LOG_LEVEL_MASK) == G_LOG_LEVEL_DEBUG &&
	    !debug) return;

	g_log_default_handler(log_domain, log_level,
	    message,
	    unused_data);
}

int
main(int argc, char *argv[])
{
	struct cfg cfg = {};
	guint owner_id;

	GError *error = NULL;
	GOptionContext *context;
	GOptionEntry entries[] = {
		{ "debug", 0, 0, G_OPTION_ARG_NONE, &debug, "Enable debugging code", NULL},
		{ NULL }
	};

	context = g_option_context_new("- media keys daemon");
	g_option_context_add_main_entries(context, entries, NULL);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_print ("option parsing failed: %s\n", error->message);
		return 1;
	}

	g_log_set_default_handler(log_default_handler, NULL);
	if (debug) g_setenv ("G_MESSAGES_DEBUG", "all", FALSE);
	g_debug("Starting...");

	cfg.introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);
	g_assert(cfg.introspection_data != NULL);
	cfg.players = NULL;

	g_unix_signal_add(SIGTERM, on_term_signal, &cfg);
	g_unix_signal_add(SIGINT, on_term_signal, &cfg);

	owner_id = g_bus_own_name(G_BUS_TYPE_SESSION,
	    dbus_bus_name,
	    G_BUS_NAME_OWNER_FLAGS_NONE,
	    on_bus_acquired,
	    on_name_acquired,
	    on_name_lost,
	    &cfg,
	    NULL);
	g_assert(owner_id > 0);

	cfg.loop = g_main_loop_new(NULL, FALSE);
	g_debug("Start main loop...");
	g_main_loop_run(cfg.loop);
	g_debug("End of main loop");

	g_option_context_free(context);
	g_bus_unown_name(owner_id);
	g_main_loop_unref(cfg.loop);
	g_list_free_full(cfg.players, (GDestroyNotify)player_free);
	g_dbus_node_info_unref(cfg.introspection_data);

	return 0;
}
