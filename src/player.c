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

#include <gio/gio.h>

/* Player related structs */
struct player {
	gchar *application;
	gchar *dbus_name;
	guint32 time;
	guint watch_id;
};

const gchar *
player_get_application(struct player *player)
{
	return player->application;
}

const gchar *
player_get_dbus_name(struct player *player)
{
	return player->dbus_name;
}

guint32
player_get_time(struct player *player)
{
	return player->time;
}

gint
player_find_by_application(gconstpointer a,
    gconstpointer b)
{
	return g_strcmp0(((struct player *)a)->application, b);
}

gint
player_find_by_name(gconstpointer a,
    gconstpointer b)
{
	return g_strcmp0(((struct player *)a)->dbus_name, b);
}


gint
player_find_by_time(gconstpointer a,
    gconstpointer b)
{
	return ((struct player *)a)->time < ((struct player *)b)->time;
}

void
player_free(struct player *player)
{
	if (player->watch_id > 0)
		g_bus_unwatch_name(player->watch_id);
	g_free(player->application);
	g_free(player->dbus_name);
	g_free(player);
}

struct player *
player_new(const gchar *application,
    const gchar *dbus_name,
    guint32 time,
    guint watch_id)
{
	struct player *player = g_new0(struct player, 1);
	player->application = g_strdup(application);
	player->dbus_name = g_strdup(dbus_name);
	player->time = time;
	player->watch_id = watch_id;
	return player;
}
