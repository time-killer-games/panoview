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

#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>

#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <climits>
#include <cstdio>

#include "xproc.h"
#if defined(EXE_INCLUDES)
#include "xproc32.h"
#include "xproc64.h"
#endif

#if !defined(_WIN32)
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <Objbase.h>
#include <tlhelp32.h>
#include <winternl.h>
#include <psapi.h>
#elif (defined(__APPLE__) && defined(__MACH__))
#include <sys/sysctl.h>
#include <sys/proc_info.h>
#include <libproc.h>
#elif (defined(__linux__) && !defined(__ANDROID__))
#include <proc/readproc.h>
#elif defined(__FreeBSD__)
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/user.h>
#include <libprocstat.h>
#include <libutil.h>
#endif

std::string StringReplaceAll(std::string str, std::string substr, std::string nstr) {
  std::size_t pos = 0;
  while ((pos = str.find(substr, pos)) != std::string::npos) {
    str.replace(pos, substr.length(), nstr);
    pos += nstr.length();
  }
  return str;
}

std::vector<std::string> StringSplit(std::string str, char delim) {
  std::vector<std::string> vec;
  std::stringstream sstr(str);
  std::string tmp;
  while (std::getline(sstr, tmp, delim))
    vec.push_back(tmp);
  return vec;
}

std::vector<std::string> StringSplitByFirstEqualsSign(std::string str) {
  std::size_t pos = 0;
  std::vector<std::string> vec;
  if ((pos = str.find_first_of("=")) != std::string::npos) {
    vec.push_back(str.substr(0, pos));
    vec.push_back(str.substr(pos + 1));
  }
  return vec;
}

#if defined(_WIN32)
std::wstring widen(std::string str) {
  std::size_t wchar_count = str.size() + 1;
  std::vector<wchar_t> buf(wchar_count);
  return std::wstring { buf.data(), 
    (std::size_t)MultiByteToWideChar(
    CP_UTF8, 0, str.c_str(), -1, 
    buf.data(), (int)wchar_count) };
}

std::string narrow(std::wstring wstr) {
  int nbytes = WideCharToMultiByte(CP_UTF8,
    0, wstr.c_str(), (int)wstr.length(), 
    nullptr, 0, nullptr, nullptr);
  std::vector<char> buf(nbytes);
  return std::string { buf.data(), 
    (std::size_t)WideCharToMultiByte(
    CP_UTF8, 0, wstr.c_str(), 
    (int)wstr.length(), buf.data(), 
    nbytes, nullptr, nullptr) };
}
#endif

namespace {

#if defined(_WIN32)
enum MEMTYP {
  MEMCMD,
  MEMENV,
  MEMCWD
};

#define RTL_DRIVE_LETTER_CURDIR struct {\
  WORD Flags;\
  WORD Length;\
  ULONG TimeStamp;\
  STRING DosPath;\
}

#define RTL_USER_PROCESS_PARAMETERS struct {\
  ULONG MaximumLength;\
  ULONG Length;\
  ULONG Flags;\
  ULONG DebugFlags;\
  PVOID ConsoleHandle;\
  ULONG ConsoleFlags;\
  PVOID StdInputHandle;\
  PVOID StdOutputHandle;\
  PVOID StdErrorHandle;\
  UNICODE_STRING CurrentDirectoryPath;\
  PVOID CurrentDirectoryHandle;\
  UNICODE_STRING DllPath;\
  UNICODE_STRING ImagePathName;\
  UNICODE_STRING CommandLine;\
  PVOID Environment;\
  ULONG StartingPositionLeft;\
  ULONG StartingPositionTop;\
  ULONG Width;\
  ULONG Height;\
  ULONG CharWidth;\
  ULONG CharHeight;\
  ULONG ConsoleTextAttributes;\
  ULONG WindowFlags;\
  ULONG ShowWindowFlags;\
  UNICODE_STRING WindowTitle;\
  UNICODE_STRING DesktopName;\
  UNICODE_STRING ShellInfo;\
  UNICODE_STRING RuntimeData;\
  RTL_DRIVE_LETTER_CURDIR DLCurrentDirectory[32];\
  ULONG EnvironmentSize;\
}

