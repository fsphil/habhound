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
#include <curl/curl.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "habhound.h"
#include "hab_layer.h"
#include "habitat.h"

static OsmGpsMap *map = NULL;

static GdkPixbuf *g_balloon_blue = NULL;
static GdkPixbuf *g_radio_green = NULL;
static GdkPixbuf *g_car_red = NULL;

typedef struct {
	hab_object_type_t type;
	const char *callsign;
	
	GdkPixbuf *image;
	double x_offset;
	double y_offset;
	
	OsmGpsMapImage *icon;
	OsmGpsMapTrack *track;
	
	OsmGpsMapTrack *horizon; /* Only for balloons at the moment */
	
	double latitude;
	double longitude;
	double altitude;
	
	time_t timestamp; /* Time last updated */
} map_object_t;

static int map_objects_count = 0;
static map_object_t **map_objects = NULL;

typedef struct {
	char *callsign;
	hab_object_type_t type;
	double latitude;
	double longitude;
	double altitude;
} obj_data_t;

static map_object_t *find_map_object(hab_object_type_t type, const char *callsign)
{
	int i;
	map_object_t *obj;
	
	/* No objects yet */
	if(!map_objects) return(NULL);
	
	/* Loop until a match is found, or the end is reached */
	for(i = 0; map_objects[i]; i++)
	{
		obj = map_objects[i];
		if(obj->type == type &&
		   strcmp(obj->callsign, callsign) == 0) return(obj);
	}
	
	/* No match was found */
	return(NULL);
}

static map_object_t *get_map_object(int index)
{
	/* Return a map object by its index number */
	if(index >= map_objects_count) return(NULL);
	return(map_objects[index]);
}

static int new_map_object(map_object_t *obj)
{
	/* Reallocate the length of the array */
	void *t = realloc(map_objects, sizeof(map_object_t *) * (map_objects_count + 2));
	if(!t) return(-1); /* Out of memory! */
	
	/* Add the new item to the end */
	map_objects = t;
	map_objects[map_objects_count++] = obj;
	map_objects[map_objects_count] = NULL;
	
	return(map_objects_count);
}

/* horizon calculations */
float calculate_distance_to_horizon(float altitude)
{
	return(sqrt(2 * 6378137.0 * altitude));
}

OsmGpsMapPoint *calculate_point_at_horizon(OsmGpsMapPoint *dst, OsmGpsMapPoint *src, float bearing, float distance)
{
	float lat1, lng1;
	float lat2, lng2;
	float d = distance / 6378137.0; /* Average radius of the earth */
	
	osm_gps_map_point_get_radians(src, &lat1, &lng1);
	
	/* Formula from http://www.movable-type.co.uk/scripts/latlong.html */
	
	lat2 = asin(sin(lat1) * cos(d) + cos(lat1) * sin(d) * cos(bearing));
	lng2 = lng1 + atan2(sin(bearing) * sin(d) * cos(lat1), cos(d) - sin(lat1) * sin(lat2));
	
	osm_gps_map_point_set_radians(dst, lat2, lng2);
	
	return(dst);
}

const char *habhound_object_type_name(hab_object_type_t type)
{
	switch(type)
	{
	case HAB_PAYLOAD: return("payload");
	case HAB_LISTENER: return("listener");
	case HAB_CHASE: return("chase");
	}
	
	return("unknown");
}

