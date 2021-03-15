#!/bin/sh
cd "${0%/*}"

if [ $(uname) = "Darwin" ]; then
  clang++ xproc.cpp -o xproc -std=c++17 -m32;
elif [ $(uname) = "Linux" ]; then
  g++ xproc.cpp -o xproc -std=c++17 -static-libgcc -static-libstdc++ -lprocps -lpthread -m32;
elif [ $(uname) = "FreeBSD" ]; then
  clang++ xproc.cpp -o xproc -std=c++17 -lc -lutil -lprocstat -lpthread -m32;
else
  /c/msys64/msys2_shell.cmd -defterm -mingw32 -no-start -here -lc "g++ xproc.cpp -o xproc.exe -std=c++17 -static-libgcc -static-libstdc++ -static -m32";
fi
