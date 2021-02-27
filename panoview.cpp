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
#include <signal.h>
#include <unistd.h>
#endif

#if OS_PLATFORM == OS_WINDOWS
#include <windows.h>
#include <Objbase.h>
#include <tlhelp32.h>
#include <winternl.h>
#include <psapi.h>
#include <GL/glut.h>
#include <SDL2/SDL.h>
#elif OS_PLATFORM == OS_MACOS
#include <sys/sysctl.h>
#include <sys/proc_info.h>
#include <libproc.h>
#include <CoreGraphics/CoreGraphics.h>
#include <GLUT/glut.h>
#elif OS_PLATFORM == OS_LINUX
#include <proc/readproc.h>
#include <GL/glut.h>
#include <SDL2/SDL.h>
#elif OS_PLATFORM == OS_FREEBSD
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/user.h>
#include <libprocstat.h>
#include <libutil.h>
#include <GL/glut.h>
#include <SDL2/SDL.h>
#endif

#define OS_32BIT 32
#define OS_64BIT 64
#if UINTPTR_MAX == 0xffffffff
#define OS_ARCHITECTURE OS_32BIT
#elif UINTPTR_MAX == 0xffffffffffffffff
#define OS_ARCHITECTURE OS_64BIT
#else
#error Unexpected value for UINTPTR_MAX; OS_ARCHITECTURE, as a result, is undefined!
#endif

#if OS_PLATFORM == OS_WINDOWS
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#if OS_UNIXLIKE == true
typedef pid_t PROCID;
#else
typedef DWORD PROCID;
#endif

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

#if OS_PLATFORM == OS_WINDOWS
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

