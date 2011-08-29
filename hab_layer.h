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

#ifndef __HAB_LAYER_H__
#define __HAB_LAYER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define HAB_LAYER_TYPE           (hab_layer_get_type())
#define HAB_LAYER(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj), HAB_LAYER_TYPE, hab_layer))
#define HAB_LAYER_CLASS(obj)     (G_TYPE_CHECK_CLASS_CAST((obj), HAB_LAYER, hab_layerClass))
#define IS_HAB_LAYER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj), HAB_LAYER))
#define IS_HAB_LAYER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE((obj), HAB_LAYER))
#define HAB_LAYER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), HAB_LAYER, hab_layerClass))

typedef struct _hab_layer hab_layer;
typedef struct _hab_layer_private hab_layer_private;
struct _hab_layer
{
	GObject parent;
	hab_layer_private *priv;
};

typedef struct _hab_layer_class hab_layerClass;
struct _hab_layer_class
{
	GObjectClass parent_class;
};

GType hab_layer_get_type(void);
hab_layer *hab_layer_new(void);

G_END_DECLS

#endif /* __HAB_LAYER_H__ */
