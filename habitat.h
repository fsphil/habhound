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

#ifndef __HABITAT_H__
#define __HABITAT_H__

typedef struct
{
	/* Base URL of the CouchDB server */
	char *url;
	
	/* libcurl mutli interface handle */
	CURLM *cm;
	
	/* Number of curl easy interfaces running */
	int running;
	
	/* Server details */
	char *db_name; /* Database name */
	int seq; /* Sequence number */
	
	/* Thread stuffs */
	pthread_t t;
	char stopping;
	
} src_habitat_t;

extern src_habitat_t *src_habitat_start(char *url);
extern void src_habitat_stop();

#endif /* __HABITAT_H__ */

