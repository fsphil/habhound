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

#include "habhound.h"
#include "hab_layer.h"

static void hab_layer_iface_init(OsmGpsMapLayerIface *iface);

enum {
	P_STATUS = 1,
};

G_DEFINE_TYPE_WITH_CODE(hab_layer, hab_layer, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE(OSM_TYPE_GPS_MAP_LAYER, hab_layer_iface_init));

struct _hab_layer_private
{
	char *status;
};

static void     hab_layer_render (OsmGpsMapLayer *osd, OsmGpsMap *map);
static void     hab_layer_draw   (OsmGpsMapLayer *osd, OsmGpsMap *map, GdkDrawable *drawable);
static gboolean hab_layer_busy   (OsmGpsMapLayer *osd);
static gboolean hab_layer_press  (OsmGpsMapLayer *osd, OsmGpsMap *map, GdkEventButton *event);

static void scale_draw(hab_layer *self, GtkAllocation *allocation, cairo_t *cr);
static void status_bar_draw(hab_layer *self, GtkAllocation *allocation, cairo_t *cr);

static void hab_layer_iface_init(OsmGpsMapLayerIface *iface)
{
	iface->render       = hab_layer_render;
	iface->draw         = hab_layer_draw;
	iface->busy         = hab_layer_busy;
	iface->button_press = hab_layer_press;
}

static void hab_layer_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	g_return_if_fail(IS_HAB_LAYER(object));
	hab_layer *o = HAB_LAYER(object);
	hab_layer_private *priv = o->priv;
	
	switch(prop_id)
	{
	case P_STATUS:
		if(priv->status) g_free(priv->status);
		if(g_value_get_string(value)) priv->status = g_value_dup_string(value);
		else priv->status = NULL;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void hab_layer_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	g_return_if_fail(IS_HAB_LAYER(object));
	hab_layer *o = HAB_LAYER(object);
	hab_layer_private *priv = o->priv;
	
	switch(prop_id)
	{
	case P_STATUS:
		g_value_set_string(value, priv->status);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static GObject *hab_layer_constructor(GType gtype, guint n_properties, GObjectConstructParam *properties)
{
	GObject *object;
	
	/* Always chain up to the parent constructor */
	object = G_OBJECT_CLASS(hab_layer_parent_class)->constructor(gtype, n_properties, properties);
	
	return(object);
}

static void hab_layer_finalize(GObject *object)
{
	hab_layer_private *priv = HAB_LAYER(object)->priv;
	
	if(priv->status) g_free(priv->status);
	
	G_OBJECT_CLASS(hab_layer_parent_class)->finalize(object);
}

static void hab_layer_class_init(hab_layerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	
	g_type_class_add_private(klass, sizeof(hab_layer_private));
	
	object_class->get_property = hab_layer_get_property;
	object_class->set_property = hab_layer_set_property;
	object_class->constructor  = hab_layer_constructor;
	object_class->finalize     = hab_layer_finalize;
	
	g_object_class_install_property(
		object_class, P_STATUS,
		g_param_spec_string("status", "status",
			"Status bar text", "habhound/alpha",
			G_PARAM_READWRITE | G_PARAM_CONSTRUCT)
	);
}

static void hab_layer_init(hab_layer *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, HAB_LAYER_TYPE, hab_layer_private);
}

static void hab_layer_render(OsmGpsMapLayer *osd, OsmGpsMap *map)
{
	/* No rendering is done here */
}

static void hab_layer_draw(OsmGpsMapLayer *osd, OsmGpsMap *map, GdkDrawable *drawable)
{
	cairo_t *cr;
	hab_layer *self;
	GtkAllocation allocation;
	
	self = HAB_LAYER(osd);
	
	gtk_widget_get_allocation(GTK_WIDGET(map), &allocation);
	cr = gdk_cairo_create(drawable);
	
	scale_draw(self, &allocation, cr);
	status_bar_draw(self, &allocation, cr);
	
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

static void scale_draw(hab_layer *self, GtkAllocation *allocation, cairo_t *cr)
{
	cairo_surface_t *surface;
	gint i, x, y;
	
	x = allocation->width - 220 - 10;
	y = 10;
	
	for(i = 0; habhound_get_infobox(i, &surface) != -1; i++)
	{
		if(!surface) continue;
		
		cairo_set_source_surface(cr, surface, x, y);
		cairo_paint(cr);
		
		y += 110;
	}
}

static void status_bar_draw(hab_layer *self, GtkAllocation *allocation, cairo_t *cr)
{
	hab_layer_private *priv = self->priv;
	
	/* Draw the outline box */
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.6);
	cairo_set_line_width(cr, 0);
	cairo_rectangle(cr, 0, allocation->height - 14, allocation->width, 14);
	cairo_fill(cr);
	
	/* Draw the payload title */
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_select_font_face(cr, "Sans",
		CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 9);
	
	/* Draw the status message */
	if(priv->status)
	{
		cairo_move_to(cr, 2, allocation->height - 3);
		cairo_show_text(cr, priv->status);
	}
}

