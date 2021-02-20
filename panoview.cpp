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

#include <string>

#include "dlgmodule.h"
#include "lodepng.h"

#include <unistd.h>
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

float angleX = 0.0f, xr = 0.0f;
void CreateCylinder() {
  const double PI = 3.141592653589793;
  double i, resolution  = 0.3141592653589793;
  double height = 3.5;
  double radius = 0.5;
  glPushMatrix();
  glRotatef(90 + xr, 0, 90 + 2, 0);
  glTranslatef(0, -1.75, 0);
  glBegin(GL_TRIANGLE_FAN);
  glTexCoord2f(0.5, 0.5);
  glVertex3f(0, height, 0);
  for (i = 2 * PI; i >= 0; i -= resolution) {
    glTexCoord2f(0.5f * cos(i) + 0.5f, 0.5f * sin(i) + 0.5f);
    glVertex3f(radius * cos(i), height, radius * sin(i));
  }
  glTexCoord2f(0.5, 0.5);
  glVertex3f(radius, height, 0);
  glEnd();
  glBegin(GL_TRIANGLE_FAN);
  glTexCoord2f(0.5, 0.5);
  glVertex3f(0, 0, 0);
  for (i = 0; i <= 2 * PI; i += resolution) {
    glTexCoord2f(0.5f * cos(i) + 0.5f, 0.5f * sin(i) + 0.5f);
    glVertex3f(radius * cos(i), 0, radius * sin(i));
  }
  glEnd();
  glBegin(GL_QUAD_STRIP);
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
  glEnd();
  glPopMatrix();
}

std::string exeParentDirectory() {
  std::string fname;
  #if defined(__APPLE__) && defined(__MACH__)
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
  int mib[4]; size_t s;
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
  size_t fpos = fname.find_last_of("/");
  return fname.substr(0, fpos + 1);
}

GLuint tex;
void CreateTexture(const char *fname) {
  unsigned char *data = nullptr;
  unsigned pngwidth, pngheight;
  unsigned error = lodepng_decode32_file(&data, &pngwidth, &pngheight, fname);
  if (error) return;
  const int size = pngwidth * pngheight * 4;
  unsigned char *buffer = new unsigned char[size]();
  for (unsigned i = 0; i < pngheight; i++) {
    for (unsigned j = 0; j < pngwidth; j++) {
      unsigned oldPos = (pngheight - i - 1) * (pngwidth * 4) + 4 * j;
      unsigned newPos = i * (pngwidth * 4) + 4 * j;
      buffer[newPos + 0] = data[oldPos + 0];
      buffer[newPos + 1] = data[oldPos + 1];
      buffer[newPos + 2] = data[oldPos + 2];
      buffer[newPos + 3] = data[oldPos + 3];
    }
  }
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA, pngwidth, pngheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
  delete[] buffer;
  delete[] data;
}

void display() {
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(90.0f, 16/9, 0.1f, 100.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glTranslatef(0, 0, 0);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CW);
  glEnable(GL_DEPTH_TEST);
  glLoadIdentity();
  glRotatef(angleX, 2, 0, 0);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, tex);
  CreateCylinder();
  glFlush();
  glutSwapBuffers();
}

void special(int key, int x, int y) {
  switch (key) {
    case GLUT_KEY_RIGHT: xr += 2.0f; break;
    case GLUT_KEY_LEFT:  xr -= 2.0f; break;
    case GLUT_KEY_UP:    angleX -= (angleX > -28) ? 2.0f : 0; break;
    case GLUT_KEY_DOWN:  angleX += (angleX <  28) ? 2.0f : 0; break;
  }
  glutPostRedisplay();
}

int window = 0;
void keyboard(unsigned char key, int x, int y) {
  switch (key) {
    case 27: glutDestroyWindow(window); exit(0); break;
  }
  glutPostRedisplay();
}

int main(int argc, char **argv) {
  const char *fname;
  if (argc == 1) {
    dialog_module::widget_set_owner((char *)"-1");
    std::string dir = exeParentDirectory() + "examples";
    chdir(dir.c_str()); dialog_module::widget_set_icon((char *)(dir + "/../icon.png").c_str());
    fname = dialog_module::get_open_filename_ext((char *)"Portable Network Graphic (*.png)|*.png;*.PNG",
      (char *)"burning_within.png", (char *)"", (char *)"Choose a 360 Degree Cylindrical Panoramic Image");
    if (strcmp(fname, "") == 0) { exit(0); }
  } else {
    fname = argv[1];
  }
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE);
  window = glutCreateWindow("");
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClearDepth(1.0f);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glShadeModel(GL_SMOOTH);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  CreateTexture(fname);
  glutDisplayFunc(display);
  glutSetCursor(GLUT_CURSOR_CROSSHAIR);
  glutKeyboardFunc(keyboard);
  glutSpecialFunc(special);
  glutFullScreen();
  glutMainLoop();
  return 0;
}
