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

/* This code handles communication with the habitat server. The idea is that
 * a curl multi handle is started with a single curl easy handle to request
 * basic details from the server (db name and sequence number). Once the
 * sequence number is known a new curl easy handle is used to read couchdb's
 * continous changes interface, requesting changes from the current sequence
 * number onwards. If necessary more curl easy interfaces can be added to
 * request specific documents. Whether they are returned as part of the
 * changes or directly doesn't matter.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <curl/curl.h>
#include <yajl/yajl_tree.h>
#include "habitat.h"
#include "habhound.h"

typedef struct {
	
	/* src_habitat state */
	src_habitat_t *s;
	
	/* The full URL of this connection */
	char *url;
	
	/* Text buffer */
	char *text;
	
	/* Length of text buffer, in characters */
	size_t length;
	
	/* Callback for when a complete string is received */
	void (*callback)(src_habitat_t *, char *, yajl_val);
	
} strbuf_t;

size_t strbuf_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	strbuf_t *sb = userdata;
	char *s;
	
	/* This function receives data from libcurl - it builds it into a
	 * string and passes each line to a callback function for processing */
	
	/* Append the new data to the buffer */
	if(sb->text == NULL)
	{
		/* First data.. just duplicate it */
		sb->text = strndup(ptr, size * nmemb);
		sb->length = size * nmemb;
	}
	else
	{
		/* Allocate new buffer */
		s = malloc(sb->length + size * nmemb + 1);
		
		/* Combine old and new data */
		strcpy(s, sb->text);
		strncat(s, ptr, size * nmemb);
		
		/* Replace old buffer */
		free(sb->text);
		sb->text = s;
		sb->length += size * nmemb;
	}
	
	/* Test for a newline */
	while((s = strstr(sb->text, "\n")) != NULL)
	{
		char errbuf[1024];
		yajl_val node = NULL;
		
		/* Null-terminate the string at the newline */
		s[0] = '\0';
		
		/* Pass the string to yajl */
		if(strlen(sb->text) > 0 && !(node = yajl_tree_parse(sb->text, errbuf, sizeof(errbuf))))
			fprintf(stderr, "parse_error: %s\n", *errbuf ? errbuf : "unknown error");
		
		/* Pass the string and result to the callback function */
		sb->callback(sb->s, sb->text, node);
		
		/* Free yajl data */
		if(node) yajl_tree_free(node);
		
		/* Move the remaining data in the string to the beginning */
		s++;
		sb->length = strlen(s);
		memmove(sb->text, s, sb->length + 1);
	}
	
	return(size * nmemb);
}

static int open_couch_url(src_habitat_t *s, void (*callback)(src_habitat_t *, char *, yajl_val), char *document, ... )
{
	CURL *c;
	strbuf_t *sb;
	va_list ap;
	char *temp;
	
	/* Allocate space for the private data */
	sb = calloc(sizeof(strbuf_t), 1);
	if(!sb) return(-1);
	
	/* Create the full URL */
	va_start(ap, document);
	temp = vmake_message(document, ap);
	va_end(ap);
	
	if(!temp)
	{
		/* Out of memory */
		free(sb);
		return(-1);
	}
	
	sb->url = sprintf_alloc("%s/%s", s->url, temp);
	free(temp);
	
	if(!sb->url)
	{
		/* Out of memory */
		free(sb);
		return(-1);
	}
	
	sb->s = s;
	sb->callback = callback;
	
	/* Got the full URL */
	fprintf(stderr, "=> %s\n", sb->url);
	
	c = curl_easy_init();
	curl_easy_setopt(c, CURLOPT_USERAGENT, "habhound/alpha");
	curl_easy_setopt(c, CURLOPT_URL, sb->url);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, strbuf_callback);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, sb);
	curl_easy_setopt(c, CURLOPT_PRIVATE, sb);
	curl_easy_setopt(c, CURLOPT_ENCODING, "");
	curl_multi_add_handle(s->cm, c);
	s->running++;
	
	return(s->running);
}

