#!/bin/sh
cd "${0%/*}"

if [ $(uname) = "Darwin" ]; then
  clang++ panoview.cpp Universal/lodepng.cpp macOS/dlgmodule.mm macOS/config.cpp -o panoview -std=c++17 -ObjC++ -lSDL2 -framework OpenGL -framework GLUT -framework Cocoa -m64
elif [ $(uname) = "Linux" ]; then
  g++ panoview.cpp Universal/lodepng.cpp xlib/dlgmodule.cpp -o panoview -std=c++17 -static-libgcc -static-libstdc++ -lSDL2 -lGL -lGLU -lglut -lm -lpthread -lX11 -lprocps -m64
elif [ $(uname) = "FreeBSD" ]; then
  clang++ panoview.cpp Universal/lodepng.cpp xlib/dlgmodule.cpp -o panoview -std=c++17 -lSDL2 -lGL -lGLU -lglut -lm -lpthread -lX11 -lutil -lc -lm -m64
else
  g++ panoview.cpp Universal/lodepng.cpp Win32/dlgmodule.cpp -DFREEGLUT_STATIC -o panoview.exe -std=c++17 -static -L/c/msys64/mingw64/lib -static-libgcc -static-libstdc++ -lmingw32 -lSDL2main -lSDL2 -lfreeglut_static -lglu32 -lopengl32 -lm -lwinpthread -lgdiplus -lshlwapi -lcomctl32 -lole32 -loleaut32 -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lsetupapi -lversion -luuid -mwindows -Wl,--subsystem,windows -m64
fi