HANDLE OpenProcessWithDebugPrivilege(PROCID procId) {
  HANDLE hToken;
  LUID luid;
  TOKEN_PRIVILEGES tkp;
  OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
  LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid);
  tkp.PrivilegeCount = 1;
  tkp.Privileges[0].Luid = luid;
  tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  AdjustTokenPrivileges(hToken, false, &tkp, sizeof(tkp), nullptr, nullptr);
  CloseHandle(hToken);
  return OpenProcess(PROCESS_ALL_ACCESS, false, procId);
}

bool IsX86Process(HANDLE procHandle) {
  bool isWow = true;
  SYSTEM_INFO systemInfo = { 0 };
  GetNativeSystemInfo(&systemInfo);
  if (systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
    return isWow;
  IsWow64Process(procHandle, (PBOOL)&isWow);
  return isWow;
}

#if defined(EXE_INCLUDES)
void OutputThread(HANDLE handle, std::string *output) {
  std::string result;
  DWORD dwRead = 0;
  char buffer[BUFSIZ];
  while (ReadFile(handle, buffer, BUFSIZ, &dwRead, nullptr) && dwRead) {
    buffer[dwRead] = '\0';
    result.append(buffer, dwRead);
    *(output) = result;
  }
}

std::string ExecuteXProcAndReadOuput(std::string arg, PROCID procId, std::string opt = "") {
  HANDLE proc = OpenProcessWithDebugPrivilege(procId);
  std::string exe;
  std::string tmp = XProc::EnvironmentGetVariable("TMP");
  if (IsX86Process(proc)) {
    exe = tmp + "\\xproc32.exe";
  } else {
    exe = tmp + "\\xproc64.exe";
  }
  std::wstring wexe = widen(exe);
  DeleteFileW(wexe.c_str());
  FILE *file = nullptr;
  std::wofstream strm(wexe.c_str());
  strm.close();
  if (_wfopen_s(&file, wexe.c_str(), L"wb") == 0) {
    if (IsX86Process(proc)) {
      fwrite((char *)xproc32, sizeof(char), sizeof(xproc32), file);
    } else {
      fwrite((char *)xproc64, sizeof(char), sizeof(xproc64), file);
    }
    fclose(file);
  }
  CloseHandle(proc); std::string output;
  std::string command = "\"" + std::string(exe) + "\" " + arg + " " + 
  std::to_string(procId) + ((!opt.empty()) ? (" \"" + opt + "\"") : "");
  std::wstring wstr_command = widen(command);
  wchar_t cwstr_command[32768];
  wcsncpy_s(cwstr_command, 32768, wstr_command.c_str(), 32768);
  bool proceed = true;
  HANDLE hStdInPipeRead = nullptr;
  HANDLE hStdInPipeWrite = nullptr;
  HANDLE hStdOutPipeRead = nullptr;
  HANDLE hStdOutPipeWrite = nullptr;
  SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, true };
  proceed = CreatePipe(&hStdInPipeRead, &hStdInPipeWrite, &sa, 0);
  if (proceed == false) return "";
  SetHandleInformation(hStdInPipeWrite, HANDLE_FLAG_INHERIT, 0);
  proceed = CreatePipe(&hStdOutPipeRead, &hStdOutPipeWrite, &sa, 0);
  if (proceed == false) return "";
  STARTUPINFOW si = { 0 };
  si.cb = sizeof(STARTUPINFOW);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdError = hStdOutPipeWrite;
  si.hStdOutput = hStdOutPipeWrite;
  si.hStdInput = hStdInPipeRead;
  PROCESS_INFORMATION pi = { 0 };
  if (CreateProcessW(nullptr, cwstr_command, nullptr, nullptr, true, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    CloseHandle(hStdOutPipeWrite);
    CloseHandle(hStdInPipeRead);
    MSG msg; HANDLE waitHandles[] = { pi.hProcess, hStdOutPipeRead };
    std::thread outthrd(OutputThread, hStdOutPipeRead, &output);
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
    while (output.back() == '\r' || output.back() == '\n')
      output.pop_back();
  }
  return output;
}
#endif

void CwdCmdEnvFromProcId(PROCID procId, wchar_t **buffer, int type) {
  HANDLE procHandle = OpenProcessWithDebugPrivilege(procId);
  if (procHandle == nullptr) return;
  PEB peb; SIZE_T nRead; ULONG len = 0;
  PROCESS_BASIC_INFORMATION pbi;
  RTL_USER_PROCESS_PARAMETERS upp;
  typedef NTSTATUS (__stdcall *NTQIP)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
  NTQIP NtQueryInformationProcess = NTQIP(GetProcAddress( GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess"));
  NTSTATUS status = NtQueryInformationProcess(procHandle, ProcessBasicInformation, &pbi, sizeof(pbi), &len);
  if (status) { CloseHandle(procHandle); return; }
  ReadProcessMemory(procHandle, pbi.PebBaseAddress, &peb, sizeof(peb), &nRead);
  if (!nRead) { CloseHandle(procHandle); return; }
  ReadProcessMemory(procHandle, peb.ProcessParameters, &upp, sizeof(upp), &nRead);
  if (!nRead) { CloseHandle(procHandle); return; }
  PVOID buf = nullptr; len = 0;
  if (type == MEMCWD) {
    buf = upp.CurrentDirectoryPath.Buffer;
    len = upp.CurrentDirectoryPath.Length;
  } else if (type == MEMENV) {
    buf = upp.Environment;
    len = upp.EnvironmentSize;
  } else if (type == MEMCMD) {
    buf = upp.CommandLine.Buffer;
    len = upp.CommandLine.Length;
  }
  wchar_t *res = new wchar_t[len / 2 + 1];
  ReadProcessMemory(procHandle, buf, res, len, &nRead);
  if (!nRead) { CloseHandle(procHandle); return; }
  res[len / 2] = L'\0'; *buffer = res;
}
#endif

#if (defined(__APPLE__) && defined(__MACH__))
enum MEMTYP {
  MEMCMD,
  MEMENV
};

std::vector<std::string> CmdEnvVec1;
void CmdEnvFromProcId(PROCID procId, char ***buffer, int *size, int type) {
  if (!XProc::ProcIdExists(procId)) return;
  CmdEnvVec1.clear(); int i = 0;
  int argmax, nargs; std::size_t s;
  char *procargs, *sp, *cp; int mib[3];
  mib[0] = CTL_KERN; mib[1] = KERN_ARGMAX;
  s = sizeof(argmax);
  if (sysctl(mib, 2, &argmax, &s, nullptr, 0) == -1) {
    return;
  }
  procargs = (char *)malloc(argmax);
  if (procargs == nullptr) {
    return;
  }
  mib[0] = CTL_KERN; mib[1] = KERN_PROCARGS2;
  mib[2] = procId; s = argmax;
  if (sysctl(mib, 3, procargs, &s, nullptr, 0) == -1) {
    free(procargs); return;
  }
  memcpy(&nargs, procargs, sizeof(nargs));
  cp = procargs + sizeof(nargs);
  for (; cp < &procargs[s]; cp++) { 
    if (*cp == '\0') break;
  }
  if (cp == &procargs[s]) {
    free(procargs); return;
  }
  for (; cp < &procargs[s]; cp++) {
    if (*cp != '\0') break;
  }
  if (cp == &procargs[s]) {
    free(procargs); return;
  }
  sp = cp; int j = 0;
  while (*sp != '\0') {
    if (type && j >= nargs) { 
      CmdEnvVec1.push_back(sp); i++;
    } else if (!type && j < nargs) {
      CmdEnvVec1.push_back(sp); i++;
    }
    sp += strlen(sp) + 1; j++;
  }
  if (procargs) free(procargs);
  std::vector<char *> CmdEnvVec2;
  for (int j = 0; j < CmdEnvVec1.size(); j++)
    CmdEnvVec2.push_back((char *)CmdEnvVec1[j].c_str());
  char **arr = new char *[CmdEnvVec2.size()]();
  std::copy(CmdEnvVec2.begin(), CmdEnvVec2.end(), arr);
  *buffer = arr; *size = i;
}
#endif

} // anonymous namespace

namespace XProc {

void ProcIdEnumerate(PROCID **procId, int *size) {
  std::vector<PROCID> vec; int i = 0;
  #if defined(_WIN32)
  HANDLE hp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  PROCESSENTRY32 pe = { 0 };
  pe.dwSize = sizeof(PROCESSENTRY32);
  if (Process32First(hp, &pe)) {
    do {
      vec.push_back(pe.th32ProcessID); i++;
    } while (Process32Next(hp, &pe));
  }
  CloseHandle(hp);
  #elif (defined(__APPLE__) && defined(__MACH__))
  if (ProcIdExists(0)) { vec.push_back(0); i++; }
  int cntp = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
  std::vector<PROCID> proc_info(cntp);
  std::fill(proc_info.begin(), proc_info.end(), 0);
  proc_listpids(PROC_ALL_PIDS, 0, &proc_info[0], sizeof(PROCID) * cntp);
  for (int j = cntp; j > 0; j--) {
    if (proc_info[j] == 0) { continue; }
    vec.push_back(proc_info[j]); i++;
  }
  #elif (defined(__linux__) && !defined(__ANDROID__))
  if (ProcIdExists(0)) { vec.push_back(0); i++; }
  PROCTAB *proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
  while (proc_t *proc_info = readproc(proc, nullptr)) {
    vec.push_back(proc_info->tgid); i++;
    freeproc(proc_info);
  }
  closeproc(proc);
  #elif defined(__FreeBSD__)
  int cntp; if (kinfo_proc *proc_info = kinfo_getallproc(&cntp)) {
    for (int j = 0; j < cntp; j++) {
      vec.push_back(proc_info[j].ki_pid); i++;
    }
    free(proc_info);
  }
  #endif
  *procId = (PROCID *)malloc(sizeof(PROCID) * vec.size());
  if (procId) {
    std::copy(vec.begin(), vec.end(), *procId);
    *size = i;
  }
}

void ProcIdFromSelf(PROCID *procId) {
  #if !defined(_WIN32)
  *procId = getpid();
  #elif defined(_WIN32)
  *procId = GetCurrentProcessId();
  #endif
}

void ParentProcIdFromSelf(PROCID *parentProcId) {
  #if !defined(_WIN32)
  *parentProcId = getppid();
  #elif defined(_WIN32)
  ParentProcIdFromProcId(GetCurrentProcessId(), parentProcId);
  #endif
}

bool ProcIdExists(PROCID procId) {
  #if !defined(_WIN32)
  return (kill(procId, 0) == 0);
  #elif defined(_WIN32)
  PROCID *buffer; int size;
  XProc::ProcIdEnumerate(&buffer, &size);
  if (procId) {
    for (int i = 0; i < size; i++) {
      if (procId == buffer[i]) {
        return true;
      }
    }
    free(buffer);
  }
  return false;
  #else
  return false;
  #endif
}

bool ProcIdKill(PROCID procId) {
  #if !defined(_WIN32)
  return (kill(procId, SIGKILL) == 0);
  #elif defined(_WIN32)
  HANDLE procHandle = OpenProcessWithDebugPrivilege(procId);
  if (procHandle == nullptr) return false;
  bool result = TerminateProcess(procHandle, 0);
  CloseHandle(procHandle);
  return result;
  #else
  return false;
  #endif
}

void ParentProcIdFromProcId(PROCID procId, PROCID *parentProcId) {
  #if defined(_WIN32)
  HANDLE hp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  PROCESSENTRY32 pe = { 0 };
  pe.dwSize = sizeof(PROCESSENTRY32);
  if (Process32First(hp, &pe)) {
    do {
      if (pe.th32ProcessID == procId) {
        *parentProcId = pe.th32ParentProcessID;
        break;
      }
    } while (Process32Next(hp, &pe));
  }
  CloseHandle(hp);
  #elif (defined(__APPLE__) && defined(__MACH__))
  proc_bsdinfo proc_info;
  if (proc_pidinfo(procId, PROC_PIDTBSDINFO, 0, &proc_info, sizeof(proc_info)) > 0) {
    *parentProcId = proc_info.pbi_ppid;
  }
  #elif (defined(__linux__) && !defined(__ANDROID__))
  PROCTAB *proc = openproc(PROC_FILLSTATUS | PROC_PID, &procId);
  if (proc_t *proc_info = readproc(proc, nullptr)) { 
    *parentProcId = proc_info->ppid;
    freeproc(proc_info);
  }
  closeproc(proc);
  #elif defined(__FreeBSD__)
  if (kinfo_proc *proc_info = kinfo_getproc(procId)) {
    *parentProcId = proc_info->ki_ppid;
    free(proc_info);
  }
  #endif
}

void ProcIdFromParentProcId(PROCID parentProcId, PROCID **procId, int *size) {
  std::vector<PROCID> vec; int i = 0;
  #if defined(_WIN32)
  HANDLE hp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  PROCESSENTRY32 pe = { 0 };
  pe.dwSize = sizeof(PROCESSENTRY32);
  if (Process32First(hp, &pe)) {
    do {
      if (pe.th32ParentProcessID == parentProcId) {
        vec.push_back(pe.th32ProcessID); i++;
      }
    } while (Process32Next(hp, &pe));
  }
  CloseHandle(hp);
  #elif (defined(__APPLE__) && defined(__MACH__))
  int cntp = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
  std::vector<PROCID> proc_info(cntp);
  std::fill(proc_info.begin(), proc_info.end(), 0);
  proc_listpids(PROC_ALL_PIDS, 0, &proc_info[0], sizeof(PROCID) * cntp);
  for (int j = cntp; j > 0; j--) {
    if (proc_info[j] == 0) { continue; }
    PROCID ppid; ParentProcIdFromProcId(proc_info[j], &ppid);
    if (ppid == parentProcId) {
      vec.push_back(proc_info[j]); i++;
    }
  }
  #elif (defined(__linux__) && !defined(__ANDROID__))
  PROCTAB *proc = openproc(PROC_FILLSTAT);
  while (proc_t *proc_info = readproc(proc, nullptr)) {
    if (proc_info->ppid == parentProcId) {
      vec.push_back(proc_info->tgid); i++;
    }
    freeproc(proc_info);
  }
  closeproc(proc);
  #elif defined(__FreeBSD__)
  int cntp; if (kinfo_proc *proc_info = kinfo_getallproc(&cntp)) {
    for (int j = 0; j < cntp; j++) {
      if (proc_info[j].ki_ppid == parentProcId) {
        vec.push_back(proc_info[j].ki_pid); i++;
      }
    }
    free(proc_info);
  }
  #endif
  *procId = (PROCID *)malloc(sizeof(PROCID) * vec.size());
  if (procId) {
    std::copy(vec.begin(), vec.end(), *procId);
    *size = i;
  }
}

void ExeFromProcId(PROCID procId, char **buffer) {
  if (!ProcIdExists(procId)) return;
  #if defined(_WIN32)
  HANDLE procHandle = OpenProcessWithDebugPrivilege(procId);
  if (procHandle == nullptr) return;
  wchar_t exe[MAX_PATH]; DWORD size = MAX_PATH;
  if (QueryFullProcessImageNameW(procHandle, 0, exe, &size)) {
    static std::string str; str = narrow(exe);
    *buffer = (char *)str.c_str();
  }
  CloseHandle(procHandle);
  #elif (defined(__APPLE__) && defined(__MACH__))
  char exe[PROC_PIDPATHINFO_MAXSIZE];
  if (proc_pidpath(procId, exe, sizeof(exe)) > 0) {
    static std::string str; str = exe;
    *buffer = (char *)str.c_str();
  }
  #elif (defined(__linux__) && !defined(__ANDROID__))
  char exe[PATH_MAX]; 
  std::string symLink = "/proc/" + std::to_string(procId) + "/exe";
  if (realpath(symLink.c_str(), exe)) {
    static std::string str; str = exe;
    *buffer = (char *)str.c_str();
  }
  #elif defined(__FreeBSD__)
  int mib[4]; std::size_t s;
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = procId;
  if (sysctl(mib, 4, nullptr, &s, nullptr, 0) == 0) {
    std::string str1; str1.resize(s, '\0');
    char *exe = str1.data();
    if (sysctl(mib, 4, exe, &s, nullptr, 0) == 0) {
      static std::string str2; str2 = exe;
      *buffer = (char *)str2.c_str();
    }
  }
  #endif
}

const char *DirectoryGetCurrentWorking() {
  static std::string str;
  #if defined(_WIN32)
  wchar_t u8dname[MAX_PATH];
  if (GetCurrentDirectoryW(MAX_PATH, u8dname) != 0) {
    str = narrow(u8dname);
  }
  #else
  char dname[PATH_MAX];
  if (getcwd(dname, sizeof(dname)) != nullptr) {
    str = dname;
  }
  #endif
  return (char *)str.c_str();
}

bool DirectorySetCurrentWorking(const char *dname) {
  #if defined(_WIN32)
  std::wstring u8dname = widen(dname);
  return SetCurrentDirectoryW(u8dname.c_str());
  #else
  return chdir(dname);
  #endif
}

void CwdFromProcId(PROCID procId, char **buffer) {
  if (!ProcIdExists(procId)) return;
  #if defined(_WIN32)
  #if defined(EXE_INCLUDES)
  static std::string result;
  result = ExecuteXProcAndReadOuput("--cwd-from-pid", procId);
  *buffer = (char *)result.c_str();
  #else
  wchar_t *cwdbuf;
  CwdCmdEnvFromProcId(procId, &cwdbuf, MEMCWD);
  if (cwdbuf) {
    static std::string str; str = narrow(cwdbuf);
    *buffer = (char *)str.c_str();
    delete[] cwdbuf;
  }
  #endif
  #elif (defined(__APPLE__) && defined(__MACH__))
  proc_vnodepathinfo vpi;
  char cwd[PROC_PIDPATHINFO_MAXSIZE];
  if (proc_pidinfo(procId, PROC_PIDVNODEPATHINFO, 0, &vpi, sizeof(vpi)) > 0) {
    strcpy(cwd, vpi.pvi_cdir.vip_path);
    static std::string str; str = cwd;
    *buffer = (char *)str.c_str();
  }
  #elif (defined(__linux__) && !defined(__ANDROID__))
  char cwd[PATH_MAX];
  std::string symLink = "/proc/" + std::to_string(procId) + "/cwd";
  if (realpath(symLink.c_str(), cwd)) {
    static std::string str; str = cwd;
    *buffer = (char *)str.c_str();
  }
  #elif defined(__FreeBSD__)
  char cwd[PATH_MAX]; unsigned cntp;
  procstat *proc_stat = procstat_open_sysctl();
  kinfo_proc *proc_info = procstat_getprocs(proc_stat, KERN_PROC_PID, procId, &cntp);
  filestat_list *head = procstat_getfiles(proc_stat, proc_info, 0);
  filestat *fst;
  STAILQ_FOREACH(fst, head, next) {
    if (fst->fs_uflags & PS_FST_UFLAG_CDIR) {
      strcpy(cwd, fst->fs_path);
      static std::string str; str = cwd;
      *buffer = (char *)str.c_str();
    }
  }
  procstat_freefiles(proc_stat, head);
  procstat_freeprocs(proc_stat, proc_info);
  procstat_close(proc_stat);
  #endif
}

void FreeCmdline(char **buffer) {
  delete[] buffer;
}

std::vector<std::string> CmdlineVec1;
void CmdlineFromProcId(PROCID procId, char ***buffer, int *size) {
  if (!ProcIdExists(procId)) return;
  CmdlineVec1.clear(); int i = 0;
  #if defined(_WIN32)
  #if defined(EXE_INCLUDES)
  std::wstring wcmdstr; int cmdsize;
  std::string cmdstr = ExecuteXProcAndReadOuput("--cmd-from-pid", procId);
  wcmdstr = widen(cmdstr);
  wchar_t **cmdline = CommandLineToArgvW(wcmdstr.c_str(), &cmdsize);
  if (cmdline) {
    while (i < cmdsize) {
      CmdlineVec1.push_back(narrow(cmdline[i])); i++;
    }
    LocalFree(cmdline);
  }
  #else
  wchar_t *cmdbuf; int cmdsize;
  CwdCmdEnvFromProcId(procId, &cmdbuf, MEMCMD);
  if (cmdbuf) {
    wchar_t **cmdline = CommandLineToArgvW(cmdbuf, &cmdsize);
    if (cmdline) {
      while (i < cmdsize) {
        CmdlineVec1.push_back(narrow(cmdline[i])); i++;
      }
      LocalFree(cmdline);
    }
    delete[] cmdbuf;
  }
  #endif
  #elif (defined(__APPLE__) && defined(__MACH__))
  char **cmdline; int cmdsiz;
  CmdEnvFromProcId(procId, &cmdline, &cmdsiz, MEMCMD);
  if (cmdline) {
    for (int j = 0; j < cmdsiz; j++) {
      CmdlineVec1.push_back(cmdline[i]); i++;
    }
    delete[] cmdline;
  } else return;
  #elif (defined(__linux__) && !defined(__ANDROID__))
  PROCTAB *proc = openproc(PROC_FILLCOM | PROC_PID, &procId);
  if (proc_t *proc_info = readproc(proc, nullptr)) {
    while (proc_info->cmdline[i]) {
      CmdlineVec1.push_back(proc_info->cmdline[i]); i++;
    }
    freeproc(proc_info);
  }
  closeproc(proc);
  #elif defined(__FreeBSD__)
  procstat *proc_stat = procstat_open_sysctl(); unsigned cntp;
  kinfo_proc *proc_info = procstat_getprocs(proc_stat, KERN_PROC_PID, procId, &cntp);
  char **cmdline = procstat_getargv(proc_stat, proc_info, 0);
  if (cmdline) {
    for (int j = 0; cmdline[j]; j++) {
      CmdlineVec1.push_back(cmdline[j]); i++;
    }
  }
  procstat_freeargv(proc_stat);
  procstat_freeprocs(proc_stat, proc_info);
  procstat_close(proc_stat);
  #endif
  std::vector<char *> CmdlineVec2;
  for (int i = 0; i < CmdlineVec1.size(); i++)
    CmdlineVec2.push_back((char *)CmdlineVec1[i].c_str());
  char **arr = new char *[CmdlineVec2.size()]();
  std::copy(CmdlineVec2.begin(), CmdlineVec2.end(), arr);
  *buffer = arr; *size = i;
}

void ParentProcIdFromProcIdSkipSh(PROCID procId, PROCID *parentProcId) {
  ParentProcIdFromProcId(procId, parentProcId);
  #if !defined(_WIN32)
  char **cmdline; int cmdsize;
  CmdlineFromProcId(*parentProcId, &cmdline, &cmdsize);
  if (cmdline) {
    if (strcmp(cmdline[0], "/bin/sh") == 0) {
      ParentProcIdFromProcIdSkipSh(*parentProcId, parentProcId);
    }
    FreeCmdline(cmdline);
  }
  #endif
}

void ProcIdFromParentProcIdSkipSh(PROCID parentProcId, PROCID **procId, int *size) {
  ProcIdFromParentProcId(parentProcId, procId, size);
  #if !defined(_WIN32)
  if (procId) {
    for (int i = 0; i < *size; i++) {
      char **cmdline; int cmdsize;
      CmdlineFromProcId(*procId[i], &cmdline, &cmdsize);
      if (cmdline) {
        if (strcmp(cmdline[0], "/bin/sh") == 0) {
          ProcIdFromParentProcIdSkipSh(*procId[i], procId, size);
        }
        FreeCmdline(cmdline);
      }
    }
  }
  #endif
}

const char *EnvironmentGetVariable(const char *name) {
  static std::string str;
  #if defined(_WIN32)
  wchar_t buffer[32767];
  std::wstring u8name = widen(name);
  if (GetEnvironmentVariableW(u8name.c_str(), buffer, 32767) != 0) {
    str = narrow(buffer);
  }
  #else
  char *value = getenv(name);
  str = value ? : "";
  #endif
  return (char *)str.c_str();
}

bool EnvironmentSetVariable(const char *name, const char *value) {
  #if defined(_WIN32)
  std::wstring u8name = widen(name);
  std::wstring u8value = widen(value);
  if (strcmp(value, "") == 0) return (SetEnvironmentVariableW(u8name.c_str(), nullptr) != 0);
  return (SetEnvironmentVariableW(u8name.c_str(), u8value.c_str()) != 0);
  #else
  if (strcmp(value, "") == 0) return (unsetenv(name) == 0);
  return (setenv(name, value, 1) == 0);
  #endif
}

void FreeEnviron(char **buffer) {
  delete[] buffer;
}

std::vector<std::string> EnvironVec1; 
void EnvironFromProcId(PROCID procId, char ***buffer, int *size) {
  if (!ProcIdExists(procId)) return;
  EnvironVec1.clear(); int i = 0;
  #if defined(_WIN32)
  #if defined(EXE_INCLUDES)
  std::string envstr = ExecuteXProcAndReadOuput("--env-from-pid", procId);
  std::vector<std::string> newlinesplit = StringSplit(envstr, '\n');
  for (int j = 0; j < newlinesplit.size(); j++) {
    std::vector<std::string> equalssplit = StringSplitByFirstEqualsSign(newlinesplit[j]);
    for (int jj = 0; jj < equalssplit.size(); jj++) {
      if (jj == equalssplit.size() - 1) {
        std::string value = equalssplit[jj]; 
        if (j < newlinesplit.size() - 1)
        value = value.substr(1, value.length() - 3);
        else value = value.substr(1, value.length() - 2);
	    value = StringReplaceAll(value, "\\\"", "\"");
	    value = equalssplit[0] + "=" + value;
        EnvironVec1.push_back(value); i++;
      }
    }
  }
  #else
  wchar_t *wenviron;
  CwdCmdEnvFromProcId(procId, &wenviron, MEMENV);
  int j = 0;
  if (wenviron) {
    while (wenviron[j] != L'\0') {
      EnvironVec1.push_back(narrow(&wenviron[j])); i++;
      j += wcslen(wenviron + j) + 1;
    }
    delete[] wenviron;
  } else return;
  #endif
  #elif (defined(__APPLE__) && defined(__MACH__))
  char **environ; int envsiz;
  CmdEnvFromProcId(procId, &environ, &envsiz, MEMENV);
  if (environ) {
    for (int j = 0; j < envsiz; j++) {
      EnvironVec1.push_back(environ[i]); i++;
    }
    delete[] environ;
  } else return;
  #elif (defined(__linux__) && !defined(__ANDROID__))
  PROCTAB *proc = openproc(PROC_FILLENV | PROC_PID, &procId);
  if (proc_t *proc_info = readproc(proc, nullptr)) {
    while (proc_info->environ[i]) {
      EnvironVec1.push_back(proc_info->environ[i]); i++;
    }
    freeproc(proc_info);
  }
  closeproc(proc);
  #elif defined(__FreeBSD__)
  procstat *proc_stat = procstat_open_sysctl(); unsigned cntp;
  kinfo_proc *proc_info = procstat_getprocs(proc_stat, KERN_PROC_PID, procId, &cntp);
  char **environ = procstat_getenvv(proc_stat, proc_info, 0);
  if (environ) {
    for (int j = 0; environ[j]; j++) {
      EnvironVec1.push_back(environ[j]); i++;
    }
  }
  procstat_freeenvv(proc_stat);
  procstat_freeprocs(proc_stat, proc_info);
  procstat_close(proc_stat);
  #endif
  std::vector<char *> EnvironVec2;
  for (int i = 0; i < EnvironVec1.size(); i++)
    EnvironVec2.push_back((char *)EnvironVec1[i].c_str());
  char **arr = new char *[EnvironVec2.size()]();
  std::copy(EnvironVec2.begin(), EnvironVec2.end(), arr);
  *buffer = arr; *size = i;
}

void EnvironFromProcIdEx(PROCID procId, const char *name, char **value) {
  char **buffer; int size; *value = (char *)"\0";
  XProc::EnvironFromProcId(procId, &buffer, &size);
  if (buffer) {
    for (int i = 0; i < size; i++) {
      std::vector<std::string> equalssplit = StringSplitByFirstEqualsSign(buffer[i]);
      for (int j = 0; j < equalssplit.size(); j++) {
        std::string str1 = name;
        std::transform(equalssplit[0].begin(), equalssplit[0].end(), equalssplit[0].begin(), ::toupper);
        std::transform(str1.begin(), str1.end(), str1.begin(), ::toupper);
        if (j == equalssplit.size() - 1 && equalssplit[0] == str1) {
          static std::string str2; str2 = equalssplit[j];
          *value = (char *)str2.c_str();
        }
      }
    }
    XProc::FreeEnviron(buffer);
  }
}

} // namespace XProc
