/*

 MIT License

 Copyright © 2021 Samuel Venable
 Copyright © 2021 Nikita Krapivin
 Copyright © 2021 Robert B. Colton

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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>

#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <sstream>
#include <algorithm>

#include "../Universal/dlgmodule.h"
#include "../Unix/lodepng.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <pthread.h>
#include <libgen.h>
#include <unistd.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <sys/proc_info.h>
#include <libproc.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#include <proc/readproc.h>
#elif defined(__FreeBSD__)
#include <sys/user.h>
#include <libutil.h>
#elif defined(__DragonFly__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <libutil.h>
#include <kvm.h>
#endif

using std::string;
using std::to_string;
using std::vector;

namespace dialog_module {

namespace {

int const dm_x11     = -1;
int const dm_zenity  =  0;
int const dm_kdialog =  1;
int dm_dialogengine  = -1;

void *owner = nullptr;
string caption;
string current_icon;

enum BUTTON_TYPES {
  BUTTON_ABORT,
  BUTTON_IGNORE,
  BUTTON_OK,
  BUTTON_CANCEL,
  BUTTON_YES,
  BUTTON_NO,
  BUTTON_RETRY
};

int const btn_array_len = 7;
string btn_array[btn_array_len] = { "Abort", "Ignore", "OK", "Cancel", "Yes", "No", "Retry" };

bool message_cancel  = false;
bool question_cancel = false;

bool dialog_position = false;
bool dialog_size     = false;

int      dialog_xpos   = 0;
int      dialog_ypos   = 0;
unsigned dialog_width  = 0;
unsigned dialog_height = 0;

void change_relative_to_kwin() {
  setenv("WAYLAND_DISPLAY", "", 1);
  if (dm_dialogengine == dm_x11) {
    Display *display = XOpenDisplay(nullptr);
    Atom aKWinRunning = XInternAtom(display, "KWIN_RUNNING", true);
    bool bKWinRunning = (aKWinRunning != None);
    if (bKWinRunning) dm_dialogengine = dm_kdialog;
    else dm_dialogengine = dm_zenity;
    XCloseDisplay(display);
  }
}

unsigned nlpo2dc(unsigned x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  return x | (x >> 16);
}

void XSetIcon(Display *display, Window window, const char *icon) {
  XSynchronize(display, true);
  Atom property = XInternAtom(display, "_NET_WM_ICON", true);

  unsigned char *data = nullptr;
  unsigned pngwidth, pngheight;
  unsigned error = lodepng_decode32_file(&data, &pngwidth, &pngheight, icon);
  if (error) return;

  unsigned
    widfull = nlpo2dc(pngwidth) + 1,
    hgtfull = nlpo2dc(pngheight) + 1,
    ih, iw;

  const int bitmap_size = widfull * hgtfull * 4;
  unsigned char *bitmap = new unsigned char[bitmap_size]();

  unsigned i = 0;
  unsigned elem_numb = 2 + pngwidth * pngheight;
  unsigned long *result = new unsigned long[elem_numb]();

  result[i++] = pngwidth;
  result[i++] = pngheight;
  for (ih = 0; ih < pngheight; ih++) {
    unsigned tmp = ih * widfull * 4;
    for (iw = 0; iw < pngwidth; iw++) {
      bitmap[tmp + 0] = data[4 * pngwidth * ih + iw * 4 + 2];
      bitmap[tmp + 1] = data[4 * pngwidth * ih + iw * 4 + 1];
      bitmap[tmp + 2] = data[4 * pngwidth * ih + iw * 4 + 0];
      bitmap[tmp + 3] = data[4 * pngwidth * ih + iw * 4 + 3];
      result[i++] = bitmap[tmp + 0] | (bitmap[tmp + 1] << 8) | (bitmap[tmp + 2] << 16) | (bitmap[tmp + 3] << 24);
      tmp += 4;
    }
  }

  XChangeProperty(display, window, property, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)result, elem_numb);
  XFlush(display);
  delete[] result;
  delete[] bitmap;
  delete[] data;
}

string string_replace_all(string str, string substr, string nstr) {
  size_t pos = 0;
  while ((pos = str.find(substr, pos)) != string::npos) {
    str.replace(pos, substr.length(), nstr);
    pos += nstr.length();
  }
  return str;
}

vector<string> string_split(string str, char delimiter) {
  vector<string> vec;
  std::stringstream sstr(str);
  string tmp;
  while (std::getline(sstr, tmp, delimiter))
    vec.push_back(tmp);
  return vec;
}

bool file_exists(string fname) {
  struct stat sb;
  return (stat(fname.c_str(), &sb) == 0 &&
    S_ISREG(sb.st_mode) != 0);
}

string filename_absolute(string fname) {
  char result[PATH_MAX];
  if (realpath(fname.c_str(), result)) {
    if (file_exists(result)) {
      return result;
    }
  }
  return "";
}

string filename_name(string fname) {
  size_t fp = fname.find_last_of("/");
  return fname.substr(fp + 1);
}

string filename_ext(string fname) {
  fname = filename_name(fname);
  size_t fp = fname.find_last_of(".");
  if (fp == string::npos)
    return "";
  return fname.substr(fp);
}

static int XErrorHandlerImpl(Display *display, XErrorEvent *event) {
  return 0;
}

static int XIOErrorHandlerImpl(Display *display) {
  return 0;
}

static void SetErrorHandlers() {
  XSetErrorHandler(XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);
}

static bool kwin_running() {
  SetErrorHandlers();
  Display *d = XOpenDisplay(nullptr);
  Atom aKWinRunning = XInternAtom(d, "KWIN_RUNNING", True);
  bool bKWinRunning = (aKWinRunning != None);
  XCloseDisplay(d);
  return bKWinRunning;
}

static unsigned long GetActiveWidOrWindowPid(Display *display, Window window, bool wid) {
  SetErrorHandlers();
  unsigned char *prop;
  unsigned long property = 0;
  Atom actual_type, filter_atom;
  int actual_format, status;
  unsigned long nitems, bytes_after;
  filter_atom = XInternAtom(display, wid ? "_NET_ACTIVE_WINDOW" : "_NET_WM_PID", true);
  status = XGetWindowProperty(display, window, filter_atom, 0, 1000, false,
  AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes_after, &prop);
  if (status == Success && prop != nullptr) {
    property = prop[0] + (prop[1] << 8) + (prop[2] << 16) + (prop[3] << 24);
    XFree(prop);
  }
  return property;
}

static Window WidFromTop(Display *display) {
  SetErrorHandlers();
  int screen = XDefaultScreen(display);
  Window window = RootWindow(display, screen);
  return (Window)GetActiveWidOrWindowPid(display, window, true);
}

static pid_t PidFromWid(Display *display, Window window) {
  return (pid_t)GetActiveWidOrWindowPid(display, window, false);
}

// create dialog process
static pid_t ProcessCreate(const char *command, int *infp, int *outfp) {
  int p_stdin[2];
  int p_stdout[2];
  pid_t pid;
  if (pipe(p_stdin) == -1)
    return -1;
  if (pipe(p_stdout) == -1) {
    close(p_stdin[0]);
    close(p_stdin[1]);
    return -1;
  }
  pid = fork();
  if (pid < 0) {
    close(p_stdin[0]);
    close(p_stdin[1]);
    close(p_stdout[0]);
    close(p_stdout[1]);
    return pid;
  } else if (pid == 0) {
    close(p_stdin[1]);
    dup2(p_stdin[0], 0);
    close(p_stdout[0]);
    dup2(p_stdout[1], 1);
    dup2(open("/dev/null", O_RDONLY), 2);
    for (int i = 3; i < 4096; i++)
      close(i);
    setsid();
    execl("/bin/sh", "/bin/sh", "-c", command, nullptr);
    exit(0);
  }
  close(p_stdin[0]);
  close(p_stdout[1]);
  if (infp == nullptr) {
    close(p_stdin[1]);
  } else {
    *infp = p_stdin[1];
  }
  if (outfp == nullptr) {
    close(p_stdout[0]);
  } else {
    *outfp = p_stdout[0];
  }
  return pid;
}

#if defined(__APPLE__) && defined(__MACH__)
static void PpidFromPid(pid_t procId, pid_t *parentProcId) {
  proc_bsdinfo proc_info;
  if (proc_pidinfo(procId, PROC_PIDTBSDINFO, 0, &proc_info, sizeof(proc_info)) > 0) {
    *parentProcId = proc_info.pbi_ppid;
  }
}
#endif

#if defined(__DragonFly__)
static kvm_t *kd = nullptr;
#endif

static std::vector<pid_t> PidFromPpid(pid_t parentProcId) {
  std::vector<pid_t> vec;
  #if defined(__APPLE__) && defined(__MACH__)
  int cntp = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
  std::vector<pid_t> proc_info(cntp);
  std::fill(proc_info.begin(), proc_info.end(), 0);
  proc_listpids(PROC_ALL_PIDS, 0, &proc_info[0], sizeof(pid_t) * cntp);
  for (int j = cntp; j > 0; j--) {
    if (proc_info[j] == 0) { continue; }
    pid_t ppid; PpidFromPid(proc_info[j], &ppid);
    if (ppid == parentProcId) {
      vec.push_back(proc_info[j]);
    }
  }
  #elif defined(__linux__) && !defined(__ANDROID__)
  PROCTAB *proc = openproc(PROC_FILLSTAT);
  while (proc_t *proc_info = readproc(proc, nullptr)) {
    if (proc_info->ppid == parentProcId) {
      vec.push_back(proc_info->tgid);
    }
    freeproc(proc_info);
  }
  closeproc(proc);
  #elif defined(__FreeBSD__)
  int cntp; if (kinfo_proc *proc_info = kinfo_getallproc(&cntp)) {
    for (int j = 0; j < cntp; j++) {
      if (proc_info[j].ki_ppid == parentProcId) {
        vec.push_back(proc_info[j].ki_pid);
      }
    }
    free(proc_info);
  }
  #elif defined(__DragonFly__)
  char errbuf[_POSIX2_LINE_MAX];
  kinfo_proc *proc_info = nullptr; 
  const char *nlistf, *memf; nlistf = memf = "/dev/null";
  kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf); if (!kd) return vec;
  int cntp = 0; if ((proc_info = kvm_getprocs(kd, KERN_PROC_ALL, 0, &cntp))) {
    for (int j = 0; j < cntp; j++) {
      if (proc_info[j].kp_pid >= 0 && proc_info[j].kp_ppid >= 0 && 
        proc_info[j].kp_ppid == parentProcId) {
        vec.push_back(proc_info[j].kp_pid);
      }
    }
    free(proc_info);
  }
  #endif
  return vec;
}

static pid_t PidFromPpidRecursive(pid_t parentProcId) {
  std::vector<pid_t> pidVec = PidFromPpid(parentProcId);
  if (pidVec.size()) {
    parentProcId = PidFromPpidRecursive(pidVec[0]);
  }
  return parentProcId;
}

static void *modify_shell_dialog(void *pid) {
  SetErrorHandlers();
  Display *display = XOpenDisplay(nullptr); Window wid;
  pid_t child = PidFromPpidRecursive((pid_t)(std::intptr_t)pid);
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    wid = WidFromTop(display);
    if (PidFromWid(display, wid) == child) {
      break;
    }
  }
  XSetTransientForHint(display, wid, (Window)(std::intptr_t)owner);
  int len = caption.length() + 1; char *buffer = new char[len]();
  strcpy(buffer, caption.c_str()); XChangeProperty(display, wid,
  XInternAtom(display, "_NET_WM_NAME", false),
  XInternAtom(display, "UTF8_STRING", false),
  8, PropModeReplace, (unsigned char *)buffer, len);
  delete[] buffer; XCloseDisplay(display);
  return nullptr;
}

string create_shell_dialog(string command) {
  string output; char buffer[BUFSIZ];
  int outfp = 0, infp = 0; ssize_t nRead = 0;
  pid_t pid = ProcessCreate(command.c_str(), &infp, &outfp);
  std::this_thread::sleep_for(std::chrono::milliseconds(100)); pthread_t thread;
  pthread_create(&thread, nullptr, modify_shell_dialog, (void *)(std::intptr_t)pid);
  while ((nRead = read(outfp, buffer, BUFSIZ)) > 0) {
    buffer[nRead] = '\0';
    output.append(buffer, nRead);
  }
  pthread_cancel(thread);
  while (output.back() == '\r' || output.back() == '\n')
    output.pop_back();
  return output;
}

string add_escaping(string str, bool is_caption, string new_caption) {
  string result = str; if (is_caption && str == "") result = new_caption;
  result = string_replace_all(result, "\"", "\\\"");
  return result;
}

string remove_trailing_zeros(double numb) {
  string strnumb = std::to_string(numb);

  while (!strnumb.empty() && strnumb.find('.') != string::npos && (strnumb.back() == '.' || strnumb.back() == '0'))
    strnumb.pop_back();

  return strnumb;
}

string zenity_filter(string input) {
  input = string_replace_all(input, "\r", "");
  input = string_replace_all(input, "\n", "");
  std::vector<string> stringVec = string_split(input, '|');
  string string_output = "";

  unsigned index = 0;
  for (string str : stringVec) {
    if (index % 2 == 0)
      string_output += string(" --file-filter=\"") +
        add_escaping(string_replace_all(str, "*.*", "*"), false, "") + string("|");
    else {
      std::replace(str.begin(), str.end(), ';', ' ');
      string_output += add_escaping(string_replace_all(str, "*.*", "*"), false, "") + string("\"");
    }

    index += 1;
  }

  return string_output;
}

string kdialog_filter(string input) {
  input = string_replace_all(input, "\r", "");
  input = string_replace_all(input, "\n", "");
  std::vector<string> stringVec = string_split(input, '|');
  string string_output = " \"";

  unsigned index = 0;
  for (string str : stringVec) {
    if (index % 2 == 0) {
      if (index != 0)
        string_output += "\n";
      size_t first = str.find('(');
      if (first != string::npos) {
        size_t last = str.find(')', first);
        if (last != string::npos)
          str.erase(first, last - first + 1);
      }
      string_output += add_escaping(str, false, "") + string(" (");
    } else {
      std::replace(str.begin(), str.end(), ';', ' ');
      string_output += add_escaping(string_replace_all(str, "*.*", "*"), false, "") + string(")");
    }

    index += 1;
  }

  string_output += "\"";
  return string_output;
}

int color_get_red(int col) { return ((col & 0x000000FF)); }
int color_get_green(int col) { return ((col & 0x0000FF00) >> 8); }
int color_get_blue(int col) { return ((col & 0x00FF0000) >> 16); }

int make_color_rgb(unsigned char r, unsigned char g, unsigned char b) {
  return r | (g << 8) | (b << 16);
}

int show_message_helperfunc(char *str) {
  change_relative_to_kwin();
  string str_command;
  string str_title = message_cancel ? add_escaping(caption, true, "Question") : add_escaping(caption, true, "Information");
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + add_escaping(current_icon, false, "") + string("\"") : "";
  string caption_previous = caption;
  caption = (str_title == "Information") ? "Information" : caption;
  caption = (str_title == "Question") ? "Question" : caption;

  string str_cancel;
  string str_echo = "echo 1";

  if (message_cancel)
    str_echo = "if [ $? = 0 ] ;then echo 1;else echo -1;fi";

  if (dm_dialogengine == dm_zenity) {
    string str_icon_2 = string("\" --icon-name=dialog-information") + str_icon + string(");");
    str_cancel = string("--info --ok-label=\"") + add_escaping(btn_array[BUTTON_OK], true, "") + string("\" ");

    if (message_cancel) {
      str_icon_2 = string("\" --icon-name=dialog-question") + str_icon + string(");");
      str_cancel = string("--question --ok-label=\"") + add_escaping(btn_array[BUTTON_OK], true, "") + string("\" --cancel-label=\"") + add_escaping(btn_array[BUTTON_CANCEL], true, "") + string("\" ");
    }

    str_command = string("ans=$(zenity ") +
    str_cancel + string("--title=\"") + str_title + string("\" --no-wrap --text=\"") +
    add_escaping(str, false, "") + str_icon_2 + str_echo;
  }
  else if (dm_dialogengine == dm_kdialog) {
    str_cancel = string("--msgbox \"") + add_escaping(str, false, "") + string("\" --ok-label \"") + add_escaping(btn_array[BUTTON_OK], true, "") + string("\"") + str_icon + string(" ");

    if (message_cancel)
      str_cancel = string("--yesno \"") + add_escaping(str, false, "") + string("\" --yes-label \"") + add_escaping(btn_array[BUTTON_OK], true, "") + string("\" --no-label \"") + add_escaping(btn_array[BUTTON_CANCEL], true, "") + string("\"") + str_icon + string(" ");

    str_command = string("kdialog ") +
    str_cancel + string("--title \"") + str_title + string("\";") + str_echo;
  }

  string str_result = create_shell_dialog(str_command);
  caption = caption_previous;
  double result = strtod(str_result.c_str(), nullptr);
  return (int)result;
}

int show_question_helperfunc(char *str) {
  change_relative_to_kwin();
  string str_command;
  string str_title = add_escaping(caption, true, "Question");
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + add_escaping(current_icon, false, "") + string("\"") : "";
  string caption_previous = caption;
  caption = (str_title == "Question") ? "Question" : caption;
  string str_cancel = "";

  if (dm_dialogengine == dm_zenity) {
    if (question_cancel)
      str_cancel = string("--extra-button=\"") + add_escaping(btn_array[BUTTON_CANCEL], true, "") + string("\" ");

    str_command = string("ans=$(zenity ") +
    string("--question --ok-label=\"") + add_escaping(btn_array[BUTTON_YES], true, "") + string("\" --cancel-label=\"") + add_escaping(btn_array[BUTTON_NO], true, "") + string("\" ") + str_cancel +  string("--title=\"") +
    str_title + string("\" --no-wrap --text=\"") + add_escaping(str, false, "") +
    string("\" --icon-name=dialog-question") + str_icon + string(");if [ $? = 0 ] ;then echo 1;elif [ $ans = \"") + btn_array[BUTTON_CANCEL] + string("\" ] ;then echo -1;else echo 0;fi");
  }
  else if (dm_dialogengine == dm_kdialog) {
    if (question_cancel)
      str_cancel = "cancel";

    str_command = string("kdialog ") +
    string("--yesno") + str_cancel + string(" \"") + add_escaping(str, false, "") + string("\" ") +
    string("--yes-label \"") + add_escaping(btn_array[BUTTON_YES], true, "") + string("\" --no-label \"") + add_escaping(btn_array[BUTTON_NO], true, "") + string("\" ") + string("--title \"") + str_title + string("\" ") + str_icon + string(";") +
    string("x=$? ;if [ $x = 0 ] ;then echo 1;elif [ $x = 1 ] ;then echo 0;elif [ $x = 2 ] ;then echo -1;fi");
  }

  string str_result = create_shell_dialog(str_command);
  caption = caption_previous;
  double result = strtod(str_result.c_str(), nullptr);
  return (int)result;
}

} // anonymous namespace

int show_message(char *str) {
  message_cancel = false;
  return show_message_helperfunc(str);
}

int show_message_cancelable(char *str) {
  message_cancel = true;
  return show_message_helperfunc(str);
}

int show_question(char *str) {
  question_cancel = false;
  return show_question_helperfunc(str);
}

int show_question_cancelable(char *str) {
  question_cancel = true;
  return show_question_helperfunc(str);
}

int show_attempt(char *str) {
  change_relative_to_kwin();
  string str_command;
  string str_title = add_escaping(caption, true, "Error");
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + add_escaping(current_icon, false, "") + string("\"") : "";
  string caption_previous = caption;
  caption = (str_title == "Error") ? "Error" : caption;

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--question --ok-label=\"") + add_escaping(btn_array[BUTTON_RETRY], true, "") + string("\" --cancel-label=\"") + add_escaping(btn_array[BUTTON_CANCEL], true, "") + string("\" ") +  string("--title=\"") +
    str_title + string("\" --no-wrap --text=\"") + add_escaping(str, false, "") +
    string("\" --icon-name=dialog-error ") + str_icon + string(");if [ $? = 0 ] ;then echo 0;else echo -1;fi");
  }
  else if (dm_dialogengine == dm_kdialog) {
    str_command = string("kdialog ") +
    string("--warningyesno") + string(" \"") + add_escaping(str, false, "") + string("\" ") +
    string("--yes-label \"") + add_escaping(btn_array[BUTTON_RETRY], true, "") + string("\" --no-label \"") + add_escaping(btn_array[BUTTON_CANCEL], true, "") + string("\" ") + string("--title \"") +
    str_title + string("\" ") + str_icon + string(";") + string("x=$? ;if [ $x = 0 ] ;then echo 0;else echo -1;fi");
  }

  string str_result = create_shell_dialog(str_command);
  caption = caption_previous;
  double result = strtod(str_result.c_str(), nullptr);
  return (int)result;
}

int show_error(char *str, bool abort) {
  change_relative_to_kwin();
  string str_command;
  string str_title = add_escaping(caption, true, "Error");
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + add_escaping(current_icon, false, "") + string("\"") : "";
  string caption_previous = caption;
  caption = (str_title == "Error") ? "Error" : caption;
  string str_echo;

  if (dm_dialogengine == dm_zenity) {
    str_echo = abort ? "echo 1" : "if [ $? = 0 ] ;then echo 1;else echo -1;fi";

    if (abort) {
      str_command = string("ans=$(zenity ") +
      string("--info --ok-label=\"") + add_escaping(btn_array[BUTTON_ABORT], true, "") + string("\" ") +
      string("--title=\"") + str_title + string("\" --no-wrap --text=\"") +
      add_escaping(str, false, "") + string("\" --icon-name=dialog-error ") + str_icon + string(");") + str_echo;
    } else {
      str_command = string("ans=$(zenity ") +
      string("--question --ok-label=\"") + add_escaping(btn_array[BUTTON_ABORT], true, "") + string("\" --cancel-label=\"") + add_escaping(btn_array[BUTTON_IGNORE], true, "") + string("\" ") +
      string("--title=\"") + str_title + string("\" --no-wrap --text=\"") +
      add_escaping(str, false, "") + string("\" --icon-name=dialog-error ") + str_icon + string(");") + str_echo;
    }
  }
  else if (dm_dialogengine == dm_kdialog) {
    str_echo = abort ? "echo 1" : "x=$? ;if [ $x = 0 ] ;then echo 1;elif [ $x = 1 ] ;then echo -1;fi";

    if (abort) {
      str_command = string("kdialog ") +
      string("--sorry \"") + add_escaping(str, false, "") + string("\" ") +
      string("--ok-label \"") + add_escaping(btn_array[BUTTON_ABORT], true, "") + string("\" ") +
      string("--title \"") + str_title + string("\" ") + str_icon + string(";") + str_echo;
    } else {
      str_command = string("kdialog ") +
      string("--warningyesno \"") + add_escaping(str, false, "") + string("\" ") +
      string("--yes-label \"") + add_escaping(btn_array[BUTTON_ABORT], true, "") + string("\" --no-label \"") + add_escaping(btn_array[BUTTON_IGNORE], true, "") + string("\" ") +
      string("--title \"") + str_title + string("\" ") + str_icon + string(";") + str_echo;
    }
  }

  string str_result = create_shell_dialog(str_command);
  caption = caption_previous;
  double result = strtod(str_result.c_str(), nullptr);
  if (result == 1) exit(0);
  return (int)result;
}

char *get_string(char *str, char *def) {
  change_relative_to_kwin();
  string str_command;
  string str_title = add_escaping(caption, true, "Input Query");
  string caption_previous = caption;
  caption = (str_title == "Input Query") ? "Input Query" : caption;
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + add_escaping(current_icon, false, "") + string("\"") : "";

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--entry --title=\"") + str_title + string("\"") + str_icon + string(" --text=\"") +
    add_escaping(str, false, "") + string("\" --entry-text=\"") +
    add_escaping(def, false, "") + string("\");echo $ans");
  }
  else if (dm_dialogengine == dm_kdialog) {
    str_command = string("ans=$(kdialog ") +
    string("--inputbox \"") + add_escaping(str, false, "") + string("\" \"") +
    add_escaping(def, false, "") + string("\" --title \"") +
    str_title + string("\"") + str_icon + string(");echo $ans");
  }

  static string result;
  result = create_shell_dialog(str_command);
  caption = caption_previous;
  return (char *)result.c_str();
}

char *get_password(char *str, char *def) {
  change_relative_to_kwin();
  string str_command;
  string str_title = add_escaping(caption, true, "Input Query");
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + add_escaping(current_icon, false, "") + string("\"") : "";
  string caption_previous = caption;
  caption = (str_title == "Input Query") ? "Input Query" : caption;

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--entry --title=\"") + str_title + string("\"") + str_icon + string(" --text=\"") +
    add_escaping(str, false, "") + string("\" --hide-text --entry-text=\"") +
    add_escaping(def, false, "") + string("\");echo $ans");
  }
  else if (dm_dialogengine == dm_kdialog) {
    str_command = string("ans=$(kdialog ") +
    string("--password \"") + add_escaping(str, false, "") + string("\" \"") +
    add_escaping(def, false, "") + string("\" --title \"") +
    str_title + string("\"") + str_icon + string(");echo $ans");
  }

  static string result;
  result = create_shell_dialog(str_command);
  caption = caption_previous;
  return (char *)result.c_str();
}

double get_integer(char *str, double def) {
  double DIGITS_MIN = -999999999999999;
  double DIGITS_MAX = 999999999999999;

  if (def < DIGITS_MIN) def = DIGITS_MIN;
  if (def > DIGITS_MAX) def = DIGITS_MAX;

  string str_def = remove_trailing_zeros(def);
  string str_result = get_string(str, (char *)str_def.c_str());
  double result = strtod(str_result.c_str(), nullptr);

  if (result < DIGITS_MIN) result = DIGITS_MIN;
  if (result > DIGITS_MAX) result = DIGITS_MAX;
  return result;
}

double get_passcode(char *str, double def) {
  double DIGITS_MIN = -999999999999999;
  double DIGITS_MAX = 999999999999999;

  if (def < DIGITS_MIN) def = DIGITS_MIN;
  if (def > DIGITS_MAX) def = DIGITS_MAX;

  string str_def = remove_trailing_zeros(def);
  string str_result = get_password(str, (char *)str_def.c_str());
  double result = strtod(str_result.c_str(), nullptr);

  if (result < DIGITS_MIN) result = DIGITS_MIN;
  if (result > DIGITS_MAX) result = DIGITS_MAX;
  return result;
}

char *get_open_filename(char *filter, char *fname) {
  return get_open_filename_ext(filter, fname, (char *)"", (char *)"Open");
}

char *get_open_filename_ext(char *filter, char *fname, char *dir, char *title) {
  change_relative_to_kwin();
  string str_command; string pwd;
  string str_title = add_escaping(title, true, "Open");
  string caption_previous = caption;
  caption = (str_title == "Open") ? "Open" : title;
  string str_fname = filename_name(filename_absolute(fname));
  string str_dir = filename_absolute(dir);
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + add_escaping(current_icon, false, "") + string("\"") : "";

  string str_path = fname;
  if (str_dir[0] != '\0') str_path = str_dir + string("/") + str_fname;
  str_fname = (char *)str_path.c_str();

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--file-selection --title=\"") + str_title + string("\" --filename=\"") +
    add_escaping(str_fname, false, "") + string("\"") + zenity_filter(filter) + str_icon + string(");echo $ans");
  }
  else if (dm_dialogengine == dm_kdialog) {
    pwd = ""; if (str_fname.c_str() && str_fname[0] != '/' && str_fname.length()) pwd = string("\"$PWD/\"") +
      string("\"") + add_escaping(str_fname, false, "") + string("\""); else pwd = "\"$PWD/\"";

    str_command = string("ans=$(kdialog ") +
    string("--getopenfilename ") + pwd + kdialog_filter(filter) +
    string(" --title \"") + str_title + string("\"") + str_icon + string(");echo $ans");
  }

  static string result;
  result = create_shell_dialog(str_command);
  caption = caption_previous;

  if (file_exists(result))
    return (char *)result.c_str();

  return (char *)"";
}

char *get_open_filenames(char *filter, char *fname) {
  return get_open_filenames_ext(filter, fname, (char *)"", (char *)"Open");
}

char *get_open_filenames_ext(char *filter, char *fname, char *dir, char *title) {
  change_relative_to_kwin();
  string str_command; string pwd;
  string str_title = add_escaping(title, true, "Open");
  string caption_previous = caption;
  caption = (str_title == "Open") ? "Open" : title;
  string str_fname = filename_name(filename_absolute(fname));
  string str_dir = filename_absolute(dir);
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + add_escaping(current_icon, false, "") + string("\"") : "";

  string str_path = fname;
  if (str_dir[0] != '\0') str_path = str_dir + string("/") + str_fname;
  str_fname = (char *)str_path.c_str();

  if (dm_dialogengine == dm_zenity) {
    str_command = string("zenity ") +
    string("--file-selection --multiple --separator='\n' --title=\"") + str_title + string("\" --filename=\"") +
    add_escaping(str_fname, false, "") + string("\"") + zenity_filter(filter) + str_icon;
  }
  else if (dm_dialogengine == dm_kdialog) {
    pwd = ""; if (str_fname.c_str() && str_fname[0] != '/' && str_fname.length()) pwd = string("\"$PWD/\"") +
      string("\"") + add_escaping(str_fname, false, "") + string("\""); else pwd = "\"$PWD/\"";

    str_command = string("kdialog ") +
    string("--getopenfilename ") + pwd + kdialog_filter(filter) +
    string(" --multiple --separate-output --title \"") + str_title + string("\"") + str_icon;
  }

  static string result;
  result = create_shell_dialog(str_command);
  caption = caption_previous;
  std::vector<string> stringVec = string_split(result, '\n');

  bool success = true;
  for (const string &str : stringVec) {
    if (!file_exists(str))
      success = false;
  }

  if (success)
    return (char *)result.c_str();

  return (char *)"";
}

char *get_save_filename(char *filter, char *fname) {
  return get_save_filename_ext(filter, fname, (char *)"", (char *)"Save As");
}

char *get_save_filename_ext(char *filter, char *fname, char *dir, char *title) {
  change_relative_to_kwin();
  string str_command; string pwd;
  string str_title = add_escaping(title, true, "Save As");
  string caption_previous = caption;
  caption = (str_title == "Save As") ? "Save As" : title;
  string str_fname = filename_name(filename_absolute(fname));
  string str_dir = filename_absolute(dir);
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + add_escaping(current_icon, false, "") + string("\"") : "";

  string str_path = fname;
  if (str_dir[0] != '\0') str_path = str_dir + string("/") + str_fname;
  str_fname = (char *)str_path.c_str();

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--file-selection  --save --confirm-overwrite --title=\"") + str_title + string("\" --filename=\"") +
    add_escaping(str_fname, false, "") + string("\"") + zenity_filter(filter) + str_icon + string(");echo $ans");
  }
  else if (dm_dialogengine == dm_kdialog) {
    pwd = ""; if (str_fname.c_str() && str_fname[0] != '/' && str_fname.length()) pwd = string("\"$PWD/\"") +
      string("\"") + add_escaping(str_fname, false, "") + string("\""); else pwd = "\"$PWD/\"";

    str_command = string("ans=$(kdialog ") +
    string("--getsavefilename ") + pwd + kdialog_filter(filter) +
    string(" --title \"") + str_title + string("\"") + str_icon + string(");echo $ans");
  }

  static string result;
  result = create_shell_dialog(str_command);
  caption = caption_previous;
  return (char *)result.c_str();
}

char *get_directory(char *dname) {
  return get_directory_alt((char *)"Select Directory", dname);
}

char *get_directory_alt(char *capt, char *root) {
  change_relative_to_kwin();
  string str_command; string pwd;
  string str_title = add_escaping(capt, true, "Select Directory");
  string caption_previous = caption;
  caption = (str_title == "Select Directory") ? "Select Directory" : capt;
  string str_dname = root;
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + add_escaping(current_icon, false, "") + string("\"") : "";
  string str_end = ");if [ $ans = / ] ;then echo $ans;elif [ $? = 1 ] ;then echo $ans/;else echo $ans;fi";

  if (dm_dialogengine == dm_zenity) {
    str_command = string("ans=$(zenity ") +
    string("--file-selection --directory --title=\"") + str_title + string("\" --filename=\"") +
    add_escaping(str_dname, false, "") + string("\"") + str_icon + str_end;
  }
  else if (dm_dialogengine == dm_kdialog) {
    if (str_dname.c_str() && str_dname[0] != '/' && str_dname.length()) pwd = string("\"$PWD/\"") +
      string("\"") + add_escaping(str_dname, false, "") + string("\""); else pwd = "\"$PWD/\"";

    str_command = string("ans=$(kdialog ") +
    string("--getexistingdirectory ") + pwd + string(" --title \"") + str_title + string("\"") + str_icon + str_end;
  }

  static string result;
  result = create_shell_dialog(str_command);
  caption = caption_previous;
  return (char *)result.c_str();
}

int get_color(int defcol) {
  return get_color_ext(defcol, (char *)"Color");
}

int get_color_ext(int defcol, char *title) {
  change_relative_to_kwin();
  string str_command;
  string str_title = add_escaping(title, true, "Color");
  string caption_previous = caption;
  caption = (str_title == "Color") ? "Color" : title;
  string str_defcol;
  string str_result;
  string str_iconflag = (dm_dialogengine == dm_zenity) ? " --window-icon=\"" : " --icon \"";
  if (current_icon == "") current_icon = filename_absolute("assets/icon.png");
  string str_icon = file_exists(current_icon) ? str_iconflag + add_escaping(current_icon, false, "") + string("\"") : "";

  int red; int green; int blue;
  red = color_get_red(defcol);
  green = color_get_green(defcol);
  blue = color_get_blue(defcol);

  if (dm_dialogengine == dm_zenity) {
    str_defcol = string("rgb(") + std::to_string(red) + string(",") +
    std::to_string(green) + string(",") + std::to_string(blue) + string(")");
    str_command = string("ans=$(zenity ") +
    string("--color-selection --show-palette --title=\"") + str_title + string("\" --color='") +
    str_defcol + string("'") + str_icon + string(");if [ $? = 0 ] ;then echo $ans;else echo -1;fi");

    str_result = create_shell_dialog(str_command);
    caption = caption_previous;
    if (str_result == "-1") return strtod(str_result.c_str(), nullptr);
    str_result = string_replace_all(str_result, "rgba(", "");
    str_result = string_replace_all(str_result, "rgb(", "");
    str_result = string_replace_all(str_result, ")", "");
    std::vector<string> stringVec = string_split(str_result, ',');

    unsigned int index = 0;
    for (const string &str : stringVec) {
      if (index == 0) red = strtod(str.c_str(), nullptr);
      if (index == 1) green = strtod(str.c_str(), nullptr);
      if (index == 2) blue = strtod(str.c_str(), nullptr);
      index += 1;
    }

  } else if (dm_dialogengine == dm_kdialog) {
    char hexcol[16];
    snprintf(hexcol, sizeof(hexcol), "%02x%02x%02x", red, green, blue);

    str_defcol = string("#") + string(hexcol);
    std::transform(str_defcol.begin(), str_defcol.end(), str_defcol.begin(), ::toupper);

    str_command = string("ans=$(kdialog ") +
    string("--getcolor --default '") + str_defcol + string("' --title \"") + str_title +
    string("\"") + str_icon + string(");if [ $? = 0 ] ;then echo $ans;else echo -1;fi");

    str_result = create_shell_dialog(str_command);
    caption = caption_previous;
    if (str_result == "-1") return strtod(str_result.c_str(), nullptr);
    str_result = str_result.substr(1, str_result.length() - 1);

    unsigned int color;
    std::stringstream ss2;
    ss2 << std::hex << str_result;
    ss2 >> color;

    blue = color_get_red(color);
    green = color_get_green(color);
    red = color_get_blue(color);
  }

  return (int)make_color_rgb(red, green, blue);
}

char *widget_get_caption() {
  return (char *)caption.c_str();
}

void widget_set_caption(char *title) {
  caption = title ? title : "";
}

char *widget_get_owner() {
  static string result;
  result = to_string((long long)owner);
  return (char *)result.c_str();
}

void widget_set_owner(char *hwnd) {
  string str_hwnd = hwnd;
  owner = (void *)strtoll(str_hwnd.c_str(), nullptr, 10);
}

char *widget_get_icon() {
  if (current_icon == "")
    current_icon = filename_absolute("assets/icon.png");
  return (char *)current_icon.c_str();
}

void widget_set_icon(char *icon) {
  current_icon = filename_absolute(icon);
}

char *widget_get_system() {
  if (dm_dialogengine == dm_zenity)
    return (char *)"Zenity";

  if (dm_dialogengine == dm_kdialog)
    return (char *)"KDialog";

  return (char *)"X11";
}

void widget_set_system(char *sys) {
  string str_sys = sys;

  if (str_sys == "X11")
    dm_dialogengine = dm_x11;

  if (str_sys == "Zenity")
    dm_dialogengine = dm_zenity;

  if (str_sys == "KDialog")
    dm_dialogengine = dm_kdialog;
}

void widget_set_button_name(int type, char *name) {
  string str_name = name;
  btn_array[type] = str_name;
}

char *widget_get_button_name(int type) {
  return (char *)btn_array[type].c_str();
}

} // namepace dialog_module