static int libcurl_perform(src_habitat_t *s)
{
	fd_set fdread, fdwrite, fdexcep;
	struct timeval t;
	long timeout;
	int msgs, maxfd;
	CURLMsg *msg;
	
	/* Let libcurl do it's thing */
	curl_multi_perform(s->cm, &s->running);
	
	if(s->running)
	{
		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);
		
		if(curl_multi_fdset(s->cm, &fdread, &fdwrite, &fdexcep, &maxfd))
		{
			fprintf(stderr, "curl_multi_fdset(): error\n");
			return(-1);
		}
		
		if(curl_multi_timeout(s->cm, &timeout))
		{
			fprintf(stderr, "curl_multi_timeout() error\n");
			return(-1);
		}
		
		/* Default timeout 100ms */
		if(timeout == -1) timeout = 100;
		
		if(maxfd == -1) usleep(timeout * 1000);
		else
		{
			t.tv_sec = timeout / 1000;
			t.tv_usec = (timeout % 1000) * 1000;
			
			if(select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &t) == -1)
			{
				fprintf(stderr, "select() error: %i: %s\n", errno, strerror(errno));
				return(-1);
			}
		}
	}
	
	while((msg = curl_multi_info_read(s->cm, &msgs)))
	{
		if(msg->msg == CURLMSG_DONE)
		{
			CURL *c;
			strbuf_t *sb;
			
			/* Remove handle from the multi group */
			c = msg->easy_handle;
			curl_multi_remove_handle(s->cm, c);
			
			/* Free memory used by the strbuf parser */
			curl_easy_getinfo(c, CURLINFO_PRIVATE, &sb);
			if(sb->text) free(sb->text);
			free(sb);
			
			/* Done with this handle */
			curl_easy_cleanup(c);
		}
	}
	
	return(0);
}

static void couch_document_callback(src_habitat_t *s, char *str, yajl_val node)
{
	const char *path[] = { 0, 0, 0 };
	const char *doctype, *callsign;
	yajl_val v;
	hab_object_type_t type;
	double lat, lng, alt;
	
	/* Find out which document type this is */
	path[0] = "type";
	v = yajl_tree_get(node, path, yajl_t_string);
	doctype = (v ? YAJL_GET_STRING(v) : NULL);
        if(!doctype) return;
	
	if(strcmp(doctype, "payload_telemetry") == 0) type = HAB_PAYLOAD;
	else if(strcmp(doctype, "listener_telemetry") == 0) type = HAB_LISTENER;
	else return; /* Unknown document type */
	
	path[0] = "data";
	
	/* In the case of payload telemetry, make sure the data has been
	 * parsed by the server */
	if(type == HAB_PAYLOAD)
	{
		path[1] = "_parsed";
		v = yajl_tree_get(node, path, yajl_t_true);
		if(!v || !YAJL_IS_TRUE(v)) return;
	}
	
	/* Get the callsign */
	path[1] = (type == HAB_PAYLOAD ? "payload" : "callsign");
	v = yajl_tree_get(node, path, yajl_t_string);
	callsign = (v ? YAJL_GET_STRING(v) : NULL);
	if(!callsign) return;
	
	/* Get the latitude */
	path[1] = "latitude";
	v = yajl_tree_get(node, path, yajl_t_number);
	lat = (v ? YAJL_GET_DOUBLE(v) : 0);
	
	/* Get the longitude */
	path[1] = "longitude";
	v = yajl_tree_get(node, path, yajl_t_number);
	lng = (v ? YAJL_GET_DOUBLE(v) : 0);
	
	/* Get the altitude */
	path[1] = "altitude";
	v = yajl_tree_get(node, path, yajl_t_number);
	alt = (v ? YAJL_GET_DOUBLE(v) : 0);
	
	/* Listener stations with "chase" in the name get the car icon */
	if(type == HAB_LISTENER && strcasestr(callsign, "chase"))
		type = HAB_CHASE;
	
	/* Send it to the map! */
	habhound_plot_object(callsign, type, time(NULL), lat, lng, alt);
}

static void couch_changes_callback(src_habitat_t *s, char *str, yajl_val node)
{
	const char *path[] = { 0, 0 };
	yajl_val v;
	char *id;
	
	/* Couchdb should send an empty line to keep the connection alive */
	if(strlen(str) == 0)
	{
		fprintf(stderr, "Ping? Pong!\n");
		return;
	}
	
	/* Update the recorded sequence number */
	path[0] = "seq";
	v = yajl_tree_get(node, path, yajl_t_number);
	if(v) s->seq = YAJL_GET_INTEGER(v);
	else return;
	
	/* Was the document included? */
	path[0] = "doc";
	v = yajl_tree_get(node, path, yajl_t_object);
	if(v)
	{
		couch_document_callback(s, NULL, v);
		return;
	}
	
	/* The document wasn't included in the changes record,
	 * request it directly */
	path[0] = "id";
	v = yajl_tree_get(node, path, yajl_t_string);
	id = (v ? YAJL_GET_STRING(v) : NULL);
	if(!id) return;
	
	open_couch_url(s, couch_document_callback, "%s", id);
}