static gboolean cb_habhound_plot_object(obj_data_t *data)
{
	map_object_t *obj;
	OsmGpsMapPoint coord;
	
	fprintf(stderr, "%s %s at %f,%f altitude %.2f\n",
		habhound_object_type_name(data->type), data->callsign,
		data->latitude, data->longitude, data->altitude);
	
	/* Ignore 0,0 coordinates */
	if(data->latitude == 0 && data->longitude == 0)
	{
		free(data->callsign);
		free(data);
		return(FALSE);
	}
	
	/* Is this a known object? */
	obj = find_map_object(data->type, data->callsign);
	if(!obj)
	{
		obj = calloc(sizeof(map_object_t), 1);
		if(!obj)
		{
			free(data->callsign);
			free(data);
			return(FALSE); /* Out of memory! */
		}
		
		/* Add the new object to the objects array */
		if(new_map_object(obj) == -1)
		{
			/* Failed to add! */
			free(obj);
			free(data->callsign);
			free(data);
			return(FALSE);
		}
		
		obj->type = data->type;
		obj->callsign = data->callsign;
		switch(obj->type)
		{
		case HAB_PAYLOAD:
			obj->image = g_balloon_blue;
			obj->x_offset = 0.5;
			obj->y_offset = 0.95;
			obj->track = osm_gps_map_track_new();
			osm_gps_map_track_add(map, obj->track);
			break;
		case HAB_LISTENER:
			obj->image = g_radio_green;
			obj->x_offset = 0.5;
			obj->y_offset = 0.9;
			obj->track = NULL;
			break;
		case HAB_CHASE:
			obj->image = g_car_red;
			obj->x_offset = 0.5;
			obj->y_offset = 0.5;
			obj->track = NULL;
			break;
		}
		
		obj->horizon = NULL;
		
		obj->icon = osm_gps_map_image_add_with_alignment(
			map, data->latitude, data->longitude, obj->image,
			obj->x_offset, obj->y_offset);
	}
	else
	{
		free(data->callsign); /* Don't need this */
		
		/* Has the data changed from the last time? */
		if((obj->latitude == data->latitude) ||
		   (obj->longitude == data->longitude) ||
		   (obj->altitude  == data->altitude))
		{
			/* Nothing has changed, ignore data */
			free(data);
			return(FALSE);
		}
	}
	
	obj->latitude  = data->latitude;
	obj->longitude = data->longitude;
	obj->altitude  = data->altitude;
	
	if(strcmp(obj->callsign, "2I0VIM") == 0) obj->altitude = 80.0;
	
	osm_gps_map_point_set_degrees(&coord, obj->latitude, obj->longitude);
	g_object_set(G_OBJECT(obj->icon), "point", &coord, NULL);
	if(obj->track) osm_gps_map_track_add_point(obj->track, &coord);
	
	/* Draw payload horizon circle */
	if(obj->type == HAB_PAYLOAD || obj->altitude > 0)
	{
		OsmGpsMapPoint p;
		int i;
		float b, s;
		float d; /* 1km */
		GdkColor c;
		
		d = calculate_distance_to_horizon(obj->altitude);
		
		if(obj->horizon)
		{
			osm_gps_map_track_remove(map, obj->horizon);
			g_object_unref(G_OBJECT(obj->horizon));
		}
		obj->horizon = osm_gps_map_track_new();
		
		for(i = 0; i <= 100; i++)
		{
			b = M_PI * 2.0 / 100.0 * (float) i;
			calculate_point_at_horizon(&p, &coord, b, d);
			osm_gps_map_track_add_point(obj->horizon, &p);
		}
		
		gdk_color_parse("#0000FF", &c);
		d = 0.5;
		s = 2.0;
		g_object_set(G_OBJECT(obj->horizon),
			"alpha", d,
			"color", &c,
			"line-width", s,
			NULL);
		
		osm_gps_map_track_add(map, obj->horizon);
	}
	else if(obj->type == HAB_PAYLOAD || obj->altitude <= 0)
	{
		/* Remove horizon if payload is on the ground */
		if(obj->horizon)
		{
			osm_gps_map_track_remove(map, obj->horizon);
			g_object_unref(G_OBJECT(obj->horizon));
			obj->horizon = NULL;
		}
	}
	
	free(data);
	
	return(FALSE);
}

void habhound_plot_object(const char *callsign, hab_object_type_t type,
	double latitude, double longitude, double altitude)
{
	obj_data_t *obj = calloc(sizeof(obj_data_t), 1);
	if(!obj) return;
	
	obj->callsign  = strdup(callsign);
	obj->type      = type;
	obj->latitude  = latitude;
	obj->longitude = longitude;
	obj->altitude  = altitude;
	
	g_idle_add((GSourceFunc) cb_habhound_plot_object, obj);
}

void habhound_delete_object(const char *callsign)
{
	return;
}

/* internal */

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

int main(int argc, char *argv[])
{
	GtkWidget *mainwin;
	//OsmGpsMap *map;
	OsmGpsMapLayer *osd;
	src_habitat_t *src_habitat;
	
	/* Initialise libraries */
	curl_global_init(CURL_GLOBAL_ALL);
	
	g_thread_init(NULL);
	gdk_threads_init();
	gdk_threads_enter();
 	
	gtk_init(&argc, &argv);
	
	/* Create the main window */
	mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(mainwin), "habhound - High Altitude Balloon tracking");
	gtk_window_set_default_size(GTK_WINDOW(mainwin), 600, 600);
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
	
	/* Setup initial map view, center of the British Isles */
	/* This will eventually be configurable */
	osm_gps_map_set_center_and_zoom(map, 54.5, -4.5, 5);
	
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
	
	/* Setup the HAB OSD -- doesn't work yet :) */
	//osd = g_object_new(HAB_LAYER_TYPE,
	//	"callsign", "APEX",
	//	NULL);
	//osm_gps_map_layer_add(map, osd);
	//g_object_unref(G_OBJECT(osd));
	
	/* Plot a point on the map */
	g_balloon_blue = gdk_pixbuf_new_from_file("icons/balloon-blue.png", NULL);
	g_radio_green  = gdk_pixbuf_new_from_file("icons/antenna-green.png", NULL);
	g_car_red      = gdk_pixbuf_new_from_file("icons/car-red.png", NULL);
	
	/* Start the habitat handler */
	src_habitat = src_habitat_start("http://habitat.habhub.org/habitat");
	
	/* Finally show the lot */
	gtk_widget_show(mainwin);
	
	gtk_main();
	
	/* Stop the habitat handler */
	src_habitat_stop(src_habitat);
	
	/* Done */
	gdk_threads_leave();
	
	return(0);
}

