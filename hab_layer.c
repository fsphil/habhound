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

#include <cairo.h>
#include <osm-gps-map-layer.h>

#include "hab_layer.h"

static void hab_layer_iface_init(OsmGpsMapLayerIface *iface);

G_DEFINE_TYPE_WITH_CODE(hab_layer, hab_layer, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE(OSM_TYPE_GPS_MAP_LAYER, hab_layer_iface_init));

struct _hab_layer_private
{
	cairo_surface_t *surface;
};

static void     hab_layer_render (OsmGpsMapLayer *osd, OsmGpsMap *map);
static void     hab_layer_draw   (OsmGpsMapLayer *osd, OsmGpsMap *map, GdkDrawable *drawable);
static gboolean hab_layer_busy   (OsmGpsMapLayer *osd);
static gboolean hab_layer_press  (OsmGpsMapLayer *osd, OsmGpsMap *map, GdkEventButton *event);

static void scale_render(hab_layer *self, OsmGpsMap *map);
static void scale_draw(hab_layer *self, GtkAllocation *allocation, cairo_t *cr);

static void hab_layer_iface_init(OsmGpsMapLayerIface *iface)
{
	iface->render       = hab_layer_render;
	iface->draw         = hab_layer_draw;
	iface->busy         = hab_layer_busy;
	iface->button_press = hab_layer_press;
}

static GObject *hab_layer_constructor(GType gtype, guint n_properties, GObjectConstructParam *properties)
{
	GObject *object;
	hab_layer_private *priv;
	
	/* Always chain up to the parent constructor */
	object = G_OBJECT_CLASS(hab_layer_parent_class)->constructor(gtype, n_properties, properties);
	priv = HAB_LAYER(object)->priv;
	
	priv->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 220, 104);
	
	return(object);
}

static void hab_layer_finalize(GObject *object)
{
	hab_layer_private *priv = HAB_LAYER(object)->priv;
	
	if(priv->surface) cairo_surface_destroy(priv->surface);
	
	G_OBJECT_CLASS(hab_layer_parent_class)->finalize(object);
}

static void hab_layer_class_init(hab_layerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	
	g_type_class_add_private(klass, sizeof(hab_layer_private));
	
	//object_class->get_property = hab_layer_get_property;
	//object_class->set_property = hab_layer_set_property;
	object_class->constructor  = hab_layer_constructor;
	object_class->finalize     = hab_layer_finalize;
}

static void hab_layer_init(hab_layer *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, HAB_LAYER_TYPE, hab_layer_private);
}

static void hab_layer_render(OsmGpsMapLayer *osd, OsmGpsMap *map)
{
	hab_layer *self;
	//hab_layer_private *priv;
	
	self = HAB_LAYER(osd);
	//priv = self->priv;
	
	scale_render(self, map);
}

static void hab_layer_draw(OsmGpsMapLayer *osd, OsmGpsMap *map, GdkDrawable *drawable)
{
	cairo_t *cr;
	hab_layer *self;
	//hab_layer_private *priv;
	GtkAllocation allocation;
	
	self = HAB_LAYER(osd);
	//priv = self->priv;
	
	gtk_widget_get_allocation(GTK_WIDGET(map), &allocation);
	cr = gdk_cairo_create(drawable);
	
        scale_draw(self, &allocation, cr);
	
	cairo_destroy(cr);
}

static gboolean hab_layer_busy(OsmGpsMapLayer *osd)
{
	return(FALSE);
}

static gboolean hab_layer_press(OsmGpsMapLayer *osd, OsmGpsMap *map, GdkEventButton *event)
{
	return(FALSE);
}

hab_layer *hab_layer_new(void)
{
	return(g_object_new(HAB_LAYER_TYPE, NULL));
}

static void scale_render(hab_layer *self, OsmGpsMap *map)
{
	hab_layer_private *priv = self->priv;
	
	if(!priv->surface) return;
	
	/* first fill with transparency */
	g_assert(priv->surface);
	cairo_t *cr = cairo_create(priv->surface);
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
	cairo_surface_t *image;
	image = cairo_image_surface_create_from_png("icons/balloon-blue.png");
	
	cairo_set_source_surface(cr, image, 166, 5);
	cairo_paint(cr);
	cairo_surface_destroy(image);
	
	/* Draw the payload title */
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, 12);
	
	cairo_move_to(cr, 5, 14 /* - te.height / 2 - te.y_bearing*/);
	cairo_show_text(cr, "XABEN1");
	
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 10);
	
	cairo_move_to(cr, 5, 14 + 11);
	cairo_show_text(cr, "Time: 2011-08-27 14:02:06");
	
	cairo_move_to(cr, 5, 14 + 22);
	cairo_show_text(cr, "Position: -152.23715, -100.22074");
	
	cairo_move_to(cr, 5, 14 + 33);
	cairo_show_text(cr, "Altitude: 39594 m Rate: 100.0 m/s");
	
	cairo_move_to(cr, 5, 14 + 44);
	cairo_show_text(cr, "Max. Altitude: 39595 m");
	
	cairo_move_to(cr, 5, 14 + 55);
	cairo_show_text(cr, "Data: ");
	
	cairo_move_to(cr, 5, 14 + 66);
	cairo_show_text(cr, "Receivers: G8KHW");
	
	cairo_move_to(cr, 5, 14 + 85);
	cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
	cairo_show_text(cr, "Pan To | Path | Follow");
	
	cairo_destroy(cr);
}

static void scale_draw(hab_layer *self, GtkAllocation *allocation, cairo_t *cr)
{
	hab_layer_private *priv = self->priv;
	//OsdScale_t *scale = self->priv->scale;
	
	gint x, y;
	
	//x =  priv->osd_x;
	//y = -priv->osd_y;
	//if(x < 0) x += allocation->width - OSD_SCALE_W;
	//if(y < 0) y += allocation->height - OSD_SCALE_H;
	
	x = allocation->width - 220 - 10;
	y = 10;
	
	cairo_set_source_surface(cr, priv->surface, x, y);
	cairo_paint(cr);
}

