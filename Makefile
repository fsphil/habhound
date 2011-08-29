
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

habhound: habhound.o hab_layer.o
	$(CC) $(LDFLAGS) habhound.o hab_layer.o -o habhound

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm *.o

