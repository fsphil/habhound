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

typedef enum {
	HAB_PAYLOAD,
	HAB_LISTENER,
	HAB_CHASE,
} hab_object_type_t;

extern void habhound_plot_object(
	const char *callsign,
	hab_object_type_t type,
	double latitude,
	double longitude,
	double altitude
);

extern void habhound_delete_object(const char *callsign);

#endif /* __HABHOUND_H__ */