static void couch_initial_callback(src_habitat_t *s, char *str, yajl_val node)
{
	const char *path[] = { 0, 0 };
	yajl_val v;
	int seq;
	
	/* Don't proceed if no JSON data present */
	if(!node) return;
	
	path[0] = "update_seq";
	v = yajl_tree_get(node, path, yajl_t_number);
	if(v) seq = YAJL_GET_INTEGER(v);
	else
	{
		fprintf(stderr, "No update_seq found in response from server\n");
		return;
	}
	
	path[0] = "db_name";
	v = yajl_tree_get(node, path, yajl_t_string);
	if(v) s->db_name = strdup(YAJL_GET_STRING(v));
	else
	{
		fprintf(stderr, "No db_name found in response from server\n");
		return;
	}
	
	fprintf(stderr, "Connected to %s\n", s->url);
	fprintf(stderr, "db_name: %s\n", s->db_name);
	fprintf(stderr, "update_seq: %i\n", seq);
	
	/* Resume from the previous point if one is set */
	if(s->seq > 0) fprintf(stderr, "Resuming from update_seq: %i\n", s->seq);
	else s->seq = seq;
	
	habhound_set_status("Connected to %s", s->url);
	
	/* Server seems good, begin monitoring changes */
	open_couch_url(s, couch_changes_callback, "_changes?feed=continuous&since=%i&heartbeat=5000&include_docs=true", s->seq);
}

static void *habitat_thread(void *arg)
{
	src_habitat_t *s = (src_habitat_t *) arg;
	int r;
	
	/* Create the multi interface, and enable pipelining */
	s->cm = curl_multi_init();
	if(!s->cm) return(NULL);
	
	curl_multi_setopt(s->cm, CURLMOPT_PIPELINING, 1L);
	
	/* Open the initial connection to the database */
	habhound_set_status("Connecting to server...");
	open_couch_url(s, couch_initial_callback, "");
	
	/* The main libcurl loop */
	while(!s->stopping)
	{
		/* The main libcurl loop */
		while(s->running)
		{
			r = libcurl_perform(s);
			if(r != 0) break;
			if(s->stopping) break;
		}
		
		if(!s->stopping)
		{
			fprintf(stderr, "Disconnected from server. Reconnecting in 10 seconds...\n");
			habhound_set_status("Disconnected from server. Reconnecting in 10 seconds...");
		}
		
		/* Sleep for 10 seconds */
		for(r = 0; r < 100 && !s->stopping; r++)
			usleep(100000);
		
		/* Start a new connection to habitat */
		if(!s->stopping) open_couch_url(s, couch_initial_callback, "");
	}
	
	curl_multi_cleanup(s->cm);
	
	fprintf(stderr, "habitat thread ending\n");
	
	return(NULL);
}

src_habitat_t *src_habitat_start(char *url)
{
	src_habitat_t *s;
	pthread_attr_t attr;
	int r;
	
	s = calloc(sizeof(src_habitat_t), 1);
	if(!s) return(NULL);
	
	/* Make a copy of the habitat server base-URL */
	s->url = strdup(url);
	if(!s)
	{
		free(s);
		return(NULL);
	}
	
	/* Start the thread */
	pthread_attr_init(&attr);
	r = pthread_create(&s->t, &attr, habitat_thread, (void *) s);
	pthread_attr_destroy(&attr);
	
	if(r != 0)
	{
		/* Didn't work! */
		fprintf(stderr, "habitat thread failed to start\n");
		perror("pthread_create");
		
		free(s->url);
		free(s);
		
		return(NULL);
	}
	
	return(s);
}

void src_habitat_stop(src_habitat_t *s)
{
	/* Signal to the thread we're stopping */
	s->stopping = 1;
	
	/* Wait until it complies */
	pthread_join(s->t, NULL);
	
	free(s->url);
	free(s);
}

