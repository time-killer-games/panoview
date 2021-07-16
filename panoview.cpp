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

#include "Universal/crossprocess.h"
#include "Universal/dlgmodule.h"
#if defined(_WIN32)
#include "Win32/libpng-util.h"
#elif !defined(_WIN32)
#if defined(__APPLE__) && defined(__MACH__)
#include "MacOSX/objcpp.h"
#else
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xinerama.h>
#endif
#include "Unix/lodepng.h"
#endif

#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#if (defined(__APPLE__) && defined(__MACH__))
#include <libproc.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <GLUT/glut.h>
#else
#if defined(_WIN32)
#include <windows.h>
#elif (defined(__linux__) && !defined(__ANDROID__))
#include <GL/glx.h>
#elif defined(__FreeBSD__)
#include <sys/sysctl.h>
#include <GL/glx.h>
#endif
#include <GL/glut.h>
#endif

#if defined(_WIN32)
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

typedef string wid_t;

namespace {

#if defined(X_PROTOCOL)
Display *display = nullptr;
int displayX            = -1;
int displayY            = -1;
int displayWidth        = -1;
int displayHeight       = -1;
int displayXGetter      = -1;
int displayYGetter      = -1;
int displayWidthGetter  = -1;
int displayHeightGetter = -1;
#endif

string cwd;
wid_t windowId  = "-1"; 
const double PI = 3.141592653589793;

#if defined(_WIN32)
wstring widen(string str) {
  size_t wchar_count = str.size() + 1;
  vector<wchar_t> buf(wchar_count);
  return wstring { buf.data(), (size_t)MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf.data(), (int)wchar_count) };
}

string narrow(wstring wstr) {
  int nbytes = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), nullptr, 0, nullptr, nullptr);
  vector<char> buf(nbytes);
  return string { buf.data(), (size_t)WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), buf.data(), nbytes, nullptr, nullptr) };
}
#endif

