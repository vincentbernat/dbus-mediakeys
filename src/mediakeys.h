/* -*- mode: c; c-file-style: "openbsd" -*- */
/*
 * Copyright (c) 2015 Vincent Bernat <bernat@luffy.cx>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BOOTSTRAP_H
#define _BOOTSTRAP_H

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

/* player.c */
struct player;
const gchar *player_get_application(struct player *);
const gchar *player_get_dbus_name(struct player *);
guint32 player_get_time(struct player *);
gint player_find_by_name(gconstpointer, gconstpointer);
gint player_find_by_application(gconstpointer, gconstpointer);
gint player_find_by_time(gconstpointer, gconstpointer);
void player_free(struct player *);
struct player *player_new(const gchar *, const gchar *, guint32, guint);

#endif
