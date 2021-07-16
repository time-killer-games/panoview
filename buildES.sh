#!/bin/sh
cd "${0%/*}"

if [ $(uname) = "Linux" ]; then
  g++ panoview.cpp Universal/crossprocess.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -DFREEGLUT_GLES=ON -o panoview -std=c++17 -static-libgcc -static-libstdc++ -lSDL2 -lGL -lGLU -lglut -lm -lpthread -lX11 -lXrandr -lXinerama -lprocps -no-pie -DXPROCESS_GUIWINDOW_IMPL
elif [ $(uname) = "FreeBSD" ]; then
  clang++ panoview.cpp Universal/crossprocess.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -DFREEGLUT_GLES=ON -o panoview -std=c++17 -I/usr/local/include -L/usr/local/lib -lGL -lGLU -lglut -lm -lpthread -lX11 -lXrandr -lXinerama -lprocstat -lutil -lc -no-pie -DXPROCESS_GUIWINDOW_IMPL
elif [ $(uname) = "DragonFly" ]; then
  g++ panoview.cpp Universal/crossprocess.cpp Unix/lodepng.cpp xlib/dlgmodule.cpp -o -DFREEGLUT_GLES=ON panoview -std=c++17 -I/usr/local/include -L/usr/local/lib -lGL -lGLU -lglut -lm -lpthread -lX11 -lXrandr -lXinerama -lkvm -lutil -lc -no-pie -DXPROCESS_GUIWINDOW_IMPL
fi
