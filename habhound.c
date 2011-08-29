/* habhound - High Altitude Balloon tracking                              */
/*======================================================================= */
/* Copyright 2011 Philip Heron <phil@sanslogic.co.uk>                     */
/*                                                                        */
/* This program is free software: you can redistribute it and/or modify   */
/* it under the terms of the GNU General Public License as published by   */
/* the Free Software Foundation, either version 3 of the License, or      */
/* (at your option) any later version.                                    */
/*                                                                        */
/* This program is distributed in the hope that it will be useful,        */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of         */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the           */
/* GNU General Public License for more details.                           */
/*                                                                        */
/* You should have received a copy of the GNU General Public License      */
/* along with this program. If not, see <http://www.gnu.org/licenses/>.   */

#include <glib.h>
#include <gtk/gtk.h>
#include <osm-gps-map.h>
#include <gdk/gdkkeysyms.h>
#include "hab_layer.h"

static OsmGpsMapImage *g_last_image = NULL;
static GdkPixbuf *g_balloon_blue = NULL;

static OsmGpsMapTrack *balloon_track = NULL;

static gboolean key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	switch(event->keyval)
	{
	case 'q':
	case 'Q':
		gtk_main_quit();
		return(TRUE);
	}
	
	return(FALSE);
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	return(FALSE);
}

static void destroy(GtkWidget *widget, gpointer data)
{
	gtk_main_quit();
}

int load_test_track()
{
	FILE *f;
	char s[100];
	OsmGpsMapPoint coord;
	float lat, lng;
	
	f = fopen("xaben1-track.txt", "rt");
	while(fgets(s, 100, f))
	{
		sscanf(s, "%f,%f", &lat, &lng);
		osm_gps_map_point_set_degrees(&coord, lat, lng);
		osm_gps_map_track_add_point(balloon_track, &coord);
	}
	fclose(f);
	
	return(0);
}

int main(int argc, char *argv[])
{
	GtkWidget *mainwin;
	OsmGpsMap *map;
	OsmGpsMapLayer *osd;
	
	gtk_init(&argc, &argv);
	
	/* Create the main window */
	mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(mainwin), "habhound - High Altitude Balloon tracking");
	gtk_window_set_default_size(GTK_WINDOW(mainwin), 600, 400);
	gtk_window_set_position(GTK_WINDOW(mainwin), GTK_WIN_POS_CENTER);
	g_signal_connect(mainwin, "delete-event", G_CALLBACK(delete_event), NULL);
	g_signal_connect(mainwin, "destroy", G_CALLBACK(destroy), NULL);
	
	/* Create the osm-gps-map control */
	map = g_object_new(OSM_TYPE_GPS_MAP,
		"map-source", OSM_GPS_MAP_SOURCE_OPENSTREETMAP,
		//"map-source", OSM_GPS_MAP_SOURCE_GOOGLE_STREET,
		//"tile-cache",cachedir,
		//"tile-cache-base", cachebasedir,
		//"proxy-uri",g_getenv("http_proxy"),
		NULL);
	gtk_container_add(GTK_CONTAINER(mainwin), GTK_WIDGET(map));
	gtk_widget_show(GTK_WIDGET(map));
	
	/* Setup key binding */
	osm_gps_map_set_keyboard_shortcut(map, OSM_GPS_MAP_KEY_FULLSCREEN, GDK_F11);
	osm_gps_map_set_keyboard_shortcut(map, OSM_GPS_MAP_KEY_UP, GDK_Up);
	osm_gps_map_set_keyboard_shortcut(map, OSM_GPS_MAP_KEY_DOWN, GDK_Down);
	osm_gps_map_set_keyboard_shortcut(map, OSM_GPS_MAP_KEY_LEFT, GDK_Left);
	osm_gps_map_set_keyboard_shortcut(map, OSM_GPS_MAP_KEY_RIGHT, GDK_Right);
	
	/* Setup key press event */
	g_signal_connect(mainwin, "key-press-event", G_CALLBACK(key_press_event), NULL);
	
	/* Setup the map OSD */
	osd = g_object_new(OSM_TYPE_GPS_MAP_OSD,
		"show-scale", TRUE,
		"show-coordinates", FALSE,
		"show-crosshair", FALSE,
		"show-dpad", TRUE,
		"show-zoom", TRUE,
		"show-gps-in-dpad", FALSE,
		"show-gps-in-zoom", FALSE,
		"dpad-radius", 20,
		NULL);
	osm_gps_map_layer_add(map, osd);
	g_object_unref(G_OBJECT(osd));
	
	/* Setup the HAB OSD */
	osd = g_object_new(HAB_LAYER_TYPE, NULL);
	osm_gps_map_layer_add(map, osd);
	g_object_unref(G_OBJECT(osd));
	
	/* Plot a point on the map */
	g_balloon_blue = gdk_pixbuf_new_from_file("icons/balloon-blue.png", NULL);
	g_last_image = osm_gps_map_image_add_with_alignment(
		map, 52.23715, 1.22074, g_balloon_blue, 0.5, 0.9);
	
	/* Add in a test track */
	balloon_track = osm_gps_map_track_new();
	osm_gps_map_track_add(map, balloon_track);
	
	/* Load track */
	load_test_track();
	
	/* Finally show the window */
	gtk_widget_show(mainwin);
	
	gtk_main();
	
	return(0);
}

