/*

 MIT License
 
 Copyright © 2021 Samuel Venable
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 
*/

#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iostream>

#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <climits>
#include <cstdio>
#include <cmath>

#include "Universal/xproc.h"
#include "Universal/dlgmodule.h"
#if OS_PLATFORM == OS_WINDOWS
#include "Win32/libpng-util.h"
#elif OS_UNIXLIKE == true
#include "Unix/lodepng.h"
#endif

#if OS_UNIXLIKE == true
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#if OS_PLATFORM == OS_MACOS
#include <libproc.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <GLUT/glut.h>
#else
#if OS_PLATFORM == OS_WINDOWS
#include <windows.h>
#elif OS_PLATFORM == OS_LINUX
#include <GL/glx.h>
#elif OS_PLATFORM == OS_FREEBSD
#include <sys/sysctl.h>
#include <GL/glx.h>
#endif
#include <GL/glut.h>
#include <SDL2/SDL.h>
#endif

#if OS_PLATFORM == OS_WINDOWS
#define GL_CLAMP_TO_EDGE 0x812F
#endif

// magic numbers...
#define KEEP_XANGLE 361
#define KEEP_YANGLE -91

using std::string;
using std::to_string;
using std::wstring;
using std::vector;
using std::size_t;

#if OS_UNIXLIKE == true
typedef pid_t PROCID;
#else
typedef DWORD PROCID;
#endif
typedef string wid_t;

namespace {

string cwd;
wid_t WindowID  = "-1"; 
const double PI = 3.141592653589793;

void LoadImage(unsigned char **out, unsigned *pngwidth, unsigned *pngheight, 
  const char *fname) {
  unsigned char *data = nullptr;
  #if OS_PLATFORM == OS_WINDOWS
  wstring u8fname = widen(fname); 
  unsigned error = libpng_decode32_file(&data, pngwidth, pngheight, u8fname.c_str());
  #else
  unsigned error = lodepng_decode32_file(&data, pngwidth, pngheight, fname);
  #endif
  if (error) { return; } unsigned width = *pngwidth, height = *pngheight;

  const int size = width * height * 4;
  unsigned char *buffer = new unsigned char[size]();

  for (unsigned i = 0; i < height; i++) {
    for (unsigned j = 0; j < width; j++) {
      unsigned oldPos = (height - i - 1) * (width * 4) + 4 * j;
      unsigned newPos = i * (width * 4) + 4 * j;
      buffer[newPos + 0] = data[oldPos + 0];
      buffer[newPos + 1] = data[oldPos + 1];
      buffer[newPos + 2] = data[oldPos + 2];
      buffer[newPos + 3] = data[oldPos + 3];
    }
  }

  *out = buffer;
  delete[] data;
}

GLuint tex;
double TexWidth, TexHeight, AspectRatio;
void LoadPanorama(const char *fname) {
  unsigned char *data = nullptr;
  unsigned pngwidth, pngheight;
  LoadImage(&data, &pngwidth, &pngheight, fname);
  TexWidth  = pngwidth; TexHeight = pngheight;
  AspectRatio = TexWidth / TexHeight;

  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pngwidth, pngheight, 0, 
  GL_RGBA, GL_UNSIGNED_BYTE, data);
  delete[] data;
}

double xangle, yangle;
void DrawPanorama() {
  double i, resolution  = 0.3141592653589793;
  double height = 700 / AspectRatio, radius = 100;

  glPushMatrix(); glTranslatef(0, -350 / AspectRatio, 0);
  glRotatef(xangle + 90, 0, 90 + 1, 0);
  glBegin(GL_TRIANGLE_FAN);

  glTexCoord2f(0.5, 1); glVertex3f(0, height, 0);
  for (i = 2 * PI; i >= 0; i -= resolution) {
    glTexCoord2f(0.5f * cos(i) + 0.5f, 0.5f * sin(i) + 0.5f);
    glVertex3f(radius * cos(i), height, radius * sin(i));
  }

  glTexCoord2f(0.5, 0.5); glVertex3f(radius, height, 0);
  glEnd(); glBegin(GL_TRIANGLE_FAN);
  glTexCoord2f(0.5, 0.5); glVertex3f(0, 0, 0);
  for (i = 0; i <= 2 * PI; i += resolution) {
    glTexCoord2f(0.5f * cos(i) + 0.5f, 0.5f * sin(i) + 0.5f);
    glVertex3f(radius * cos(i), 0, radius * sin(i));
  }

  glEnd(); glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= 2 * PI; i += resolution) {
    const float tc = (i / (float)(2 * PI));
    glTexCoord2f(tc, 0.0);
    glVertex3f(radius * cos(i), 0, radius * sin(i));
    glTexCoord2f(tc, 1.0);
    glVertex3f(radius * cos(i), height, radius * sin(i));
  }

  glTexCoord2f(0.0, 0.0);
  glVertex3f(radius, 0, 0);
  glTexCoord2f(0.0, 1.0);
  glVertex3f(radius, height, 0);
  glEnd(); glPopMatrix();
}

