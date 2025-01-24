#!/bin/bash

if [ "$LINKING" != "dynamic" ]
then
    LINKING="static"
fi

LIGHTSTONE_INC=./liblightstone-1.5/include
LIGHTSTONE_LIB=./liblightstone-1.5/build/lib
EXE=odysseusmonitor.x

CFLAGS="-I $LIGHTSTONE_INC -g"
if [ "$LINKING" == "static" ]
then
    # Static linking
    LDFLAGS="$(sdl2-config --static-libs) $LIGHTSTONE_LIB/liblightstone.a -L $LIGHTSTONE_LIB -lSDL2_ttf -lSDL2_gfx -lusb-1.0"
else
    # Dynamic linking
    LDFLAGS="$(sdl2-config --libs) -L $LIGHTSTONE_LIB -llightstone -lSDL2_ttf -lSDL2_gfx"
    cp $LIGHTSTONE_LIB/liblightstone.so.1.5.0 .
fi

g++ $(sdl2-config --cflags) -c src/main.cpp $CFLAGS -o src/main.o
g++ -Wl,-R -Wl,. src/main.o $LDFLAGS -o $EXE


