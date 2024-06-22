#!/bin/bash

LIGHTSTONE_INC=./liblightstone-1.5/include
LIGHTSTONE_LIB=./liblightstone-1.5/build/lib

CFLAGS="-I $LIGHTSTONE_INC"
LDFLAGS="-L $LIGHTSTONE_LIB -llightstone -lSDL2_ttf -lSDL2_gfx"

g++ $(sdl2-config --cflags) -c main.cpp $CFLAGS -o main.o
g++ -Wl,-R -Wl,. main.o $(sdl2-config --libs) $LDFLAGS -o main.x

cp $LIGHTSTONE_LIB/liblightstone.so.1.5.0 .

#g++ -I ./include -c lightstone.c -o lightstone.o
#g++ -I ./include -c lightstone_libusb1.c -o lightstone_libusb1.o

#g++ main.o lightstone.o lightstone_libusb1.o $(sdl2-config --libs) -lSDL2_ttf -o main.x

exit
# This was for the lightstone
g++ -I ../../include -c lightstone_test.c -o lightstone_test.o
g++ -Wl,-R -Wl,. -L ../../build2/lib lightstone_test.o -llightstone -o test.x