GLuint cur;
void LoadCursor(const char *fname) {
  unsigned char *data = nullptr;
  unsigned pngwidth, pngheight;
  LoadImage(&data, &pngwidth, &pngheight, fname);

  glGenTextures(1, &cur);
  glBindTexture(GL_TEXTURE_2D, cur);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pngwidth, pngheight, 0, 
  GL_RGBA, GL_UNSIGNED_BYTE, data);
  delete[] data;
}

void DrawCursor(GLuint texid, int curx, int cury, int curwidth, int curheight) {
  glBindTexture(GL_TEXTURE_2D, texid); glEnable(GL_TEXTURE_2D);
  glColor4f(1, 1, 1, 1); glBegin(GL_QUADS);

  glTexCoord2f(0, 1); glVertex2f(curx, cury);
  glTexCoord2f(0, 0); glVertex2f(curx, cury + curheight);
  glTexCoord2f(1, 0); glVertex2f(curx + curwidth, cury + curheight);
  glTexCoord2f(1, 1); glVertex2f(curx + curwidth, cury);
  glEnd(); glDisable(GL_TEXTURE_2D);
}

#if OS_PLATFORM != OS_MACOS
SDL_Window *hidden = nullptr;
#endif

vector<string> StringSplit(string str, string delim) {
  vector<string> vec;
  std::stringstream sstr(str);
  string tmp;
  while (std::getline(sstr, tmp, delim[0]))
    vec.push_back(tmp);
  return vec;
}

void EnvironFromStdInput(string name, string *value) {
  #if OS_PLATFORM == OS_WINDOWS
  DWORD bytesAvail = 0;
  HANDLE hPipe = GetStdHandle(STD_INPUT_HANDLE);
  FlushFileBuffers(hPipe);
  if (PeekNamedPipe(hPipe, nullptr, 0, nullptr, &bytesAvail, nullptr)) {
    DWORD bytesRead = 0;    
    string buffer; buffer.resize(bytesAvail, '\0');
    if (PeekNamedPipe(hPipe, &buffer[0], bytesAvail, &bytesRead, 
      nullptr, nullptr)) {
      vector<string> newlinesplit;
      newlinesplit = StringSplit(buffer, "\n");
      for (int i = 0; i < newlinesplit.size(); i++) {
        vector<string> equalssplit;
        equalssplit = StringSplitByFirstEqualsSign(newlinesplit[i]);
        if (equalssplit.size() == 2) {
          if (equalssplit[0] == name) {
            *value = equalssplit[1];
          }
        }
      }
    }
  }
  #else
  string input; char buffer[BUFSIZ]; ssize_t nRead = 0;
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0); if (-1 == flags) return;
  while ((nRead = read(fcntl(STDIN_FILENO, F_SETFL, flags | 
    O_NONBLOCK), buffer, BUFSIZ)) > 0) {
    buffer[nRead] = '\0';
    input.append(buffer, nRead);
  }
  vector<string> newlinesplit;
  newlinesplit = StringSplit(input, "\n");
  for (int i = 0; i < newlinesplit.size(); i++) {
    vector<string> equalssplit;
    equalssplit = StringSplitByFirstEqualsSign(newlinesplit[i]);
    if (equalssplit.size() == 2) {
      if (equalssplit[0] == name) {
        *value = equalssplit[1];
      }
    }
  }
  #endif
}

