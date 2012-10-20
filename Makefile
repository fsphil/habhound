
CC=gcc
CFLAGS=-g -Wall
LDFLAGS=-g -lpthread -lm

# GLIB
CFLAGS+=`pkg-config --cflags glib-2.0`
LDFLAGS+=`pkg-config --libs glib-2.0`

# gthread
CFLAGS+=`pkg-config --cflags gthread-2.0`
LDFLAGS+=`pkg-config --libs gthread-2.0`

# GTK
CFLAGS+=`pkg-config --cflags gtk+-2.0`
LDFLAGS+=`pkg-config --libs gtk+-2.0`

# osm-gps-map
CFLAGS+=`pkg-config --cflags osmgpsmap`
LDFLAGS+=`pkg-config --libs osmgpsmap`

# libcurl
CFLAGS+=`pkg-config --cflags libcurl`
LDFLAGS+=`pkg-config --libs libcurl`

# yajl
#CFLAGS+=`pkg-config --cflags yajl`
LDFLAGS+="-lyajl"

habhound: habhound.o hab_layer.o habitat.o
	$(CC) -o habhound habhound.o hab_layer.o habitat.o $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm *.o

