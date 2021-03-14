#!/bin/sh
cd "${0%/*}"

if [ $(uname) = "Linux" ]; then
  g++ panoview.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -DFREEGLUT_GLES=ON -o panoview -std=c++17 -static-libgcc -static-libstdc++ -lSDL2 -lGL -lGLU -lglut -lm -lpthread -lX11
elif [ $(uname) = "FreeBSD" ]; then
  clang++ panoview.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -DFREEGLUT_GLES=ON -o panoview -std=c++17 -I/usr/local/include -L/usr/local/lib -lSDL2 -lGL -lGLU -lglut -lm -lpthread -lX11
fi
