/*

 MIT License
 
 Copyright © 2021 Samuel Venable
 Copyright © 2021 Lars Nilsson
 
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

#include <cstring>
#include <iostream>

#include "xproc.h"

using std::string;
using std::wstring;
using std::vector;
using std::to_string;
using std::size_t;

namespace XProcPrint {

void PrintXProcHelp() {
  std::cout << "usage: xproc [options]         " << std::endl;
  std::cout << "  options:                     " << std::endl;
  std::cout << "    --help                     " << std::endl;
  std::cout << "    --pid-enum                 " << std::endl;
  std::cout << "    --pid-exists     pid       " << std::endl;
  std::cout << "    --pid-kill       pid       " << std::endl;
  std::cout << "    --ppid-from-pid  pid       " << std::endl;
  std::cout << "    --pid-from-ppid  pid       " << std::endl;
  std::cout << "    --exe-from-pid   pid       " << std::endl;
  std::cout << "    --cwd-from-pid   pid       " << std::endl;
  std::cout << "    --cmd-from-pid   pid       " << std::endl;
  std::cout << "    --env-from-pid   pid [name]" << std::endl;
}

void PrintPidEnumeration() {
  PROCID *procId; int size;
  XProc::ProcIdEnumerate(&procId, &size);
  if (procId) {
    for (int i = 0; i < size; i++) {
      std::cout << procId[i] << std::endl;
    }
    free(procId);
  }
}

void PrintWhetherPidExists(PROCID procId) {
  std::cout << XProc::ProcIdExists(procId) << std::endl;
}

void PrintWhetherPidKilled(PROCID procId) {
  std::cout << XProc::ProcIdKill(procId) << std::endl;
}

void PrintPpidFromPid(PROCID procId) {
  if (!XProc::ProcIdExists(procId)) return;
  PROCID parentProcId;
  XProc::ParentProcIdFromProcId(procId, &parentProcId);
  std::cout << parentProcId << std::endl;
}

void PrintPidFromPpid(PROCID parentProcId) {
  if (!XProc::ProcIdExists(parentProcId)) return;
  PROCID *procId; int size;
  XProc::ProcIdFromParentProcId(parentProcId, &procId, &size);
  if (procId) {
    for (int i = 0; i < size; i++) {
      std::cout << procId[i] << std::endl;
    }
    free(procId);
  }
}

void PrintExeFromPid(PROCID procId) {
  if (!XProc::ProcIdExists(procId)) return;
  char *buffer;
  XProc::ExeFromProcId(procId, &buffer);
  if (buffer) {
    std::cout << buffer << std::endl;
  }
}

void PrintCwdFromPid(PROCID procId) {
  if (!XProc::ProcIdExists(procId)) return;
  char *buffer;
  XProc::CwdFromProcId(procId, &buffer);
  if (buffer) {
    std::cout << buffer << std::endl;
  }
}

void PrintCmdFromPid(PROCID procId) {
  if (!XProc::ProcIdExists(procId)) return;
  char **buffer; int size;
  XProc::CmdlineFromProcId(procId, &buffer, &size);
  if (buffer) {
    for (int i = 0; i < size; i++) {
      std::cout << "\"" << StringReplaceAll(buffer[i], "\"", 
	  "\\\"") << ((i < size - 1) ? "\" " : "\"");
    }
    std::cout << std::endl;
    XProc::FreeCmdline(buffer);
  }
}

void PrintEnvFromPid(PROCID procId, const char *name) {
  if (!XProc::ProcIdExists(procId)) return;
  if (name) {
    char *value;
    XProc::EnvironFromProcIdEx(procId, name, &value);
    if (value) {
      std::cout << "\"" << StringReplaceAll(value, "\"", "\\\"") << "\"" << std::endl;
      return;
    }
  }
  char **buffer; int size;
  XProc::EnvironFromProcId(procId, &buffer, &size);
  if (buffer) {
    for (int i = 0; i < size; i++) {
      vector<string> equalssplit = StringSplitByFirstEqualsSign(buffer[i]);
      for (int j = 0; j < equalssplit.size(); j++) {
        if (j == equalssplit.size() - 1) {
          equalssplit[j] = StringReplaceAll(equalssplit[j], "\"", "\\\"");
          std::cout << equalssplit[0] << "=\"" << equalssplit[j] << "\"" << std::endl;
        }
      }
    }
    XProc::FreeEnviron(buffer);
  }
}

} // namespace XProcPrint

int main(int argc, char **argv) {
  if (argc == 1 || (argc >= 2 && strcmp(argv[1], "--help") == 0)) {
    XProcPrint::PrintXProcHelp();
  } else if (argc == 2) {
    if (strcmp(argv[1], "--pid-enum") == 0) {
      XProcPrint::PrintPidEnumeration();
    }
  } else if (argc >= 3) {
    PROCID pid = strtoul(argv[2], nullptr, 10);
    if (strcmp(argv[1], "--pid-exists") == 0) {
      XProcPrint::PrintWhetherPidExists(pid);
    } else if (strcmp(argv[1], "--pid-kill") == 0) {
      XProcPrint::PrintWhetherPidKilled(pid);
    } else if (strcmp(argv[1], "--ppid-from-pid") == 0) {
      XProcPrint::PrintPpidFromPid(pid);
    } else if (strcmp(argv[1], "--pid-from-ppid") == 0) {
      XProcPrint::PrintPidFromPpid(pid);
    } else if (strcmp(argv[1], "--exe-from-pid") == 0) {
      XProcPrint::PrintExeFromPid(pid);
    } else if (strcmp(argv[1], "--cwd-from-pid") == 0) {
      XProcPrint::PrintCwdFromPid(pid);
    } else if (strcmp(argv[1], "--cmd-from-pid") == 0) {
      XProcPrint::PrintCmdFromPid(pid);
    } else if (strcmp(argv[1], "--env-from-pid") == 0) {
      if (argc == 3) {
        XProcPrint::PrintEnvFromPid(pid, nullptr);
      } else if (argc > 3) {
        XProcPrint::PrintEnvFromPid(pid, argv[3]);
      }
    }
  }
  return 0;
}
