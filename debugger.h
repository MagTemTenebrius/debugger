/*
    Файл debugger.h.

    Заголовочный файл модуля debugger.c.

    Маткин Илья Александрович   01.10.2014
*/

#ifndef _DEBUGGER_H_
#define _DEBUGGER_H_

#include <windows.h>

//***************************************************************

BOOL CreateDebugProcess (char *procName);

void DebugRun (void);

void EventCreateProcess (DWORD pid, DWORD tid, LPCREATE_PROCESS_DEBUG_INFO procDebugInfo);
void EventExitProcess (DWORD pid, DWORD tid, LPEXIT_PROCESS_DEBUG_INFO procDebugInfo);
void EventCreateThread (DWORD pid, DWORD tid, LPCREATE_THREAD_DEBUG_INFO threadDebugInfo);
void EventExitThread (DWORD pid, DWORD tid, LPEXIT_THREAD_DEBUG_INFO threadDebugInfo);
void EventLoadDll (DWORD pid, DWORD tid, LPLOAD_DLL_DEBUG_INFO dllDebugInfo);
void EventUnloadDll (DWORD pid, DWORD tid, LPUNLOAD_DLL_DEBUG_INFO dllDebugInfo);
void EventOutputDebugString (DWORD pid, DWORD tid, LPOUTPUT_DEBUG_STRING_INFO debugStringInfo);
DWORD EventException (DWORD pid, DWORD tid, LPEXCEPTION_DEBUG_INFO exceptionDebugInfo);

//***************************************************************


#endif  // _DEBUGGER_H_