void DisplayCursor(bool display) {
  #if OS_PLATFORM == OS_MACOS
  if (display) {
    CGDisplayShowCursor(kCGDirectMainDisplay);
  } else {
    CGDisplayHideCursor(kCGDirectMainDisplay);
  }
  #else
  if (display) {
    glutSetCursor(GLUT_CURSOR_INHERIT);
  } else {
    glutSetCursor(GLUT_CURSOR_NONE);
  }
  #endif
}

void UpdateEnvironmentVariables() {
  string texture; EnvironFromStdInput("PANORAMA_TEXTURE", &texture);
  string pointer; EnvironFromStdInput("PANORAMA_POINTER", &pointer);

  string direction;
  EnvironFromStdInput("PANORAMA_XANGLE", &direction);

  string zdirection;
  EnvironFromStdInput("PANORAMA_YANGLE", &zdirection);

  if (!texture.empty())
    LoadPanorama(texture.c_str());
  if (!pointer.empty()) 
    LoadCursor(pointer.c_str());

  if (!direction.empty()) {
    double xtemp = strtod(direction.c_str(), nullptr);
    if (xtemp != KEEP_XANGLE) {
      xangle = xtemp;
    }
  }

  if (!zdirection.empty()) {
    double ytemp = strtod(zdirection.c_str(), nullptr);
    if (ytemp != KEEP_YANGLE) {
      yangle = ytemp;
    }
  }
}

void display() {
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(60, 4 / 3, 0.1, 1024);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CW);
  glEnable(GL_DEPTH_TEST);
  glLoadIdentity();
  glRotatef(yangle, 1, 0, 0);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tex);
  DrawPanorama(); glFlush();

  #if OS_PLATFORM != OS_MACOS
  SDL_Rect rect; if (hidden == nullptr) { return; }
  int dpy = SDL_GetWindowDisplayIndex(hidden);
  int err = SDL_GetDisplayBounds(dpy, &rect); 
  if (err == 0) {
    glClear(GL_DEPTH_BITS);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, rect.w, rect.h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, cur);
    DrawCursor(cur, (rect.w / 2) - 16, (rect.h / 2) - 16, 32, 32);
    glBlendFunc(GL_ONE, GL_ZERO);
  }
  #else
  glClear(GL_DEPTH_BITS);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  int w = CGDisplayPixelsWide(kCGDirectMainDisplay);
  int h = CGDisplayPixelsHigh(kCGDirectMainDisplay);
  glOrtho(0, w, h, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, cur);
  DrawCursor(cur, (w / 2) - 16, (h / 2) - 16, 32, 32);
  glBlendFunc(GL_ONE, GL_ZERO);
  #endif

  glutSwapBuffers();
}

double PanoramaSetVertAngle() {
  return xangle;
}

void PanoramaSetHorzAngle(double hangle) {
  xangle -= hangle;
  if      (xangle > 360) xangle = 0;
  else if (xangle < 0) xangle = 360;
}

double PanoramaGetVertAngle() {
  return yangle;
}

double MaximumVerticalAngle;
void PanoramaSetVertAngle(double vangle) {
  yangle -= vangle;
  yangle  = std::fmin(std::fmax(yangle, -MaximumVerticalAngle), 
  MaximumVerticalAngle);
}

void WarpMouse() {
  #if OS_PLATFORM != OS_MACOS
  int mx, my; SDL_Rect rect;
  SDL_GetGlobalMouseState(&mx, &my);
  if (hidden == nullptr) { return; }
  int dpy = SDL_GetWindowDisplayIndex(hidden);
  int err = SDL_GetDisplayBounds(dpy, &rect);
  if (err == 0) {
    int dx = rect.x, hdw = rect.x + (rect.w / 2);
    int dy = rect.y, hdh = rect.y + (rect.h / 2);
    SDL_WarpMouseGlobal(hdw, hdh);
    PanoramaSetHorzAngle((hdw - mx) / 20);
    PanoramaSetVertAngle((hdh - my) / 20);
  }
  #else
  CGEventRef event = CGEventCreate(nullptr);
  CGPoint cursor = CGEventGetLocation(event);
  CFRelease(event);
  int hdw = CGDisplayPixelsWide(kCGDirectMainDisplay) / 2;
  int hdh = CGDisplayPixelsHigh(kCGDirectMainDisplay) / 2;
  CGDisplayMoveCursorToPoint(kCGDirectMainDisplay, CGPointMake(hdw, hdh));
  CGAssociateMouseAndMouseCursorPosition(true);
  PanoramaSetHorzAngle((hdw - cursor.x) / 20);
  PanoramaSetVertAngle((hdh - cursor.y) / 20);
  #endif
}