NTSTATUS NtQueryInformationProcessEx(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, 
  PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength) {
  if (IsX86Process(ProcessHandle) || !IsX86Process(GetCurrentProcess())) {
    typedef NTSTATUS (__stdcall *NTQIP)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
    NTQIP NtQueryInformationProcess = NTQIP(GetProcAddress(
      GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess"));
    return NtQueryInformationProcess(ProcessHandle, ProcessInformationClass, 
      ProcessInformation, ProcessInformationLength, ReturnLength);
  } else {
    typedef NTSTATUS (__stdcall *NTWOW64QIP64)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
    NTWOW64QIP64 NtWow64QueryInformationProcess64 = NTWOW64QIP64(GetProcAddress(
      GetModuleHandleW(L"ntdll.dll"), "NtWow64QueryInformationProcess64"));
    return NtWow64QueryInformationProcess64(ProcessHandle, ProcessInformationClass, 
      ProcessInformation, ProcessInformationLength, ReturnLength);
  }
  return 0;
}

DWORD ReadProcessMemoryEx(HANDLE ProcessHandle, PVOID64 BaseAddress, PVOID Buffer, 
  ULONG64 Size, PULONG64 NumberOfBytesRead) {
  if (IsX86Process(ProcessHandle) || !IsX86Process(GetCurrentProcess())) {
    return ReadProcessMemory(ProcessHandle, BaseAddress, Buffer, 
      Size, (SIZE_T *)NumberOfBytesRead);
  } else {
    typedef DWORD (__stdcall *NTWOW64RVM64)(HANDLE, PVOID64, PVOID, ULONG64, PULONG64);
    NTWOW64RVM64 NtWow64ReadVirtualMemory64 = NTWOW64RVM64(GetProcAddress(
      GetModuleHandleW(L"ntdll.dll"), "NtWow64ReadVirtualMemory64"));
    return NtWow64ReadVirtualMemory64(ProcessHandle, BaseAddress, Buffer, 
      Size, NumberOfBytesRead);
  }
  return 0;
}

void CwdCmdEnvFromProcId(PROCID procId, wchar_t **buffer, int type) {
  HANDLE procHandle = OpenProcessWithDebugPrivilege(procId);
  if (procHandle == nullptr) return;
  PEB peb; SIZE_T nRead; ULONG len = 0;
  PROCESS_BASIC_INFORMATION pbi;
  RTL_USER_PROCESS_PARAMETERS upp;
  NTSTATUS status = NtQueryInformationProcessEx(procHandle, ProcessBasicInformation, &pbi, sizeof(pbi), &len);
  if (status) { CloseHandle(procHandle); return; }
  ReadProcessMemoryEx(procHandle, pbi.PebBaseAddress, &peb, sizeof(peb), (PULONG64)&nRead);
  if (!nRead) { CloseHandle(procHandle); return; }
  ReadProcessMemoryEx(procHandle, peb.ProcessParameters, &upp, sizeof(upp), (PULONG64)&nRead);
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
  ReadProcessMemoryEx(procHandle, buf, res, len, (PULONG64)&nRead); res[len / 2] = L'\0';
  if (!nRead) { delete[] res; CloseHandle(procHandle); *buffer = nullptr; return; }
  *buffer = res;
}
#endif

#if OS_PLATFORM == OS_MACOS
enum MEMTYP {
  MEMCMD,
  MEMENV
};

void CmdEnvFromProcId(PROCID procId, char ***buffer, int *size, int type) {
  static vector<string> vec1; int i = 0;
  int argmax, nargs; size_t s;
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
    if (type && j < nargs) { 
      vec1.push_back(sp); i++;
    } else if (!type && j >= nargs) {
      vec1.push_back(sp); i++;
    }
    sp += strlen(sp) + 1; j++;
  } 
  vector<char *> vec2;
  for (int j = 0; j <= vec1.size(); j++)
    vec2.push_back((char *)vec1[j].c_str());
  char **arr = new char *[vec2.size()]();
  std::copy(vec2.begin(), vec2.end(), arr);
  *buffer = arr; *size = i;
  if (procargs) {
    free(procargs);
  }
}
#endif

namespace XProc {

const char *EnvironmentGetVariable(const char *name) {
  #if OS_PLATFORM == OS_WINDOWS
  static string value;
  wchar_t buffer[32767];
  wstring u8name = widen(name);
  if (GetEnvironmentVariableW(u8name.c_str(), buffer, 32767) != 0) {
    value = narrow(buffer);
  }
  return value.c_str();
  #else
  char *value = getenv(name);
  return value ? value : "";
  #endif
}

bool EnvironmentSetVariable(const char *name, const char *value) {
  #if OS_PLATFORM == OS_WINDOWS
  wstring u8name = widen(name);
  wstring u8value = widen(value);
  if (strcmp(value, "") == 0) return (SetEnvironmentVariableW(u8name.c_str(), nullptr) != 0);
  return (SetEnvironmentVariableW(u8name.c_str(), u8value.c_str()) != 0);
  #else
  if (strcmp(value, "") == 0) return (unsetenv(name) == 0);
  return (setenv(name, value, 1) == 0);
  #endif
}

const char *DirectoryGetCurrentWorking() {
  #if OS_PLATFORM == OS_WINDOWS
  static string dname;
  wchar_t u8dname[MAX_PATH];
  if (GetCurrentDirectoryW(MAX_PATH, u8dname) != 0) {
	dname = narrow(u8dname);
    return dname.c_str();
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

bool DirectorySetCurrentWorking(const char *dname) {
  #if OS_PLATFORM == OS_WINDOWS
  wstring u8dname = widen(dname);
  return SetCurrentDirectoryW(u8dname.c_str());
  #else
  return chdir(dname);
  #endif
}

#if OS_UNIXLIKE == true
#if OS_PLATFORM == OS_MACOS || OS_PLATFORM == OS_LINUX
bool ProcIdExists(PROCID procId);
#endif
#endif

void ProcIdEnumerate(PROCID **procId, int *size) {
  vector<PROCID> vec; int i = 0;
  #if OS_PLATFORM == OS_WINDOWS
  HANDLE hp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  PROCESSENTRY32 pe = { 0 };
  pe.dwSize = sizeof(PROCESSENTRY32);
  if (Process32First(hp, &pe)) {
    do {
      vec.push_back(pe.th32ProcessID); i++;
    } while (Process32Next(hp, &pe));
  }
  CloseHandle(hp);
  #elif OS_PLATFORM == OS_MACOS
  if (ProcIdExists(0)) { vec.push_back(0); i++; }
  int cntp = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
  vector<PROCID> proc_info(cntp);
  std::fill(proc_info.begin(), proc_info.end(), 0);
  proc_listpids(PROC_ALL_PIDS, 0, &proc_info[0], sizeof(PROCID) * cntp);
  for (int j = cntp; j > 0; j--) {
    if (proc_info[j] == 0) { continue; }
    vec.push_back(proc_info[j]); i++;
  }
  #elif OS_PLATFORM == OS_LINUX
  if (ProcIdExists(0)) { vec.push_back(0); i++; }
  PROCTAB *proc = openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS);
  while (proc_t *proc_info = readproc(proc, nullptr)) {
    vec.push_back(proc_info->tgid); i++;
    freeproc(proc_info);
  }
  closeproc(proc);
  #elif OS_PLATFORM == OS_FREEBSD
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
  #if OS_UNIXLIKE == true
  *procId = getpid();
  #elif OS_PLATFORM == OS_WINDOWS
  *procId = GetCurrentProcessId();
  #endif
}

#if OS_PLATFORM == OS_WINDOWS
void ParentProcIdFromProcId(PROCID procId, PROCID *parentProcId);
#endif

void ParentProcIdFromSelf(PROCID *parentProcId) {
  #if OS_UNIXLIKE == true
  *parentProcId = getppid();
  #elif OS_PLATFORM == OS_WINDOWS
  ParentProcIdFromProcId(GetCurrentProcessId(), parentProcId);
  #endif
}

bool ProcIdExists(PROCID procId) {
  #if OS_UNIXLIKE == true
  return (kill(procId, 0) == 0);
  #elif OS_PLATFORM == OS_WINDOWS
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
  #if OS_UNIXLIKE == true
  return (kill(procId, SIGKILL) == 0);
  #elif OS_PLATFORM == OS_WINDOWS
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
  #if OS_PLATFORM == OS_WINDOWS
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
  #elif OS_PLATFORM == OS_MACOS
  proc_bsdinfo proc_info;
  if (proc_pidinfo(procId, PROC_PIDTBSDINFO, 0, &proc_info, sizeof(proc_info)) > 0) {
    *parentProcId = proc_info.pbi_ppid;
  }
  #elif OS_PLATFORM == OS_LINUX
  PROCTAB *proc = openproc(PROC_FILLSTATUS | PROC_PID, &procId);
  if (proc_t *proc_info = readproc(proc, nullptr)) { 
    *parentProcId = proc_info->ppid;
    freeproc(proc_info);
  }
  closeproc(proc);
  #elif OS_PLATFORM == OS_FREEBSD
  if (kinfo_proc *proc_info = kinfo_getproc(procId)) {
    *parentProcId = proc_info->ki_ppid;
    free(proc_info);
  }
  #endif
}

void ProcIdFromParentProcId(PROCID parentProcId, PROCID **procId, int *size) {
  vector<PROCID> vec; int i = 0;
  #if OS_PLATFORM == OS_WINDOWS
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
  #elif OS_PLATFORM == OS_MACOS
  int cntp = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
  vector<PROCID> proc_info(cntp);
  std::fill(proc_info.begin(), proc_info.end(), 0);
  proc_listpids(PROC_ALL_PIDS, 0, &proc_info[0], sizeof(PROCID) * cntp);
  for (int j = cntp; j > 0; j--) {
    if (proc_info[j] == 0) { continue; }
    PROCID ppid; ParentProcIdFromProcId(proc_info[j], &ppid);
    if (ppid == parentProcId) {
      vec.push_back(proc_info[j]); i++;
    }
  }
  #elif OS_PLATFORM == OS_LINUX
  PROCTAB *proc = openproc(PROC_FILLSTAT);
  while (proc_t *proc_info = readproc(proc, nullptr)) {
    if (proc_info->ppid == parentProcId) {
      vec.push_back(proc_info->tgid); i++;
    }
    freeproc(proc_info);
  }
  closeproc(proc);
  #elif OS_PLATFORM == OS_FREEBSD
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
  #if OS_PLATFORM == OS_WINDOWS
  HANDLE procHandle = OpenProcessWithDebugPrivilege(procId);
  if (procHandle == nullptr) return;
  wchar_t exe[MAX_PATH]; DWORD size = MAX_PATH;
  if (QueryFullProcessImageNameW(procHandle, 0, exe, &size)) {
    static string str; str = narrow(exe);
    *buffer = (char *)str.c_str();
  }
  CloseHandle(procHandle);
  #elif OS_PLATFORM == OS_MACOS
  char exe[PROC_PIDPATHINFO_MAXSIZE];
  if (proc_pidpath(procId, exe, sizeof(exe)) > 0) {
    static string str; str = exe;
    *buffer = (char *)str.c_str();
  }
  #elif OS_PLATFORM == OS_LINUX
  char exe[PATH_MAX]; 
  string symLink = string("/proc/") + to_string(procId) + string("/exe");
  if (realpath(symLink.c_str(), exe)) {
    static string str; str = exe;
    *buffer = (char *)str.c_str();
  }
  #elif OS_PLATFORM == OS_FREEBSD
  int mib[4]; size_t s;
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = procId;
  if (sysctl(mib, 4, nullptr, &s, nullptr, 0) == 0) {
    string str1; str1.resize(s, '\0');
    char *exe = str1.data();
    if (sysctl(mib, 4, exe, &s, nullptr, 0) == 0) {
      static string str2; str2 = exe;
      *buffer = (char *)str2.c_str();
    }
  }
  #endif
}

void CwdFromProcId(PROCID procId, char **buffer) {
  if (!ProcIdExists(procId)) return;
  #if OS_PLATFORM == OS_WINDOWS
  wchar_t *cwdbuf;
  CwdCmdEnvFromProcId(procId, &cwdbuf, MEMCWD);
  if (cwdbuf) {
    static string str; str = narrow(cwdbuf);
    *buffer = (char *)str.c_str();
    delete[] cwdbuf;
  }
  #elif OS_PLATFORM == OS_MACOS
  proc_vnodepathinfo vpi;
  char cwd[PROC_PIDPATHINFO_MAXSIZE];
  if (proc_pidinfo(procId, PROC_PIDVNODEPATHINFO, 0, &vpi, sizeof(vpi)) > 0) {
    strcpy(cwd, vpi.pvi_cdir.vip_path);
    static string str; str = cwd;
    *buffer = (char *)str.c_str();
  }
  #elif OS_PLATFORM == OS_LINUX
  char cwd[PATH_MAX];
  string symLink = string("/proc/") + to_string(procId) + string("/cwd");
  if (realpath(symLink.c_str(), cwd)) {
    static string str; str = cwd;
    *buffer = (char *)str.c_str();
  }
  #elif OS_PLATFORM == OS_FREEBSD
  char cwd[PATH_MAX]; unsigned cntp;
  procstat *proc_stat = procstat_open_sysctl();
  kinfo_proc *proc_info = procstat_getprocs(proc_stat, KERN_PROC_PID, procId, &cntp);
  filestat_list *head = procstat_getfiles(proc_stat, proc_info, 0);
  filestat *fst;
  STAILQ_FOREACH(fst, head, next) {
    if (fst->fs_uflags & PS_FST_UFLAG_CDIR) {
      strcpy(cwd, fst->fs_path);
      static string str; str = cwd;
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

void CmdlineFromProcId(PROCID procId, char ***buffer, int *size) {
  if (!ProcIdExists(procId)) return;
  static vector<string> vec1; int i = 0;
  #if OS_PLATFORM == OS_WINDOWS
  wchar_t *cmdbuf; int cmdsize;
  CwdCmdEnvFromProcId(procId, &cmdbuf, MEMCMD);
  if (cmdbuf) {
    wchar_t **cmdline = CommandLineToArgvW(cmdbuf, &cmdsize);
    if (cmdline) {
      while (i < cmdsize) {
        vec1.push_back(narrow(cmdline[i])); i++;
      }
      LocalFree(cmdline);
    }
    delete[] cmdbuf;
  }
  #elif OS_PLATFORM == OS_MACOS
  char **cmdline; int cmdsiz;
  CmdEnvFromProcId(procId, &cmdline, &cmdsiz, MEMCMD);
  if (cmdline) {
    for (int j = 0; j < cmdsiz; j++) {
      vec1.push_back(cmdline[i]); i++;
    }
    delete[] cmdline;
  } else return;
  #elif OS_PLATFORM == OS_LINUX
  PROCTAB *proc = openproc(PROC_FILLCOM | PROC_PID, &procId);
  if (proc_t *proc_info = readproc(proc, nullptr)) {
    while (proc_info->cmdline[i]) {
      vec1.push_back(proc_info->cmdline[i]); i++;
    }
    freeproc(proc_info);
  }
  closeproc(proc);
  #elif OS_PLATFORM == OS_FREEBSD
  procstat *proc_stat = procstat_open_sysctl(); unsigned cntp;
  kinfo_proc *proc_info = procstat_getprocs(proc_stat, KERN_PROC_PID, procId, &cntp);
  char **cmdline = procstat_getargv(proc_stat, proc_info, 0);
  if (cmdline) {
    for (int j = 0; cmdline[j]; j++) {
      vec1.push_back(cmdline[j]); i++;
    }
  }
  procstat_freeargv(proc_stat);
  procstat_freeprocs(proc_stat, proc_info);
  procstat_close(proc_stat);
  #endif
  vector<char *> vec2;
  for (int i = 0; i <= vec1.size(); i++)
    vec2.push_back((char *)vec1[i].c_str());
  char **arr = new char *[vec2.size()]();
  std::copy(vec2.begin(), vec2.end(), arr);
  *buffer = arr; *size = i;
}

void FreeEnviron(char **buffer) {
  delete[] buffer;
}

void EnvironFromProcId(PROCID procId, char ***buffer, int *size) {
  if (!ProcIdExists(procId)) return;
  static vector<string> vec1; int i = 0;
  #if OS_PLATFORM == OS_WINDOWS
  wchar_t *wenviron = nullptr;
  CwdCmdEnvFromProcId(procId, &wenviron, MEMENV);
  int j = 0;
  if (wenviron) {
    while (wenviron[j] != L'\0') {
      vec1.push_back(narrow(&wenviron[j])); i++;
      j += wcslen(wenviron + j) + 1;
    }
    delete[] wenviron;
  } else return;
  #elif OS_PLATFORM == OS_MACOS
  char **environ; int envsiz;
  CmdEnvFromProcId(procId, &environ, &envsiz, MEMENV);
  if (environ) {
    for (int j = 0; j < envsiz; j++) {
      vec1.push_back(environ[i]); i++;
    }
    delete[] environ;
  } else return;
  #elif OS_PLATFORM == OS_LINUX
  PROCTAB *proc = openproc(PROC_FILLENV | PROC_PID, &procId);
  if (proc_t *proc_info = readproc(proc, nullptr)) {
    while (proc_info->environ[i]) {
      vec1.push_back(proc_info->environ[i]); i++;
    }
    freeproc(proc_info);
  }
  closeproc(proc);
  #elif OS_PLATFORM == OS_FREEBSD
  procstat *proc_stat = procstat_open_sysctl(); unsigned cntp;
  kinfo_proc *proc_info = procstat_getprocs(proc_stat, KERN_PROC_PID, procId, &cntp);
  char **environ = procstat_getenvv(proc_stat, proc_info, 0);
  if (environ) {
    for (int j = 0; environ[j]; j++) {
      vec1.push_back(environ[j]); i++;
    }
  }
  procstat_freeenvv(proc_stat);
  procstat_freeprocs(proc_stat, proc_info);
  procstat_close(proc_stat);
  #endif
  vector<char *> vec2;
  for (int i = 0; i <= vec1.size(); i++)
    vec2.push_back((char *)vec1[i].c_str());
  char **arr = new char *[vec2.size()]();
  std::copy(vec2.begin(), vec2.end(), arr);
  *buffer = arr; *size = i;
}

void EnvironFromProcIdEx(PROCID procId, const char *name, char **value) {
  char **buffer; int size;
  XProc::EnvironFromProcId(procId, &buffer, &size);
  if (buffer) {
    for (int i = 0; i < size; i++) {
      vector<string> equalssplit = StringSplitByFirstEqualsSign(buffer[i]);
      for (int j = 0; j < equalssplit.size(); j++) {
        string str1 = name;
        std::transform(equalssplit[0].begin(), equalssplit[0].end(), equalssplit[0].begin(), ::toupper);
        std::transform(str1.begin(), str1.end(), str1.begin(), ::toupper);
        if (j == equalssplit.size() - 1 && equalssplit[0] == str1) {
          static string str2; str2 = equalssplit[j];
          *value = (char *)str2.c_str();
        }
      }
    }
    XProc::FreeEnviron(buffer);
  }
}

} // namespace XProc

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

void display() {
  string firstline;
  std::ifstream conf("panoview.conf");
  if (conf.is_open()) {
    if (std::getline(conf, firstline)) {
      if (firstline.substr(0, 3) == "ID=" && 
        firstline.length() > 3 && firstline[4] != ' ') {
        ID = (PROCID)strtoul(StringReplaceAll(firstline, "ID=", "").c_str(), nullptr, 10);
        if (ID != 0 && XProc::ProcIdExists(ID)) {
          char *texture; XProc::EnvironFromProcIdEx(ID, "PANORAMA_TEXTURE", &texture);
          const char *panorama = XProc::EnvironmentGetVariable("PANORAMA_TEXTURE");
          char *pointer; XProc::EnvironFromProcIdEx(ID, "PANORAMA_POINTER", &pointer);
          const char *cursor = XProc::EnvironmentGetVariable("PANORAMA_POINTER");
          if (texture && strcmp(texture, panorama) != 0) { LoadPanorama(texture); }
          if (texture && strcmp(pointer, cursor) != 0) { LoadCursor(pointer); }
        }
      }
    }
    conf.close();
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
  if (argc == 2) { return 0; }
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
  string panorama; if (argc == 1) {
  #if OS_PLATFORM == OS_WINDOWS
  dialog_module::widget_set_owner((char *)std::to_string((std::uintptr_t)GetDesktopWindow()).c_str());
  #else
  dialog_module::widget_set_owner((char *)"-1");
  #endif

  dialog_module::widget_set_icon((char *)"icon.png");
  panorama = dialog_module::get_open_filename_ext((char *)"Portable Network Graphic (*.png)|*.png;*.PNG",
  (char *)"burning_within.png", (char *)"examples", (char *)"Choose a 360 Degree Cylindrical Panoramic Image");
  if (strcmp(panorama.c_str(), "") == 0) { glutDestroyWindow(window); exit(0); } } else { panorama = argv[1]; }
  string cursor = (argc > 2) ? argv[2] : "cursor.png";

  glClearDepth(1);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glShadeModel(GL_SMOOTH);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

  LoadPanorama(panorama.c_str()); LoadCursor(cursor.c_str());
  XProc::EnvironmentSetVariable("PANORAMA_TEXTURE", panorama.c_str());
  XProc::EnvironmentSetVariable("PANORAMA_POINTER", cursor.c_str());

  glutSetCursor(GLUT_CURSOR_NONE);
  glutKeyboardFunc(keyboard);
  glutMouseFunc(mouse);
  glutTimerFunc(0, timer, 0);
  
  string firstline;
  std::ifstream conf("panoview.conf");
  if (conf.is_open()) {
    if (std::getline(conf, firstline)) {
      if (firstline.substr(0, 3) == "ID=" && 
        firstline.length() > 3 && firstline[4] != ' ') {
        ID = (PROCID)strtoul(StringReplaceAll(firstline, "ID=", "").c_str(), nullptr, 10);
        if (ID != 0 && XProc::ProcIdExists(ID)) {
          char *xvalue; XProc::EnvironFromProcIdEx(ID, "PANORAMA_XANGLE", &xvalue);
          if (xvalue) XProc::EnvironmentSetVariable("PANORAMA_XANGLE", xvalue);
          char *yvalue; XProc::EnvironFromProcIdEx(ID, "PANORAMA_YANGLE", &yvalue);
          if (yvalue) XProc::EnvironmentSetVariable("PANORAMA_YANGLE", yvalue);
        }
      }
    }
    conf.close();
  }

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
  #elif OS_PLATFORM == OS_MACOS
  CGDisplayHideCursor(kCGDirectMainDisplay);
  #endif
  
  glutMainLoop();
  return 0;
}