void LoadImage(unsigned char **out, unsigned *pngwidth, unsigned *pngheight, 
  const char *fname) {
  unsigned char *data = nullptr;
  #if defined(_WIN32)
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

vector<string> StringSplitByFirstEqualsSign(string str) {
  size_t pos = 0;
  vector<string> vec;
  if ((pos = str.find_first_of("=")) != string::npos) {
    vec.push_back(str.substr(0, pos));
    vec.push_back(str.substr(pos + 1));
  }
  return vec;
}

vector<string> StringSplit(string str, string delim) {
  vector<string> vec;
  std::stringstream sstr(str);
  string tmp;
  while (std::getline(sstr, tmp, delim[0]))
    vec.push_back(tmp);
  return vec;
}

string StringReplaceAll(string str, string substr, string nstr) {
  size_t pos = 0;
  while ((pos = str.find(substr, pos)) != string::npos) {
    str.replace(pos, substr.length(), nstr);
    pos += nstr.length();
  }
  return str;
}

#if defined(_WIN32)
void window_get_size_from_id(CrossProcess::WINDOWID winId, int *width, int *height) {
  RECT rc; GetClientRect((HWND)(void *)strtoull(winId, nullptr, 10), &rc);
  *width = rc.right; *height = rc.bottom;
}
CrossProcess::PROCID parentProcId = 0;
void window_id_set_parent_window_id(CrossProcess::WINDOWID wid, CrossProcess::WINDOWID pwid) {
  HWND child = (HWND)(void *)strtoull(wid, nullptr, 10);
  HWND parent = (HWND)(void *)strtoull(pwid, nullptr, 10);
  SetParent(child, (HWND)(void *)strtoull(pwid, nullptr, 10));
  int width, height; window_get_size_from_id(pwid, &width, &height);
  SetWindowLongPtr(child, GWL_STYLE, GetWindowLongPtr(child, GWL_STYLE) & ~(WS_CAPTION | WS_SIZEBOX));
  SetWindowLongPtr(parent, GWL_STYLE, GetWindowLongPtr(parent, GWL_STYLE) | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
  MoveWindow(child, 0, 0, width, height, true);
  if (!parentProcId) CrossProcess::ProcIdFromWindowId(pwid, &parentProcId);
  if (parentProcId && !CrossProcess::ProcIdExists(parentProcId)) exit(0);
}
#elif defined(X_PROTOCOL)
typedef struct {
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long inputMode;
  unsigned long status;
} Hints;
void window_get_size_from_id(CrossProcess::WINDOWID winId, int *width, int *height) {
  Window window = strtoull(winId, nullptr, 10);
  Window r = 0; int x = 0, y = 0; unsigned w = 0, h = 0, b = 0, d = 0; 
  XGetGeometry(display, (Drawable)window, &r, &x, &y, &w, &h, &b, &d);
  *width = w; *height = h;
}
void window_id_set_parent_window_id(CrossProcess::WINDOWID wid, CrossProcess::WINDOWID pwid) {
  Hints hints;
  Atom property = XInternAtom(display, "_MOTIF_WM_HINTS", false);
  hints.flags = 2; hints.decorations = 0;
  Window child = strtoull(wid, nullptr, 10);
  Window parent = strtoull(pwid, nullptr, 10);
  XChangeProperty(display, child, property, property, 32, PropModeReplace, (unsigned char *)&hints, 5);
  XReparentWindow(display, child, parent, 0, 0);
  int width = 0, height = 0; window_get_size_from_id(wid, &width, &height);
  XResizeWindow(display, strtoull(wid, nullptr, 10), width, height);
}
#endif
#if defined(_WIN32) || defined(X_PROTOCOL)
int window_get_width_from_id(CrossProcess::WINDOWID winId) {
  int width, height;
  window_get_size_from_id(winId, &width, &height);
  return width;
}
int window_get_height_from_id(CrossProcess::WINDOWID winId) {
  int width, height;
  window_get_size_from_id(winId, &width, &height);
  return height;
}
#endif

void EnvironFromStdInput(string name, string *value) {
  #if defined(_WIN32)
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
  #if (defined(__APPLE__) && defined(__MACH__))
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

#if defined(X_PROTOCOL)
void DisplayGetPosition(bool i, int *result) {
  *result = 0; Rotation original_rotation; 
  Window root = XDefaultRootWindow(display);
  XRRScreenConfiguration *conf = XRRGetScreenInfo(display, root);
  SizeID original_size_id = XRRConfigCurrentConfiguration(conf, &original_rotation);
  if (XineramaIsActive(display)) {
    int m = 0; XineramaScreenInfo *xrrp = XineramaQueryScreens(display, &m);
    if (!i) *result = xrrp[original_size_id].x_org;
    else if (i) *result = xrrp[original_size_id].y_org;
    XFree(xrrp);
  }
}

void DisplayGetSize(bool i, int *result) {
  *result = 0; int num_sizes; Rotation original_rotation; 
  Window root = XDefaultRootWindow(display);
  int screen = XDefaultScreen(display);
  XRRScreenConfiguration *conf = XRRGetScreenInfo(display, root);
  SizeID original_size_id = XRRConfigCurrentConfiguration(conf, &original_rotation);
  if (XineramaIsActive(display)) {
    XRRScreenSize *xrrs = XRRSizes(display, screen, &num_sizes);
    if (!i) *result = xrrs[original_size_id].width;
    else if (i) *result = xrrs[original_size_id].height;
  } else if (!i) *result = XDisplayWidth(display, screen);
  else if (i) *result = XDisplayHeight(display, screen);
}

int DisplayGetX() {
  if (displayXGetter == displayX && displayX != -1)
    return displayXGetter;
  DisplayGetPosition(false, &displayXGetter);
  int result = displayXGetter;
  displayX = result;
  return result;
}

int DisplayGetY() { 
  if (displayYGetter == displayY && displayY != -1)
    return displayYGetter;
  DisplayGetPosition(true, &displayYGetter);
  int result = displayYGetter;
  displayY = result;
  return result;
}

int DisplayGetWidth() {
  if (displayWidthGetter == displayWidth && displayWidth != -1) 
    return displayWidthGetter;
  DisplayGetSize(false, &displayWidthGetter);
  int result = displayWidthGetter;
  displayWidth = result;
  return result;
}

int DisplayGetHeight() {
  if (displayHeightGetter == displayHeight && displayHeight != -1)
    return displayHeightGetter;
  DisplayGetSize(true, &displayHeightGetter);
  int result = displayHeightGetter;
  displayHeight = result;
  return result;
}
#endif

void DisplayGraphics() {
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
  glClear(GL_DEPTH_BITS);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  int ww = window_get_width_from_id((CrossProcess::WINDOWID)windowId.c_str());
  int wh = window_get_height_from_id((CrossProcess::WINDOWID)windowId.c_str());
  glOrtho(0, ww, wh, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, cur);
  DrawCursor(cur, (ww / 2) - 16, (wh / 2) - 16, 32, 32);
  glBlendFunc(GL_ONE, GL_ZERO);
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

void WarpMouse(int x, int y) {
  #ifdef _WIN32
  SetCursorPos(x, y);
  #elif defined(__APPLE__) && defined(__MACH__)
  CGDisplayMoveCursorToPoint(kCGDirectMainDisplay, CGPointMake(x, y));
  CGAssociateMouseAndMouseCursorPosition(true);
  #else
  Window root = XDefaultRootWindow(display);
  XWarpPointer(display, None, root, 0, 0, 0, 0, x, y);
  XFlush(display);
  #endif
}

void ScreenGetCenter(int *hdw, int *hdh) {
  #if defined(_WIN32)
  *hdw = GetSystemMetrics(SM_CXSCREEN) / 2;
  *hdh = GetSystemMetrics(SM_CYSCREEN) / 2;
  #elif defined(__APPLE__) && defined(__MACH__)
  *hdw = CGDisplayPixelsWide(kCGDirectMainDisplay) / 2;
  *hdh = CGDisplayPixelsHigh(kCGDirectMainDisplay) / 2;
  #else
  *hdw = DisplayGetX() + (DisplayGetWidth() / 2);
  *hdh = DisplayGetY() + (DisplayGetHeight() / 2); 
  #endif
}

void MouseGetPosition(int *mx, int *my) {
  #if defined(_WIN32)
  POINT mouse = { 0 }; GetCursorPos(&mouse);
  *mx = (int)mouse.x; *my = (int)mouse.y;
  #elif defined(__APPLE__) && defined(__MACH__)
  CGEventRef event = CGEventCreate(nullptr);
  CGPoint mouse = CGEventGetLocation(event);
  *mx = mouse.x; *my = mouse.y;
  CFRelease(event);
  #else
  unsigned m; int rx, ry; 
  Window root = XDefaultRootWindow(display), r, c; 
  XQueryPointer(display, root, &r, &c, &rx, &ry, 
  mx, my, &m);
  #endif
}

void UpdateMouseLook() {
  int hdw, hdh, mx, my;
  ScreenGetCenter(&hdw, &hdh); 
  MouseGetPosition(&mx, &my); 
  WarpMouse(hdw, hdh);
  PanoramaSetHorzAngle((hdw - mx) / 20);
  PanoramaSetVertAngle((hdh - my) / 20);
}

void GetTexelUnderCursor(int *TexX, int *TexY) {
  *TexX = abs(round(2 - std::fmod((xangle / 360 + 1) * TexWidth, TexWidth)));
  *TexY = TexHeight - (round(((1 - ((std::tan(yangle * PI / 180.0) * 100) / 
  (700 / AspectRatio))) * TexHeight)) - (TexHeight / 2));
}

void timer(int i) {
  string str = CrossProcess::EnvironmentGetVariable("WINDOWID");
  if (str.empty()) str = "0";
  if (windowId == "-1") {
    #if defined(_WIN32)
    HWND handle = WindowFromDC(wglGetCurrentDC());
    ShowWindow(handle, SW_HIDE);
    windowId = std::to_string((unsigned long long)(void *)handle);
    std::cout << "Window ID: " << windowId << std::endl;
    #elif (defined(__APPLE__) && defined(__MACH__))
    CrossProcess::WINDOWID *wid = nullptr; int widsize = 0;
    CrossProcess::WindowIdFromProcId(getpid(), &wid, &widsize);
    if (wid) { 
      if (widsize) { 
        for (int i = 0; i < widsize; i++) { 
          const char *title = window_id_set_parent_window_id(wid[i], (char *)str.c_str());
          if (title != nullptr && strcmp(title, "") == 0) {
            windowId = wid[i];
          } 
        }
      }
      CrossProcess::FreeWindowId(wid);
    } 
    std::cout << "Window ID: " << windowId << std::endl; 
    #elif (defined(__linux__) && !defined(__ANDROID__)) || defined(__FreeBSD__)
    windowId = std::to_string((unsigned long)glXGetCurrentDrawable());
    std::cout << "Window ID: " << windowId << std::endl;
    #endif
  }
  if (CrossProcess::WindowIdExists((char *)str.c_str()) && str != "0")
  window_id_set_parent_window_id((char *)windowId.c_str(), (char *)str.c_str());
  AspectRatio = std::fmin(std::fmax(AspectRatio, 0.1), 6);
  MaximumVerticalAngle = (std::atan2((700 / AspectRatio) / 2, 100) * 180.0 / PI) - 30;
  UpdateMouseLook();
  glutPostRedisplay();
  glutTimerFunc(5, timer, 0);
}

int window = 0;
void keyboard(unsigned char key, int x, int y) {
  switch (key) {
    case 27:
      glutDestroyWindow(window);
      std::cout << "Forced Quit..." << std::endl;
      #if defined(X_PROTOCOL)
      XCloseDisplay(display);
      #endif
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

} // anonymous namespace

int main(int argc, char **argv) {
  #if defined(__APPLE__) && defined(__MACH__)
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE);
  int hdw = 0, hdh = 0;
  ScreenGetCenter(&hdw, &hdh);
  glutInitWindowSize(640, 480);
  glutInitWindowPosition(hdw - 320, hdh - 240);
  window = glutCreateWindow("");
  glutDisplayFunc(DisplayGraphics);
  glutHideWindow();
  setpolicy();
  #endif
  CrossProcess::PROCID pid; 
  string exefile; char *exe = nullptr;
  CrossProcess::ProcIdFromSelf(&pid);
  CrossProcess::ExeFromProcId(pid, &exe);
  exefile = exe;
  if (exefile.find_last_of("/\\") != string::npos)
  cwd = exefile.substr(0, exefile.find_last_of("/\\"));
  string panorama; if (argc == 1) {
  #if defined(_WIN32)
  dialog_module::widget_set_owner((char *)std::to_string(
  (std::uintptr_t)GetDesktopWindow()).c_str());
  #else
  dialog_module::widget_set_owner((char *)"-1");
  #endif
  CrossProcess::DirectorySetCurrentWorking((cwd + "/examples").c_str());
  dialog_module::widget_set_icon((char *)(cwd + "/icon.png").c_str());
  panorama = dialog_module::get_open_filename_ext((char *)
  "Portable Network Graphic (*.png)|*.png;*.PNG",
  (char *)"burning_within.png", (char *)(cwd + "/examples").c_str(), 
  (char *)"Choose a 360 Degree Cylindrical Panoramic Image");
  if (strcmp(panorama.c_str(), "") == 0) { 
  glutDestroyWindow(window); 
  #if defined(X_PROTOCOL)
  XCloseDisplay(display);
  #endif
  exit(0); } } else { panorama = argv[1]; }
  string cursor = (argc > 2) ? argv[2] : cwd + "/cursor.png";
  CrossProcess::DirectorySetCurrentWorking(cwd.c_str());
  #if defined(X_PROTOCOL)
  display = XOpenDisplay(nullptr);
  #endif
  #if !defined(__APPLE__) && !defined(__MACH__)
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE);
  int hdw = 0, hdh = 0;
  ScreenGetCenter(&hdw, &hdh);
  glutInitWindowSize(640, 480);
  glutInitWindowPosition(hdw - 320, hdh - 240);
  window = glutCreateWindow("");
  glutDisplayFunc(DisplayGraphics);
  glutHideWindow();
  #endif
  glClearColor(0, 0, 0, 1);
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
  string str1 = CrossProcess::EnvironmentGetVariable("PANORAMA_XANGLE");
  string str2 = CrossProcess::EnvironmentGetVariable("PANORAMA_YANGLE");
  double initxangle = strtod((!str1.empty()) ? str1.c_str() : "0", nullptr); 
  double inityangle = strtod((!str2.empty()) ? str2.c_str() : "0", nullptr);
  for (size_t i = 0; i < 150; i++) {
    UpdateMouseLook();
    xangle = initxangle;
    yangle = inityangle;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  glutShowWindow();
  #if defined(_WIN32)
  ShowWindow((HWND)(void *)strtoull(windowId.c_str(), nullptr, 10), SW_SHOW);
  #endif
  DisplayCursor(false);
  glutMainLoop();
  #if defined(X_PROTOCOL)
  XCloseDisplay(display);
  #endif
  return 0;
}
