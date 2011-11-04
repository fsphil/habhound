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

#ifndef __HABHOUND_H__
#define __HABHOUND_H__

#include <cairo.h>
#include <stdarg.h>

typedef enum {
	HAB_PAYLOAD,
	HAB_LISTENER,
	HAB_CHASE,
} hab_object_type_t;

extern char *vmake_message(const char *fmt, va_list ap);
extern char *sprintf_alloc(const char *format, ... );

extern void habhound_plot_object(
	const char *callsign,
	hab_object_type_t type,
	time_t timestamp,
	double latitude,
	double longitude,
	double altitude
);

extern void habhound_set_status(char *format, ... );
extern int habhound_get_infobox(int index, cairo_surface_t **surface);
extern void habhound_delete_object(const char *callsign);

#endif /* __HABHOUND_H__ */

