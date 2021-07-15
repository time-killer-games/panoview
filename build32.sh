#!/bin/sh
cd "${0%/*}"

if [ $(uname) = "Darwin" ]; then
  clang++ panoview.cpp Universal/crossprocess.cpp Unix/lodepng.cpp MacOSX/objcpp.mm MacOSX/dlgmodule.mm MacOSX/config.cpp -o panoview -std=c++17 -ObjC++ -framework OpenGL -framework GLUT -framework Cocoa -DGL_SILENCE_DEPRECATION -DXPROCESS_GUIWINDOW_IMPL -framework Cocoa -m32
elif [ $(uname) = "Linux" ]; then
  g++ panoview.cpp Universal/crossprocess.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -o panoview -std=c++17 -static-libgcc -static-libstdc++ -lGL -lGLU -lglut -lm -lpthread -lX11 -lXrandr -lXinerama -lprocps -no-pie -m32
elif [ $(uname) = "FreeBSD" ]; then
  clang++ panoview.cpp Universal/crossprocess.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -o panoview -std=c++17 -I/usr/local/include -L/usr/local/lib -lGL -lGLU -lglut -lm -lpthread -lX11 -lXrandr -lXinerama -lprocstat -lutil -lc -no-pie -m32
elif [ $(uname) = "DragonFly" ]; then
  g++ panoview.cpp Universal/crossprocess.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -o panoview -std=c++17 -I/usr/local/include -L/usr/local/lib -lGL -lGLU -lglut -lm -lpthread -lX11 -lXrandr -lXinerama -lkvm -lutil -lc -no-pie -m32
else
  windres icon.rc -O coff -o icon.res
  g++ panoview.cpp Universal/crossprocess.cpp Win32/libpng-util.cpp Win32/dlgmodule.cpp /c/msys64/mingw32/lib/libpng.a /c/msys64/mingw32/lib/libz.a /c/msys64/mingw32/lib/libfreeglut_static.a icon.res -DXPROCESS_WIN32EXE_INCLUDES -DXPROCESS_GUIWINDOW_IMPL -DFREEGLUT_STATIC -o panoview.exe -std=c++17 -static -I/c/msys64/mingw32/inlcude -L/c/msys64/mingw32/lib -static-libgcc -static-libstdc++ -lmingw32 -lglu32 -lopengl32 -lgdiplus -lgdi32 -lshlwapi -lcomctl32 -lcomdlg32 -lole32 -lwinmm -Wl,--subsystem,windows -fPIC -m32
  rm -f icon.res
fi
