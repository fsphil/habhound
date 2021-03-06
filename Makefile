
CC=gcc
CFLAGS=-g -Wall
LDFLAGS=-g -lpthread -lm

# GTK
CFLAGS+=`pkg-config --cflags gtk+-3.0`
LDFLAGS+=`pkg-config --libs gtk+-3.0`

# osm-gps-map
CFLAGS+=`pkg-config --cflags osmgpsmap-1.0`
LDFLAGS+=`pkg-config --libs osmgpsmap-1.0`

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

