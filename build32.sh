#!/bin/sh
cd "${0%/*}"

if [ $(uname) = "Darwin" ]; then
  clang++ panoview.cpp Unix/lodepng.cpp macOS/dlgmodule.mm macOS/config.cpp -o panoview -std=c++17 -ObjC++ -framework OpenGL -framework GLUT -DGL_SILENCE_DEPRECATION -framework Cocoa -m32
elif [ $(uname) = "Linux" ]; then
  g++ panoview.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -o panoview -std=c++17 -static-libgcc -static-libstdc++ -lSDL2 -lGL -lGLU -lglut -lm -lpthread -lX11 -lprocps -m32
elif [ $(uname) = "FreeBSD" ]; then
  clang++ panoview.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -o panoview -std=c++17 -I/usr/local/include -L/usr/local/lib -lSDL2 -lGL -lGLU -lglut -lm -lpthread -lX11 -lutil -lc -m32
else
  g++ panoview.cpp Win32/libpng-util.cpp Win32/dlgmodule.cpp -DFREEGLUT_STATIC -o panoview.exe -std=c++17 -static -L/c/msys64/mingw32/lib -static-libgcc -static-libstdc++ -lmingw32 -lSDL2main -lSDL2 -lpng -lz -lfreeglut_static -lglu32 -lopengl32 -lm -lwinpthread -lgdiplus -lshlwapi -lcomctl32 -lole32 -loleaut32 -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lsetupapi -lversion -luuid -mwindows -Wl,--subsystem,windows -fPIC -m32
fi
