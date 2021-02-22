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

#include <cmath>
#include <cstring>
#include <climits>
#if defined(_WIN32)
#include <cwchar>
#endif

#include <string>
#include <thread>
#include <chrono>
#if defined(_WIN32)
#include <vector>
#endif

#include "Universal/dlgmodule.h"
#include "Universal/lodepng.h"

#include <SDL2/SDL.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__APPLE__) && defined(__MACH__)
#include <GLUT/glut.h>
#include <libproc.h>
#else
#if defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif
#include <GL/glut.h>
#endif

namespace {

#if defined(_WIN32)
std::wstring widen(std::string str) {
  std::size_t wchar_count = str.size() + 1;
  std::vector<wchar_t> buf(wchar_count);
  return std::wstring { buf.data(), (std::size_t)MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf.data(), (int)wchar_count) };
}

std::string narrow(std::wstring wstr) {
  int nbytes = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
  std::vector<char> buf(nbytes);
  return std::string { buf.data(), (std::size_t)WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), buf.data(), nbytes, NULL, NULL) };
}
#endif

std::string ExecutableParentDirectory() {
  std::string fname;
  #if defined(_WIN32)
  wchar_t exe[MAX_PATH];
  if (GetModuleFileNameW(NULL, buffer, MAX_PATH) != 0) {
    fname = narrow(exe);
  }
  #elif defined(__APPLE__) && defined(__MACH__)
  char exe[PROC_PIDPATHINFO_MAXSIZE];
  if (proc_pidpath(getpid(), exe, sizeof(exe)) > 0) {
    fname = exe;
  }
  #elif defined(__linux__) && !defined(__ANDROID__)
  char exe[PATH_MAX];
  std::string symLink = "/proc/self/exe";
  if (realpath(symLink.c_str(), exe)) {
    fname = exe;
  }
  #elif defined(__FreeBSD__)
  int mib[4]; std::size_t s;
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;
  if (sysctl(mib, 4, nullptr, &s, nullptr, 0) == 0) {
    std::string str; str.resize(s, '\0');
    char *exe = str.data();
    if (sysctl(mib, 4, exe, &s, nullptr, 0) == 0) {
      fname = exe;
    }
  }
  #endif
  std::size_t fpos = fname.find_last_of("/\\");
  return fname.substr(0, fpos + 1);
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
void LoadPanorama(const char *fname) {
  unsigned char *data = nullptr;
  unsigned pngwidth, pngheight;
  LoadImage(&data, &pngwidth, &pngheight, fname);

  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pngwidth, pngheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  delete[] data;
}

float xangle = 0.0f;
float yangle = 0.0f;
void DrawPanorama() {
  const double PI = 3.141592653589793;
  double i, resolution  = 0.3141592653589793;
  double height = 3.5, radius = 0.5;

  glPushMatrix(); glRotatef(90 + xangle, 0, 90 + 0.5, 0);
  glTranslatef(0, -1.75, 0); glBegin(GL_TRIANGLE_FAN);

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

SDL_Window *hidden = nullptr;
void display() {
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(90.0f, 16 / 9, 0.1f, 100.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0, 0, 0);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CW);
  glEnable(GL_DEPTH_TEST);
  glLoadIdentity();
  glRotatef(yangle, 1, 0, 0);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tex);
  DrawPanorama(); glFlush();

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

  glutSwapBuffers();
}

double PanoramaSetVertAngle() {
  return xangle;
}

void PanoramaSetHorzAngle(double hangle) {
  xangle -= hangle;
}

double PanoramaGetVertAngle() {
  return yangle;
}

void PanoramaSetVertAngle(double vangle) {
  yangle -= vangle;
  yangle  = ((yangle >= -28) ? yangle : -28);
  yangle  = ((yangle <=  28) ? yangle :  28);
}

void WarpMouse() {
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
}

void timer(int i) {
  WarpMouse();
  glutPostRedisplay();
  glutTimerFunc(5, timer, 0);
}

int window = 0;
void keyboard(unsigned char key, int x, int y) {
  switch (key) {
    case 27: glutDestroyWindow(window); exit(0); break;
  }
  glutPostRedisplay();
}

} // anonymous namespace

int main(int argc, char **argv) {
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE);

  SDL_Init(SDL_INIT_VIDEO);
  hidden = SDL_CreateWindow("hidden",
  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0, 0);
  if (hidden != nullptr) { SDL_HideWindow(hidden); }
  if (window != 0) glutDestroyWindow(window);
  window = glutCreateWindow("");
  glutDisplayFunc(display);
  glutHideWindow();

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  std::string parpath = ExecutableParentDirectory();
  std::string panorama; if (argc == 1) {
  dialog_module::widget_set_owner((char *)"-1");
  std::string dir = parpath + "examples";

  chdir(dir.c_str()); dialog_module::widget_set_icon((char *)(parpath + "icon.png").c_str());
  panorama = dialog_module::get_open_filename_ext((char *)"Portable Network Graphic (*.png)|*.png;*.PNG",
  (char *)"burning_within.png", (char *)"", (char *)"Choose a 360 Degree Cylindrical Panoramic Image");
  if (strcmp(panorama.c_str(), "") == 0) { glutDestroyWindow(window); exit(0); } } else { panorama = argv[1]; }
  std::string defcursor = (argc > 2) ? argv[2] : parpath + "defcursor.png";

  glClearDepth(1.0f);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glShadeModel(GL_SMOOTH);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  LoadPanorama(panorama.c_str());
  LoadCursor(defcursor.c_str());

  glutSetCursor(GLUT_CURSOR_NONE);
  glutKeyboardFunc(keyboard);
  glutTimerFunc(0, timer, 0);

  for (std::size_t i = 0; i < 100; i++) {
    WarpMouse(); xangle = 0; yangle = 0;
    std::this_thread::sleep_for (std::chrono::milliseconds(5));
  }

  glutShowWindow();
  glutFullScreen();
  glutMainLoop();
  return 0;
}