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

#if !defined(_WIN32)
#define PROCID int
#else
#define PROCID unsigned long
#endif

namespace CrossProcess {

typedef struct {
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

} // namespace CrossProcess

