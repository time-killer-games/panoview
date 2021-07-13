#!/bin/sh
cd "${0%/*}"

if [ $(uname) = "Darwin" ]; then
  clang++ panoview.cpp Universal/crossprocess.cpp Unix/lodepng.cpp MacOSX/setpolicy.mm MacOSX/dlgmodule.mm MacOSX/config.cpp -o panoview -std=c++17 -ObjC++ -framework OpenGL -framework GLUT -DGL_SILENCE_DEPRECATION -DXPROCESS_GUIWINDOW_IMPL -framework Cocoa -m64
elif [ $(uname) = "Linux" ]; then
  g++ panoview.cpp Universal/crossprocess.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -o panoview -std=c++17 -static-libgcc -static-libstdc++ -lGL -lGLU -lglut -lm -lpthread -lX11 -lXrandr -lXinerama -lprocps -m64
elif [ $(uname) = "FreeBSD" ]; then
  clang++ panoview.cpp Universal/crossprocess.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -o panoview -std=c++17 -I/usr/local/include -L/usr/local/lib -lGL -lGLU -lglut -lm -lpthread -lX11 -lXrandr -lXinerama -lprocstat -lutil -lc -m64
elif [ $(uname) = "DragonFly" ]; then
  g++ panoview.cpp Universal/crossprocess.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -o panoview -std=c++17 -I/usr/local/include -L/usr/local/lib -lGL -lGLU -lglut -lm -lpthread -lX11 -lXrandr -lXinerama -lkvm -lutil -lc -m64
else
  cd "Universal"
  "/c/msys64/msys2_shell.cmd" -defterm -mingw32 -no-start -here -lc "g++ crossprocess.cpp -o crossprocess32.exe -std=c++17 -static-libgcc -static-libstdc++ -static -m32";
  "/c/msys64/msys2_shell.cmd" -defterm -mingw64 -no-start -here -lc "g++ crossprocess.cpp -o crossprocess64.exe -std=c++17 -static-libgcc -static-libstdc++ -static -m64";
  xxd -i 'crossprocess32' | sed 's/\([0-9a-f]\)$/\0, 0x00/' > 'crossprocess32.h'
  xxd -i 'crossprocess64' | sed 's/\([0-9a-f]\)$/\0, 0x00/' > 'crossprocess64.h'
  rm -f "crossprocess32.exe" "crossprocess64.exe"
  cd ".."
  g++ panoview.cpp Universal/crossprocess.cpp Win32/libpng-util.cpp Win32/dlgmodule.cpp -DXPROCESS_WIN32EXE_INCLUDES -DFREEGLUT_STATIC -o panoview.exe -std=c++17 -static -I/c/msys64/mingw64/inlcude -L/c/msys64/mingw64/lib -static-libgcc -static-libstdc++ -lmingw32 -lSDL2 -lSDL2_main `pkg-config --cflags --libs libpng` -lfreeglut_static -lglu32 -lopengl32 -lm -lwinpthread -lgdiplus -lshlwapi -lcomctl32 -lole32 -loleaut32 -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lsetupapi -lversion -luuid -mwindows -Wl,--subsystem,windows -fPIC -m64
  rm -f "Universal/crossprocess32.h" "Universal/crossprocess64.h"
fi
