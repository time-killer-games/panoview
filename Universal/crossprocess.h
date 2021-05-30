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

namespace CrossProcess {

#if !defined(_WIN32)
typedef int PROCID;
#else
typedef unsigned long PROCID;
#endif
typedef struct {
  PROCID ProcessId;
  char *ExecutableImageFilePath;
  char *CurrentWorkingDirectory;
  PROCID ParentProcessId;
  PROCID *ChildProcessId;
  int ChildProcessIdLength;
  char **CommandLine;
  int CommandLineLength;
  char **Environment;
  int EnvironmentLength;
} PROCINFO;

void ProcIdEnumerate(PROCID **procId, int *size);
void FreeProcIds(PROCID *procId);
void ProcIdFromSelf(PROCID *procId);
void ParentProcIdFromSelf(PROCID *parentProcId);
bool ProcIdExists(PROCID procId);
bool ProcIdKill(PROCID procId);
void ParentProcIdFromProcId(PROCID procId, PROCID *parentProcId);
void ProcIdFromParentProcId(PROCID parentProcId, PROCID **procId, int *size);
void ExeFromProcId(PROCID procId, char **buffer);
const char *DirectoryGetCurrentWorking();
bool DirectorySetCurrentWorking(const char *dname);
void CwdFromProcId(PROCID procId, char **buffer);
void FreeCmdline(char **buffer);
void CmdlineFromProcId(PROCID procId, char ***buffer, int *size);
void ParentProcIdFromProcIdSkipSh(PROCID procId, PROCID *parentProcId);
void ProcIdFromParentProcIdSkipSh(PROCID parentProcId, PROCID **procId, int *size);
const char *EnvironmentGetVariable(const char *name);
bool EnvironmentSetVariable(const char *name, const char *value);
void FreeEnviron(char **buffer);
void EnvironFromProcId(PROCID procId, char ***buffer, int *size);
void EnvironFromProcIdEx(PROCID procId, const char *name, char **value);
PROCINFO *ProcInfoFromProcId(PROCID procId);
void FreeProcInfo(PROCINFO *procInfo);

inline PROCID ProcessId(PROCINFO *procInfo) { return procInfo->ProcessId; }
inline char *ExecutableImageFilePath(PROCINFO *procInfo) { return procInfo->ExecutableImageFilePath; }
inline char *CurrentWorkingDirectory(PROCINFO *procInfo) { return procInfo->CurrentWorkingDirectory; }
inline PROCID ParentProcessId(PROCINFO *procInfo) { return procInfo->ParentProcessId; }
inline PROCID *ChildProcessId(PROCINFO *procInfo) { return procInfo->ChildProcessId; }
inline PROCID ChildProcessId(PROCINFO *procInfo, int i) { return procInfo->ChildProcessId[i]; }
inline int ChildProcessIdLength(PROCINFO *procInfo) { return procInfo->ChildProcessIdLength; }
inline char **CommandLine(PROCINFO *procInfo) { return procInfo->CommandLine; }
inline char *CommandLine(PROCINFO *procInfo, int i) { return procInfo->CommandLine[i]; }
inline int CommandLineLength(PROCINFO *procInfo) { return procInfo->CommandLineLength; }
inline char **Environment(PROCINFO *procInfo) { return procInfo->Environment; }
inline char *Environment(PROCINFO *procInfo, int i) { return procInfo->Environment[i]; }
inline int EnvironmentLength(PROCINFO *procInfo) { return procInfo->EnvironmentLength; }

} // namespace CrossProcess