void GetTexelUnderCursor(int *TexX, int *TexY) {
  *TexX = abs(round(2 - std::fmod((xangle / 360 + 1) * TexWidth, TexWidth)));
  *TexY = TexHeight - (round(((1 - ((std::tan(yangle * PI / 180.0) * 100) / 
  (700 / AspectRatio))) * TexHeight)) - (TexHeight / 2));
}

void timer(int i) {
  AspectRatio = std::fmin(std::fmax(AspectRatio, 0.1), 6);
  MaximumVerticalAngle = (std::atan2((700 / AspectRatio) / 2, 100) * 180.0 / PI) - 30;
  WarpMouse();
  glutPostRedisplay();
  glutTimerFunc(5, timer, 0);
}

int window = 0;
void keyboard(unsigned char key, int x, int y) {
  switch (key) {
    case 27:
      glutDestroyWindow(window);
      std::cout << "Forced Quit..." << std::endl;
      exit(0);
      break;
  }
  glutPostRedisplay();
}

void mouse(int button, int state, int x, int y) {
  switch (button) {
    case GLUT_LEFT_BUTTON:
      if (state == GLUT_DOWN) {
        int TexX, TexY;
        GetTexelUnderCursor(&TexX, &TexY);
        std::cout << "Texel Clicked: " << TexX << "," << TexY << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        UpdateEnvironmentVariables();
        break;
      }
  }
  glutPostRedisplay();
}

#if OS_PLATFORM == OS_WINDOWS
BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
  DWORD dwProcessId;
  GetWindowThreadProcessId(hWnd, &dwProcessId);
  if (dwProcessId == GetCurrentProcessId()) {
    wchar_t buffer[256];
    GetWindowTextW(hWnd, buffer, 256);
    string caption = narrow(buffer);
    if (caption == "") {
      ShowWindow(hWnd, (DWORD)lParam);
    }
  }
  return true;
}
#endif

} // anonymous namespace

