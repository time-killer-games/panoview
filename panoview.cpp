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

#define OS_UNKNOWN -1
#define OS_WINDOWS  0
#define OS_MACOS    1
#define OS_LINUX    2
#define OS_FREEBSD  3

#if defined(_WIN32)
#define OS_PLATFORM OS_WINDOWS
#define OS_UNIXLIKE false
#elif defined(__APPLE__) && defined(__MACH__)
#define OS_PLATFORM OS_MACOS
#define OS_UNIXLIKE true
#elif defined(__linux__) && !defined(__ANDROID__)
#define OS_PLATFORM OS_LINUX
#define OS_UNIXLIKE true
#elif defined(__FreeBSD__)
#define OS_PLATFORM OS_FREEBSD
#define OS_UNIXLIKE true
#endif

#if !defined(OS_PLATFORM)
#define OS_PLATFORM OS_UNKNOWN
#define OS_UNIXLIKE false
#endif

#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <fstream>

#if defined(_WIN32)
#include <cwchar>
#endif

#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <climits>
#include <cstdio>
#include <cmath>

#include "Universal/dlgmodule.h"
#include "Universal/lodepng.h"

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
#include <GLUT/glut.h>
#else
#if OS_PLATFORM == OS_WINDOWS
#include <windows.h>
#elif OS_PLATFORM == OS_FREEBSD
#include <sys/sysctl.h>
#endif
#include <GL/glut.h>
#include <SDL2/SDL.h>
#endif

#if OS_PLATFORM == OS_WINDOWS
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#if OS_UNIXLIKE == true
typedef pid_t PROCID;
#else
typedef DWORD PROCID;
#endif

// magic numbers...
#define KEEP_XANGLE 361
#define KEEP_YANGLE -91

using std::string;
using std::to_string;
using std::wstring;
using std::vector;
using std::size_t;

namespace {

PROCID ID = 0;
const double PI = 3.141592653589793;

#if OS_PLATFORM == OS_WINDOWS
wstring widen(string str) {
  size_t wchar_count = str.size() + 1;
  vector<wchar_t> buf(wchar_count);
  return wstring { buf.data(), (size_t)MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf.data(), (int)wchar_count) };
}

string narrow(wstring wstr) {
  int nbytes = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
  vector<char> buf(nbytes);
  return string { buf.data(), (size_t)WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), buf.data(), nbytes, NULL, NULL) };
}
#endif

