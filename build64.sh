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
  g++ panoview.cpp Universal/crossprocess.cpp Win32/libpng-util.cpp Win32/dlgmodule.cpp /c/msys64/mingw64/lib/libpng.a /c/msys64/mingw64/lib/libz.a /c/msys64/mingw64/lib/libfreeglut_static.a -DXPROCESS_WIN32EXE_INCLUDES -DFREEGLUT_STATIC -o panoview.exe -std=c++17 -static -I/c/msys64/mingw64/inlcude -L/c/msys64/mingw64/lib -static-libgcc -static-libstdc++ -lmingw32 -lglu32 -lopengl32 -lgdiplus -lgdi32 -lshlwapi -lcomctl32 -lcomdlg32 -lole32 -lwinmm -Wl,--subsystem,windows -fPIC -m64
fi
