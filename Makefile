
CC=gcc
CFLAGS=-g -Wall
LDFLAGS=-g

# GLIB
CFLAGS+=`pkg-config --cflags glib-2.0`
LDFLAGS+=`pkg-config --libs glib-2.0`

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
	$(CC) $(LDFLAGS) habhound.o hab_layer.o habitat.o -o habhound

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm *.o

