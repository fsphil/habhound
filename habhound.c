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
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "habhound.h"
#include "hab_layer.h"
#include "habitat.h"

static OsmGpsMap *map = NULL;
static OsmGpsMapLayer *osd = NULL;

static GdkPixbuf *g_balloon_blue = NULL;
static GdkPixbuf *g_radio_green = NULL;
static GdkPixbuf *g_car_red = NULL;

typedef struct {
	hab_object_type_t type;
	const char *callsign;
	
	GdkPixbuf *image;
	GdkPixbuf *mapimage;
	double x_offset;
	double y_offset;
	
	OsmGpsMapImage *icon;
	OsmGpsMapTrack *track;
	
	OsmGpsMapTrack *horizon; /* Only for balloons at the moment */
	
	cairo_surface_t *infobox; /* Also only for balloons */
	
	time_t timestamp;
	double latitude;
	double longitude;
	double altitude;
	
	double max_altitude;
} map_object_t;

static int map_objects_count = 0;
static map_object_t **map_objects = NULL;

typedef struct {
	char *callsign;
	hab_object_type_t type;
	time_t timestamp;
	double latitude;
	double longitude;
	double altitude;
} obj_data_t;

/* Taken from the GCC manual and cleaned up a bit. */
char *vmake_message(const char *fmt, va_list ap)
{
	/* Start with 100 bytes. */
	int n, size = 100;
	char *p;
	
	p = (char *) malloc(size);
	if(!p) return(NULL);
	
	while(1)
	{
		char *np;
		va_list apc;
		
		/* Try to print in the allocated space. */
		va_copy(apc, ap);
		n = vsnprintf(p, size, fmt, apc);
		
		/* If that worked, return the string. */
		if(n > -1 && n < size) return(p);
		
		/* Else try again with more space. */
		if(n > -1) size = n + 1; /* gibc 2.1: exactly what is needed */
		else size *= 2; /* glibc 2.0: twice the old size */
		
		np = (char *) realloc(p, size);
		if(!np)
		{
			free(p);
			return(NULL);
		}
		
		p = np;
	}
	
	free(p);
	return(NULL);
}

char *sprintf_alloc(const char *format, ... )
{
	va_list ap;
	char *msg;
	
	va_start(ap, format);
	msg = vmake_message(format, ap);
	va_end(ap);
	
	return(msg);
}

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

