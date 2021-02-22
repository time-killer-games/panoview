#!/bin/sh
cd "${0%/*}"

if [ $(uname) = "Darwin" ]; then
  clang++ panoview.cpp Universal/lodepng.cpp macOS/dlgmodule.cpp macOS/config.cpp -o panoview -std=c++17 -lSDL2 -framework OpenGL -framework GLUT -framework Cocoa -m64
elif [ $(uname) = "Linux" ]; then
  g++ panoview.cpp Universal/lodepng.cpp xlib/dlgmodule.cpp -o panoview -std=c++17 -static-libgcc -static-libstdc++ -lSDL2 -lGL -lGLU -lglut -lm -lpthread -lX11 -lprocps -m64
elif [ $(uname) = "FreeBSD" ]; then
  clang++ panoview.cpp Universal/lodepng.cpp xlib/dlgmodule.cpp -o panoview -std=c++17 -lSDL2 -lGL -lGLU -lglut -lm -lpthread -lX11 -lutil -lc -lm -m64
else
  g++ panoview.cpp Universal/lodepng.cpp Win32/dlgmodule.cpp -o panoview.exe -std=c++17 -static -static-libgcc -static-libstdc++ -lSDL2 -lGL -lGLU -lglut -lm -m64
fi