string StringReplaceAll(string str, string substr, string nstr) {
  size_t pos = 0;
  while ((pos = str.find(substr, pos)) != string::npos) {
    str.replace(pos, substr.length(), nstr);
    pos += nstr.length();
  }
  return str;
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

void OutputThread(void *handle, string *output) {
  #if OS_PLATFORM == OS_WINDOWS
  DWORD dwRead = 0;
  char buffer[BUFSIZ]; string result;
  while (ReadFile((HANDLE)handle, buffer, BUFSIZ, &dwRead, nullptr) && dwRead) {
    buffer[dwRead] = '\0';
    std::cout << buffer;
    result.append(buffer, dwRead);
    *(output) = result;
  }
  while (!result.empty() && (result.back() == '\r' || result.back() == '\n')) 
    result.pop_back();
  *(output) = result;
  #elif OS_UNIXLIKE == true
  ssize_t nRead = 0;
  char buffer[BUFSIZ]; string result;
  while ((nRead = read((int)(std::intptr_t)handle, buffer, BUFSIZ)) > 0) {
    buffer[nRead] = '\0';
    result.append(buffer, nRead);
    *(output) = result;
  }
  while (!result.empty() && (result.back() == '\r' || result.back() == '\n')) 
    result.pop_back();
  *(output) = result;
  #endif
}

void ProcessExecAndReadOutput(string command, string *output) {
  #if OS_PLATFORM == OS_WINDOWS
  string result; wstring wstrCommand = widen(command);
  wchar_t cwstrCommand[32768];
  wcsncpy_s(cwstrCommand, 32768, wstrCommand.c_str(), 32768);
  bool proceed = true;
  HANDLE hStdInPipeRead = nullptr;
  HANDLE hStdInPipeWrite = nullptr;
  HANDLE hStdOutPipeRead = nullptr;
  HANDLE hStdOutPipeWrite = nullptr;
  SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, true };
  proceed = CreatePipe(&hStdInPipeRead, &hStdInPipeWrite, &sa, 0);
  if (proceed == false) return;
  proceed = CreatePipe(&hStdOutPipeRead, &hStdOutPipeWrite, &sa, 0);
  if (proceed == false) return;
  STARTUPINFOW si = { 0 };
  si.cb = sizeof(STARTUPINFOW);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdError = hStdOutPipeWrite;
  si.hStdOutput = hStdOutPipeWrite;
  si.hStdInput = hStdInPipeRead;
  PROCESS_INFORMATION pi = { 0 };
  if (CreateProcessW(nullptr, cwstrCommand, nullptr, nullptr, true, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    CloseHandle(hStdOutPipeWrite);
    CloseHandle(hStdInPipeRead); MSG msg;
    HANDLE waitHandles[] = { pi.hProcess, hStdOutPipeRead };
    std::thread outthrd(OutputThread, (void *)hStdOutPipeRead, output);
    while (MsgWaitForMultipleObjects(2, waitHandles, false, 5, QS_ALLEVENTS) != WAIT_OBJECT_0) {
      while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
    outthrd.join();
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdOutPipeRead);
    CloseHandle(hStdInPipeWrite);
  }
  #elif OS_UNIXLIKE == true
  int infp, outfp;
  int p_stdin[2];
  int p_stdout[2];
  if (pipe(p_stdin) == -1)
    return;
  if (pipe(p_stdout) == -1) {
    close(p_stdin[0]);
    close(p_stdin[1]);
    return;
  }
  PROCID pid = fork();
  if (pid < 0) {
    close(p_stdin[0]);
    close(p_stdin[1]);
    close(p_stdout[0]);
    close(p_stdout[1]);
    return;
  } else if (pid == 0) {
    close(p_stdin[1]);
    dup2(p_stdin[0], 0);
    close(p_stdout[0]);
    dup2(p_stdout[1], 1);
    dup2(open("/dev/null", O_RDONLY), 2);
    for (int i = 3; i < 4096; i++)
      close(i);
    setsid();
    execl("/bin/sh", "/bin/sh", "-c", command.c_str(), nullptr);
    exit(0);
  }
  close(p_stdin[0]);
  close(p_stdout[1]);
  infp = p_stdin[1];
  outfp = p_stdout[0];
  std::thread outthrd(OutputThread, (void *)(std::intptr_t)outfp, output);
  outthrd.join();
  #endif
}

string EnvironmentGetVariable(string name) {
  #if OS_PLATFORM == OS_WINDOWS
  wchar_t buffer[32767];
  wstring u8name = widen(name);
  if (GetEnvironmentVariableW(u8name.c_str(), buffer, 32767) != 0) {
    return narrow(buffer);
  }
  return "";
  #else
  char *value = getenv(name.c_str());
  return value ? value : "";
  #endif
}

bool EnvironmentSetVariable(string name, string value) {
  #if OS_PLATFORM == OS_WINDOWS
  wstring u8name = widen(name);
  wstring u8value = widen(value);
  if (strcmp(value.c_str(), "") == 0) return (SetEnvironmentVariableW(u8name.c_str(), nullptr) != 0);
  return (SetEnvironmentVariableW(u8name.c_str(), u8value.c_str()) != 0);
  #else
  if (strcmp(value.c_str(), "") == 0) return (unsetenv(name.c_str()) == 0);
  return (setenv(name.c_str(), value.c_str(), 1) == 0);
  #endif
}

string DirectoryGetCurrentWorking() {
  #if OS_PLATFORM == OS_WINDOWS
  wchar_t u8dname[MAX_PATH];
  if (GetCurrentDirectoryW(MAX_PATH, u8dname) != 0) {
	return narrow(u8dname);
  }
  return "";
  #else
  char dname[PATH_MAX];
  if (getcwd(dname, sizeof(dname)) != nullptr) {
    return dname;
  }
  return "";
  #endif
}

bool DirectorySetCurrentWorking(string dname) {
  #if OS_PLATFORM == OS_WINDOWS
  wstring u8dname = widen(dname);
  return SetCurrentDirectoryW(u8dname.c_str());
  #else
  return chdir(dname.c_str());
  #endif
}

void LoadImage(unsigned char **out, unsigned *pngwidth, unsigned *pngheight, const char *fname) {
  unsigned char *data = nullptr;
  unsigned error = lodepng_decode32_file(&data, pngwidth, pngheight, fname);
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

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pngwidth, pngheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  EnvironmentSetVariable("PANORAMA_TEXTURE", fname);
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

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pngwidth, pngheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  EnvironmentSetVariable("PANORAMA_POINTER", fname);
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

void ProcessEnvirnomentVariables() {
  if (ID != 0) {
    string panorama1 = EnvironmentGetVariable("PANORAMA_TEXTURE");
    string panorama2; ProcessExecAndReadOutput("xproc --env-from-pid " + 
      std::to_string(ID) + " PANORAMA_TEXTURE", &panorama2);
    panorama2 = StringReplaceAll(panorama2, "\\\"", "\"");
	if (panorama2.length() >= 2) {
      panorama2 = panorama2.substr(1, panorama2.length() - 2);
    }
	
    string cursor1 = EnvironmentGetVariable("PANORAMA_POINTER");
    string cursor2; ProcessExecAndReadOutput("xproc --env-from-pid " + 
      std::to_string(ID) + " PANORAMA_POINTER", &cursor2);
    cursor2 = StringReplaceAll(cursor2, "\\\"", "\"");
    if (cursor2.length() >= 2) {
      cursor2 = cursor2.substr(1, cursor2.length() - 2);
    }

    string direction1 = EnvironmentGetVariable("PANORAMA_XANGLE");
    string direction2; ProcessExecAndReadOutput("xproc --env-from-pid " + 
      std::to_string(ID) + " PANORAMA_XANGLE", &direction2);
    direction2 = StringReplaceAll(direction2, "\\\"", "\"");
    if (direction2.length() >= 2) {
      direction2 = direction2.substr(1, direction2.length() - 2);
    }

    string zdirection1 = EnvironmentGetVariable("PANORAMA_YANGLE");
    string zdirection2; ProcessExecAndReadOutput("xproc --env-from-pid " + 
      std::to_string(ID) + " PANORAMA_YANGLE", &zdirection2);
    zdirection2 = StringReplaceAll(zdirection2, "\\\"", "\"");
    if (zdirection2.length() >= 2) {
      zdirection2 = zdirection2.substr(1, zdirection2.length() - 2);
    }

    if (!panorama2.empty() && panorama1 != panorama2)
      LoadPanorama(panorama2.c_str());
    if (!cursor2.empty() && cursor1 != cursor2) 
      LoadCursor(cursor2.c_str());

    if (!direction2.empty()) {
      double xtemp = strtod(direction2.c_str(), nullptr);
      if (xtemp != KEEP_XANGLE) {
        EnvironmentSetVariable("PANORAMA_XANGLE", direction2);
        xangle = xtemp;
      }
    }

    if (!zdirection2.empty()) {
      double ytemp = strtod(zdirection2.c_str(), nullptr);
      if (ytemp != KEEP_YANGLE) {
        EnvironmentSetVariable("PANORAMA_YANGLE", zdirection2);
        yangle = ytemp;
      }
    }
  }
}

bool clicked = false;
void display() {
  if (clicked == true) {
    ProcessEnvirnomentVariables();
    clicked = false;
  }

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
  yangle  = std::fmin(std::fmax(yangle, -MaximumVerticalAngle), MaximumVerticalAngle);
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
  CGSetLocalEventsSuppressionInterval(0.0);
  PanoramaSetHorzAngle((hdw - cursor.x) / 20);
  PanoramaSetVertAngle((hdh - cursor.y) / 20);
  #endif
}

void GetTexelUnderCursor(int *TexX, int *TexY) {
  *TexX = abs(round(2 - std::fmod((xangle / 360 + 1) * TexWidth, TexWidth)));
  *TexY = TexHeight - (round(((1 - ((std::tan(yangle * PI / 180.0) * 100) / (700 / AspectRatio))) * TexHeight)) - (TexHeight / 2));
}

void timer(int i) {
  AspectRatio = std::fmin(std::fmax(AspectRatio, 0.1), 6);
  MaximumVerticalAngle = (std::atan2((700 / AspectRatio) / 2, 100) * 180.0 / PI) - 30;
  WarpMouse();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
        clicked = true;
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
  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0, SDL_WINDOW_BORDERLESS);
  if (hidden != nullptr) { SDL_HideWindow(hidden); }
  #endif

  if (window != 0) glutDestroyWindow(window);
  glutInitWindowPosition(0, 0);
  glutInitWindowSize(1, 1);
  window = glutCreateWindow("");
  #if OS_PLATFORM == OS_WINDOWS
  EnumWindows(&EnumWindowsProc, SW_HIDE);
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

  string cwd; if (exefile.find_last_of("/\\") != string::npos)
  cwd = exefile.substr(0, exefile.find_last_of("/\\"));

  string panorama; if (argc == 1) {
  #if OS_PLATFORM == OS_WINDOWS
  dialog_module::widget_set_owner((char *)std::to_string((std::uintptr_t)GetDesktopWindow()).c_str());
  #else
  dialog_module::widget_set_owner((char *)"-1");
  #endif

  DirectorySetCurrentWorking(cwd + "/examples");
  dialog_module::widget_set_icon((char *)(cwd + "/icon.png").c_str());
  panorama = dialog_module::get_open_filename_ext((char *)"Portable Network Graphic (*.png)|*.png;*.PNG",
  (char *)"burning_within.png", (char *)"", (char *)"Choose a 360 Degree Cylindrical Panoramic Image");
  if (strcmp(panorama.c_str(), "") == 0) { glutDestroyWindow(window); exit(0); } } else { panorama = argv[1]; }
  string cursor = (argc > 2) ? argv[2] : "cursor.png";
  DirectorySetCurrentWorking(cwd);

  glClearDepth(1);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glShadeModel(GL_SMOOTH);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  LoadPanorama(panorama.c_str());
  LoadCursor(cursor.c_str());

  glutSetCursor(GLUT_CURSOR_NONE);
  glutKeyboardFunc(keyboard);
  glutMouseFunc(mouse);
  glutTimerFunc(0, timer, 0);
  
  string firstline;
  std::ifstream conf("panoview.conf");
  if (conf.is_open()) {
    if (std::getline(conf, firstline)) {
      ID = (PROCID)strtoul(StringReplaceAll(firstline, "ID=", "").c_str(), nullptr, 10);
      ProcessEnvirnomentVariables();
    }
    conf.close();
  }

  string str1 = EnvironmentGetVariable("PANORAMA_XANGLE");
  string str2 = EnvironmentGetVariable("PANORAMA_YANGLE");
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
  #elif OS_PLATFORM == OS_MACOS
  CGDisplayHideCursor(kCGDirectMainDisplay);
  #endif
  
  glutMainLoop();
  return 0;
}