int main(int argc, char **argv) {
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE);

  #if OS_PLATFORM != OS_MACOS
  SDL_Init(SDL_INIT_VIDEO);
  hidden = SDL_CreateWindow("hidden",
  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
  0, 0, SDL_WINDOW_BORDERLESS);
  if (hidden != nullptr) { SDL_HideWindow(hidden); }
  #endif

  glutInitWindowPosition(0, 0);
  glutInitWindowSize(1, 1);
  window = glutCreateWindow("");

  #if OS_PLATFORM == OS_WINDOWS
  EnumWindows(&EnumWindowsProc, SW_HIDE);
  WindowID = std::to_string((std::uintptr_t)WindowFromDC(wglGetCurrentDC()));
  std::cout << "Window ID: " << WindowID << std::endl;
  #elif OS_PLATFORM == OS_MACOS
  CFArrayRef windowArray = CGWindowListCopyWindowInfo(
  kCGWindowListOptionAll, kCGNullWindowID);
  CFIndex windowCount = 0;
  if ((windowCount = CFArrayGetCount(windowArray))) {
    for (CFIndex i = 0; i < windowCount; i++) {
      CFDictionaryRef windowInfoDictionary = 
      (CFDictionaryRef)CFArrayGetValueAtIndex(windowArray, i);
      CFNumberRef ownerPID = (CFNumberRef)CFDictionaryGetValue(
      windowInfoDictionary, kCGWindowOwnerPID);
      PROCID pid; CFNumberGetValue(ownerPID, kCFNumberIntType, &pid);
      if (getpid() == pid) {
        CFNumberRef windowID = (CFNumberRef)CFDictionaryGetValue(
        windowInfoDictionary, kCGWindowNumber);
        CGWindowID CoreGraphicsWindowID; CFNumberGetValue(windowID, 
        kCGWindowIDCFNumberType, &CoreGraphicsWindowID);
        WindowID = std::to_string((std::uintptr_t)CoreGraphicsWindowID);
        std::cout << "Window ID: " << WindowID << std::endl;
        break;
      }
    }
  }
  CFRelease(windowArray);
  #elif OS_PLATFORM == OS_LINUX || OS_PLATFORM == OS_FREEBSD
  WindowID = std::to_string((std::uintptr_t)glXGetCurrentDrawable());
  std::cout << "Window ID: " << WindowID << std::endl;
  #endif

  glutDisplayFunc(display);
  glutHideWindow();
  glClearColor(0, 0, 0, 1);

  string exefile;
  #if OS_PLATFORM == OS_WINDOWS
  wchar_t exe[MAX_PATH];
  if (GetModuleFileNameW(nullptr, exe, MAX_PATH)) {
    exefile = narrow(exe);
  }
  #elif OS_PLATFORM == OS_MACOS
  char exe[PROC_PIDPATHINFO_MAXSIZE];
  if (proc_pidpath(getpid(), exe, sizeof(exe)) > 0) {
    exefile = exe;
  }
  #elif OS_PLATFORM == OS_LINUX
  char exe[PATH_MAX]; 
  if (realpath("/proc/self/exe", exe)) {
    exefile = exe;
  }
  #elif OS_PLATFORM == OS_FREEBSD
  int mib[4]; size_t s;
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;
  if (sysctl(mib, 4, nullptr, &s, nullptr, 0) == 0) {
    string str1; str1.resize(s, '\0');
    char *exe = str1.data();
    if (sysctl(mib, 4, exe, &s, nullptr, 0) == 0) {
      exefile = exe;
    }
  }
  #endif

  if (exefile.find_last_of("/\\") != string::npos)
  cwd = exefile.substr(0, exefile.find_last_of("/\\"));

  string panorama; if (argc == 1) {
  #if OS_PLATFORM == OS_WINDOWS
  dialog_module::widget_set_owner((char *)std::to_string(
  (std::uintptr_t)GetDesktopWindow()).c_str());
  #else
  dialog_module::widget_set_owner((char *)"-1");
  #endif

  XProc::DirectorySetCurrentWorking((cwd + "/examples").c_str());
  dialog_module::widget_set_icon((char *)(cwd + "/icon.png").c_str());
  panorama = dialog_module::get_open_filename_ext((char *)
  "Portable Network Graphic (*.png)|*.png;*.PNG",
  (char *)"burning_within.png", (char *)"", 
  (char *)"Choose a 360 Degree Cylindrical Panoramic Image");
  if (strcmp(panorama.c_str(), "") == 0) { 
  glutDestroyWindow(window); exit(0); } } else { panorama = argv[1]; }
  string cursor = (argc > 2) ? argv[2] : cwd + "/cursor.png";
  XProc::DirectorySetCurrentWorking(cwd.c_str());

  glClearDepth(1);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glShadeModel(GL_SMOOTH);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  LoadPanorama(panorama.c_str());
  LoadCursor(cursor.c_str());
  glutKeyboardFunc(keyboard);
  glutMouseFunc(mouse);
  glutTimerFunc(0, timer, 0);

  string str1 = XProc::EnvironmentGetVariable("PANORAMA_XANGLE");
  string str2 = XProc::EnvironmentGetVariable("PANORAMA_YANGLE");
  double initxangle = strtod((!str1.empty()) ? str1.c_str() : "0", nullptr); 
  double inityangle = strtod((!str2.empty()) ? str2.c_str() : "0", nullptr);
  
  for (size_t i = 0; i < 150; i++) {
    WarpMouse();
    xangle = initxangle;
    yangle = inityangle;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  
  glutShowWindow();
  glutFullScreen();
  #if OS_PLATFORM == OS_WINDOWS
  EnumWindows(&EnumWindowsProc, SW_SHOW);
  #endif

  DisplayCursor(false);
  glutMainLoop();
  return 0;
}