/* Create a GdkPixbuf from a cairo surface -- based on convert_alpha() from aprsmap */
static GdkPixbuf *_gdk_pixbuf_new_from_surface(cairo_surface_t *surface)
{
	GdkPixbuf *pixbuf;
	unsigned char *dst_data, *src_data;
	int dst_stride, src_stride;
	int width, height;
	int x, y;
	
	/* Get the details of the surface */
	width      = cairo_image_surface_get_width(surface);
	height     = cairo_image_surface_get_height(surface);
	src_data   = cairo_image_surface_get_data(surface);
	src_stride = cairo_image_surface_get_stride(surface);
	
	/* Create the new pixbuf */
	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
	if(!pixbuf) return(NULL);
	
	/* Get details of the pixbuf */
	dst_data   = gdk_pixbuf_get_pixels(pixbuf);
	dst_stride = gdk_pixbuf_get_rowstride(pixbuf);
	
	/* Flush any pending drawing bits */
	cairo_surface_flush(surface);
	
	/* Copy the image data */
	for(y = 0; y < height; y++)
	{
		uint32_t *src = (uint32_t *) src_data;
		
		for(x = 0; x < width; x++)
		{
			unsigned int alpha = src[x] >> 24;
			
			if(alpha == 0)
			{
				dst_data[x * 4 + 0] = 0;
				dst_data[x * 4 + 1] = 0;
				dst_data[x * 4 + 2] = 0;
			}
			else
			{
				dst_data[x * 4 + 0] = (((src[x] & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
				dst_data[x * 4 + 1] = (((src[x] & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
				dst_data[x * 4 + 2] = (((src[x] & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
			}
			dst_data[x * 4 + 3] = alpha;
		}
		src_data += src_stride;
		dst_data += dst_stride;
	}
	
	return(pixbuf);
}

static void render_mapimage(map_object_t *obj)
{
	cairo_t *cr;
	cairo_surface_t *surface;
	cairo_text_extents_t extent;
	int width, height;
	
	/* Get the width and height of the icon */
	width  = gdk_pixbuf_get_width(obj->image);
	height = gdk_pixbuf_get_height(obj->image);
	
	/* Create a dummy surface, to find the width of the callsign. */
	/* There must be a handier way of doing this! */
	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 16, 16);
	cr = cairo_create(surface);
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 8);
	cairo_text_extents(cr, obj->callsign, &extent);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	
	/* Adjust the size to fit both the icon and text */
	if(extent.width >= width) width = extent.width + 2;
	height += extent.height + 2;
	
	obj->x_offset -= 0.5;
	obj->x_offset *= (double) gdk_pixbuf_get_width(obj->image) / width;
	obj->x_offset += 0.5;
	obj->y_offset -= (double) (extent.height + 2) / height;
	
	/* Create and render the new icon */
	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cr = cairo_create(surface);
	
	/* Draw the outline box */
	//cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	//cairo_set_line_width(cr, 2);
	//cairo_rectangle(cr, 0, 0, width, height);
	//cairo_stroke_preserve(cr);
	
	/* Draw the balloon icon */
	gdk_cairo_set_source_pixbuf(cr, obj->image,
		(width - gdk_pixbuf_get_width(obj->image)) / 2, 0);
	cairo_paint(cr);
	
	/* Render the callsign */
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 8);
	cairo_move_to(cr, (width - extent.width) / 2, height - extent.height / 2 + 2);
	cairo_show_text(cr, obj->callsign);
	
	/* Create the GdkPixbuf from the cairo_surface */
	obj->mapimage = _gdk_pixbuf_new_from_surface(surface);
	
	/* Destroy the surface */
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
}

static void render_infobox(map_object_t *obj)
{
	cairo_t *cr;
	char msg[100];
	
	/* Create the surface */
	if(!obj->infobox) obj->infobox = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 220, 104);
	
	/* first fill with transparency */
	cr = cairo_create(obj->infobox);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	
	/* Draw the outline box */
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 2);
	cairo_rectangle(cr, 1, 1, 218, 102);
	cairo_stroke_preserve(cr);
	
	/* Fill in box with semi-transparent white */
	cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.85);
	cairo_fill(cr);
	
	/* Draw the balloon icon */
	if(obj->image)
	{
		gdk_cairo_set_source_pixbuf(cr, obj->image, 166, 5);
		cairo_paint(cr);
	}
	
	/* Draw the payload title */
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 12);
	
	cairo_move_to(cr, 5, 14 /* - te.height / 2 - te.y_bearing*/);
	cairo_show_text(cr, obj->callsign);
	
	/* Set the font for the rest of the data */
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 10);
	
	/* Print telemetry time */
	cairo_move_to(cr, 5, 14 + 11);
	if(obj->timestamp)
	{
		struct tm tm;
		strftime(msg, 100, "Time: %Y-%m-%d %H:%M:%S", gmtime_r(&obj->timestamp, &tm));
		cairo_show_text(cr, msg);
	}
	
	/* Print position */
	cairo_move_to(cr, 5, 14 + 22);
	snprintf(msg, 100, "Position: %.5f, %.5f", obj->latitude, obj->longitude);
	cairo_show_text(cr, msg);
	
	/* Altitude */
	cairo_move_to(cr, 5, 14 + 33);
	snprintf(msg, 100, "Altitude: %i m", (int) obj->altitude);
	cairo_show_text(cr, msg);
	
	/* Max altitude */
	cairo_move_to(cr, 5, 14 + 44);
	snprintf(msg, 100, "Max. Altitude: %i m", (int) obj->max_altitude);
	cairo_show_text(cr, msg);
	
	cairo_destroy(cr);
}

static gboolean cb_habhound_set_status(char *data)
{
	g_object_set(G_OBJECT(osd), "status", data, NULL);
	osm_gps_map_scroll(map, 0, 0); /* <-- hacky way to re-render map */
	return(FALSE);
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
			obj->y_offset = 1.0;
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
		
		/* Render the map image - icon + callsign */
		render_mapimage(obj);
		
		obj->icon = osm_gps_map_image_add_with_alignment(
			map, data->latitude, data->longitude, obj->mapimage,
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
	
	obj->timestamp = data->timestamp;
	obj->latitude  = data->latitude;
	obj->longitude = data->longitude;
	obj->altitude  = data->altitude;
	if(data->altitude > obj->max_altitude)
		obj->max_altitude = data->altitude;
	
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
	
	/* Render the payload infobox */
	if(obj->type == HAB_PAYLOAD) render_infobox(obj);
	
	free(data);
	
	return(FALSE);
}

/* Set the status message */
void habhound_set_status(char *message, ... )
{
	va_list ap;
	char *s;
	
	if(!message) return;
	
	/* Create the full URL */
	va_start(ap, message);
	s = vmake_message(message, ap);
	va_end(ap);
	
	if(!s) return;
	
	g_idle_add((GSourceFunc) cb_habhound_set_status, s);
}

/* Get a pointer to a map objects infobox. Used by the hab_layer
 * object to retrieve the rendered infoboxes for each payload */
int habhound_get_infobox(int index, cairo_surface_t **surface)
{
	map_object_t *obj = get_map_object(index);
	if(!obj) return(-1);
	
	*surface = obj->infobox;
	
	return(index);
}

void habhound_plot_object(const char *callsign, hab_object_type_t type,
	time_t timestamp, double latitude, double longitude, double altitude)
{
	obj_data_t *data = calloc(sizeof(obj_data_t), 1);
	if(!data) return;
	
	data->callsign  = strdup(callsign);
	data->type      = type;
	data->timestamp = timestamp;
	data->latitude  = latitude;
	data->longitude = longitude;
	data->altitude  = altitude;
	
	g_idle_add((GSourceFunc) cb_habhound_plot_object, data);
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
		"osd-y", 20,
		NULL);
	osm_gps_map_layer_add(map, osd);
	g_object_unref(G_OBJECT(osd));
	
	/* Setup the HAB OSD */
	osd = g_object_new(HAB_LAYER_TYPE, NULL);
	osm_gps_map_layer_add(map, osd);
	g_object_unref(G_OBJECT(osd));
	
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

