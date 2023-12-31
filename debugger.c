/*
    ������ debugger.c.

    �������� �������� ������� �������.

    ������ ���� �������������   01.10.2014
*/


#include <windows.h>

#include "disas.h"
#include "debugger.h"
#include "string_util.h"
#include "logger.h"


//***************************************************************

#define BREAK_TYPE_SOFT 1
#define BREAK_TYPE_HARD 2

#define BREAK_STATE_DISABLE 0
#define BREAK_STATE_ENABLE  1

typedef struct _BreakEntry {

    unsigned int state;
    unsigned int type;
    void*        addr;
    unsigned int saveByte;
    struct _BreakEntry *next;

} BreakEntry;

typedef struct _BreakList {

    BreakEntry *first;
} BreakList;

typedef struct _Instruction {
    unsigned int offset;
    char* instruction;
} Instruction;


typedef struct _Dr7Reg {
unsigned int L0:1;
unsigned int G0:1;
unsigned int L1:1;
unsigned int G1:1;
unsigned int L2:1;
unsigned int G2:1;
unsigned int L3:1;
unsigned int G3:1;
unsigned int LE:1;   // 8
unsigned int GE:1;   // 9
unsigned int stub:3; // 10-12
unsigned int GD:1;   // 13
unsigned int stub2:2;// 14-15
unsigned int RW0:2;  // 16-17
unsigned int len0:2; // 18-19
unsigned int RW1:2;  // 20-21
unsigned int len1:2; // 22-23
unsigned int RW2:2;  // 24-25
unsigned int len2:2; // 26-27
unsigned int RW3:2;  // 28-29
unsigned int len3:2; // 30-31

} Dr7Reg;



#define DEBUGGER_STATE_NOT_TRACE    0
#define DEBUGGER_STATE_BREAK_SOFT   1
#define DEBUGGER_STATE_BREAK_HARD   2
#define DEBUGGER_STATE_TRACE        3

typedef struct _DebuggerState {

    unsigned int state;
    BOOL isTrace;
    BOOL isRun;
    HANDLE debugProcess;
    BreakList breakpointList;

} DebuggerState;

// ������� ���� ��������� ����������

typedef struct _Library {
    WCHAR* libName;
    DWORD64 libPid;
    DWORD libTid;
    LPVOID libAddr;
    struct _Library* nextLib; // ����� ����������� ������, ��� ��������� �� ����. ����������
} Library;

// �����

typedef struct _Thread {
    DWORD64 thPID;
    DWORD64 thID;
    LPTHREAD_START_ROUTINE thAddr;
    struct _Thread* nextTh;
} Thread;

//***************************************************************


DebuggerState glDebugger;

// ������� ���� ������ ���������
Library* libraryes = NULL;
// ������� ���� ������ �������
Thread* threads = NULL;

// ������ ���������
Logger* logger = NULL;

// ����� ���������� �����������
char *myInstructions[] = { "sub", "add", "xor", "or" , "and" };
int instrCount = 5;

//***************************************************************


//
// ������� ������� ������������ �������.
//
BOOL CreateDebugProcess (char *procName) {
  
STARTUPINFO startupInfo;
PROCESS_INFORMATION procInfo;
BOOL ret;
DebuggerState *debugger = &glDebugger;
char logName[100];

    
    RtlZeroMemory (&startupInfo, sizeof(startupInfo));
    RtlZeroMemory (&procInfo, sizeof(procInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_SHOWNORMAL;


    // �������������� ��� ������
    //printf("AAAAAAAAAAAAAAAAAAAA\n");
    sprintf(logName, "%s.txt", procName);
    logger = initLog(logName);
    
    ret = CreateProcess (procName, 
                         NULL,
                         NULL,
                         NULL,
                         TRUE,
                         DEBUG_ONLY_THIS_PROCESS,
                         NULL,
                         NULL,
                         &startupInfo,
                         &procInfo);

    debugger->debugProcess = procInfo.hProcess;

    CloseHandle (procInfo.hThread);

    return ret;
}


//--------------------


//
// ��������� ����� �������� � ������ ������.
//
void InsertBreakHeadList (BreakList *list, unsigned int type, void* addr, unsigned int saveByte) {

BreakEntry *newEntry;

    newEntry = (BreakEntry*) malloc (sizeof(BreakEntry));
    if (!newEntry) {
        return;
        }

    newEntry->state = BREAK_STATE_ENABLE;
    newEntry->type = type;
    newEntry->addr = addr;
    newEntry->saveByte = saveByte;

    newEntry->next = list->first;
    list->first = newEntry;

    return;
}


//--------------------


//
// ����� ������������� ����� �������� � ������ �� ������.
//
BreakEntry *FindBreakByAddr (BreakList *list, void *addr) {

BreakEntry *cur;

    for (cur = list->first; cur != NULL; cur = cur->next) {
        if (cur->addr == addr) {
            return cur;
            }
        }

    return NULL;
}


//--------------------


//
// ������������� ����� �������� �� �����.
//
void SetBreakpoint (DWORD tid, void *addr, unsigned int type) {

BreakEntry *breakEntry;
DebuggerState *debugger = &glDebugger;

    breakEntry = FindBreakByAddr (&debugger->breakpointList, addr);
    if (breakEntry) {
        }
    else {
        if (type == BREAK_TYPE_SOFT) {
            unsigned int saveByte = 0;
            ReadProcessMemory (debugger->debugProcess, addr, &saveByte, 1, NULL);
            WriteProcessMemory (debugger->debugProcess, addr, "\xCC", 1, NULL);
            InsertBreakHeadList (&debugger->breakpointList, type, addr, saveByte);
            }
        else {
            CONTEXT context;
            HANDLE thread = OpenThread (THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, tid);
            if (thread != INVALID_HANDLE_VALUE) {
                context.ContextFlags = CONTEXT_ALL;
                GetThreadContext (thread, &context);
                context.Dr0 = addr;
                (*((Dr7Reg*)&context.Dr7)).L0 = 1;
                (*((Dr7Reg*)&context.Dr7)).LE = 1;
				(*((Dr7Reg*)&context.Dr7)).G0 = 1;
				(*((Dr7Reg*)&context.Dr7)).GE = 1;
                (*((Dr7Reg*)&context.Dr7)).len0 = 0;
                (*((Dr7Reg*)&context.Dr7)).RW0 = 0;
                SetThreadContext (thread, &context);
                CloseHandle (thread);
                }
            InsertBreakHeadList (&debugger->breakpointList, type, addr, 0);
            }
        }

    return;
}


//--------------------


//
// ������� ����� �������� �� ������.
//
void RemoveBreakFromList (BreakList *list, void *addr) {

BreakEntry *cur = NULL;
BreakEntry *last = NULL;

    for (cur = list->first; cur != NULL; last = cur, cur = cur->next) {
        if (cur->addr == addr) {
            break;
            }
        }
    if (cur) {
        if (last) {
            last->next = cur->next;
            }
        else {
            list->first = cur->next;
            }

        free (cur);
        }

    return;
}


//--------------------


//
// ������� ����� �������� � ������� �� �� ������.
//
void DeleteBreakpoint (DWORD tid, void *addr) {

BreakEntry *breakEntry;
DebuggerState *debugger = &glDebugger;

    breakEntry = FindBreakByAddr (&debugger->breakpointList, addr);
    if (breakEntry) {
        if (breakEntry->type == BREAK_TYPE_SOFT) {
            WriteProcessMemory (debugger->debugProcess, (PVOID)addr, &breakEntry->saveByte, 1, NULL);
            }
        else {
            CONTEXT context;
            HANDLE thread = OpenThread (THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, tid);
            if (thread != INVALID_HANDLE_VALUE) {
                context.ContextFlags = CONTEXT_ALL;
                GetThreadContext (thread, &context);
                context.Dr0 = 0;
                (*((Dr7Reg*)&context.Dr7)).L0 = 0;
                (*((Dr7Reg*)&context.Dr7)).LE = 0;
                (*((Dr7Reg*)&context.Dr7)).len0 = 0;
                (*((Dr7Reg*)&context.Dr7)).RW0 = 0;
                SetThreadContext (thread, &context);
                CloseHandle (thread);
                }
            }
        RemoveBreakFromList (&debugger->breakpointList, addr);
        }

    return;
}


//--------------------

#define LINE_SIZE   16
//
// ������� ���� ������ ������������� ��������.
//
void DumpDebugProcessMemory (void *addr, DWORD size) {

PBYTE buf = malloc (size);
unsigned int i, j;
DebuggerState *debugger = &glDebugger;

    if (!buf) {
        return;
        }

    ReadProcessMemory (debugger->debugProcess, addr, buf, size, NULL);

    for (i = 0; i < size / LINE_SIZE; ++i) {
        for (j = 0; j < LINE_SIZE; ++j) {
            printf ("%02x ", buf[LINE_SIZE*i+j]);
            }
        printf (" | ");
        for (j = 0; j < LINE_SIZE; ++j) {
            printf ("%c ", buf[LINE_SIZE*i+j]);
            }
        printf ("\n");
        }

    return;
}


//--------------------


//
// ��������������� �������� ���������� ���������� �� ������.
//
Instruction* DisasDebugProcess (void *addr, DWORD instCount) {
    Instruction* result = malloc(sizeof(Instruction));

DWORD size = 15 * instCount;
PBYTE buf = malloc (size);
unsigned int i;
unsigned int offset = 0;
unsigned strLen = 0;
DebuggerState *debugger = &glDebugger;
byte flag = 0;

    if (!buf) {
        return 0;
        }

    ReadProcessMemory (debugger->debugProcess, addr, buf, size, NULL);

    for (i = 0; i < instCount; ++i) {
        char asmBuf[128];
        char hexBuf[128];
        unsigned int len = DisasInstruction (&buf[offset], size-offset, (UINT_PTR)addr + offset, asmBuf, hexBuf);
        if (!len) {
            break;
            }
        strLen = printf ("%p: %-16s %s\n", (UINT_PTR)addr + offset, hexBuf, asmBuf);
        // ������ ���������� ���������� ������.
        if (instCount == 1) {
            result->instruction = 0;
            for (int j = 0; j < instrCount; j++) {
                flag = 0;
                for (int k = 0; k < strlen(myInstructions[j]); k++) {
                    if (strlen(asmBuf) <= k) {
                        flag = 0;
                        break;
                    }
                    if (asmBuf[k] != myInstructions[j][k]) {
                        flag = 0; 
                        break;
                    }
                    else {
                        flag = 1;
                    }
                }
            }
            if (flag) {
                result->instruction = (char*)malloc(sizeof(char) * (strLen + 1));
                sprintf(result->instruction, "%p: %-16s %s\n", (UINT_PTR)addr + offset, hexBuf, asmBuf);
            }
            else {
                result->instruction = 0;
            }
        }
        offset += len;
        }
    result->offset = offset;

    return result;
}


//--------------------


//
// ������� ���������� ���������.
//
char * DumpRegisterContext (CONTEXT *context) {

char *flags1 = "C P A ZSTIDO";
char *flags2 = "c p a zstido";
unsigned int i;
char* result;
int sizeResult = 0;

#ifndef _AMD64_
DWORD   Eax = context->Eax;
DWORD   Ebx = context->Ebx;
DWORD   Ecx = context->Ecx;
DWORD   Edx = context->Edx;
DWORD   Edi = context->Edi;
DWORD   Esi = context->Esi;
DWORD   Ebp = context->Ebp;
DWORD   Esp = context->Esp;
    sizeResult += printf ("eax = %08X  ebx = %08X  ecx = %08X edx = %08X\n", Eax, Ebx, Ecx, Edx);
    sizeResult += printf ("edi = %08X  esi = %08X  ebp = %08X esp = %08X\n", Edi, Esi, Ebp, Esp);
    result = (char*)malloc(sizeof(char) * sizeResult + 1);
    sprintf(result, "rax = %08X  rbx = %08X  rcx = %08X rdx = %08X rdi = %08X  rsi = %08X  rbp = %08X rsp = %08X\n", Eax, Ebx, Ecx, Edx, Edi, Esi, Ebp, Esp);
    printf ("eflags = %08X\t", context->EFlags);
#else   // _AMD64_
DWORD64 Rax = context->Rax;
DWORD64 Rbx = context->Rbx;
DWORD64 Rcx = context->Rcx;
DWORD64 Rdx = context->Rdx;
DWORD64 Rdi = context->Rdi;
DWORD64 Rsi = context->Rsi;
DWORD64 Rbp = context->Rbp;
DWORD64 Rsp = context->Rsp;
    sizeResult += printf ("rax = %016X  rbx = %016X  rcx = %016X rdx = %016X\n", Rax, Rbx, Rcx, Rdx);
    sizeResult += printf ("rdi = %016X  rsi = %016X  rbp = %016X rsp = %016X\n", Rdi, Rsi, Rbp, Rsp);
    result = (char*)malloc(sizeof(char) * sizeResult + 1);
    sprintf(result, "rax = %016X  rbx = %016X  rcx = %016X rdx = %016X rdi = %016X  rsi = %016X  rbp = %016X rsp = %016X\n", Rax, Rbx, Rcx, Rdx, Rdi, Rsi, Rbp, Rsp);
    printf ("eflags = %08X\t", context->EFlags);
#endif  // _AMD64_

    for (i = 0; i < 12; ++i) {
        if (context->EFlags & (1 << i)) {
            printf ("%c", flags1[i]);
            }
        else {
            printf ("%c", flags2[i]);
            }
        }
    printf ("\n\n");

    return result;
}

//--------------------

PVOID glLastInt3Address;
DWORD glLastInt3SaveValue;
//
// ���������� ������� �������� ��������.
//
void EventCreateProcess (DWORD pid, DWORD tid, LPCREATE_PROCESS_DEBUG_INFO procDebugInfo) {

    //DumpDebugProcessMemory (procDebugInfo->lpBaseOfImage, 0x1000);
    DisasDebugProcess (procDebugInfo->lpStartAddress, 5);

    // ����������� ����� �������� �� ��������� ����� ��������
    SetBreakpoint (tid, procDebugInfo->lpStartAddress, BREAK_TYPE_SOFT);
    
    printf ("Create process %d(%d) %p\n", pid, tid, procDebugInfo->lpImageName);
    printf ("Base: %p  start: %p\n", procDebugInfo->lpBaseOfImage, procDebugInfo->lpStartAddress);
    return;
}


//--------------------


//
// ���������� ������� ���������� ��������.
//
void EventExitProcess (DWORD pid, DWORD tid, LPEXIT_PROCESS_DEBUG_INFO procDebugInfo) {

    printf ("Exit process %d(%d) code: %d\n", pid, tid, procDebugInfo->dwExitCode);
    return;
}


//--------------------


//
// ���������� ������� �������� ������.
//
void EventCreateThread (DWORD pid, DWORD tid, LPCREATE_THREAD_DEBUG_INFO threadDebugInfo) {

    printf ("Create thread %d(%d) start: %p\n", pid, tid, threadDebugInfo->lpStartAddress);

    Thread* thread;
    Thread* currentThr;

    DWORD64 thPID;
    DWORD64 thID;
    LPTHREAD_START_ROUTINE thAddr;

    thread = (Thread*)malloc(sizeof(Thread));
    thread->thID = pid;
    thread->thPID = tid;
    thread->thAddr = threadDebugInfo->lpStartAddress;
    thread->nextTh = 0;

    if (threads == NULL) { // ������� ������, ���� �� ���� ��� ���������
        threads = thread;
    }
    else { // ��������� � ����� ������
        for (currentThr = threads; currentThr->nextTh != 0; currentThr = currentThr->nextTh);
        currentThr->nextTh = thread;
    }

    return;
}


//--------------------


//
// ���������� ������� ���������� ������.
//
void EventExitThread (DWORD pid, DWORD tid, LPEXIT_THREAD_DEBUG_INFO threadDebugInfo) {

    printf("Exit thread %d(%d) code: %d\n", pid, tid, threadDebugInfo->dwExitCode);

    Thread* currentTh;
    Thread* selectTh;
    for (currentTh = threads; currentTh != NULL && currentTh->nextTh != 0 && (currentTh->nextTh->thID != tid || currentTh->nextTh->thPID != pid); currentTh = currentTh->nextTh);
    if (currentTh != NULL && currentTh->nextTh != 0 && ((currentTh->nextTh))->thID == tid && currentTh->nextTh->thPID == pid) {
        selectTh = currentTh->nextTh;
        currentTh->nextTh = selectTh->nextTh;
        free(currentTh);
    }
    return;
}


//--------------------


//
// ���������� ������� �������� ���������.
//
void EventLoadDll (DWORD pid, DWORD tid, LPLOAD_DLL_DEBUG_INFO dllDebugInfo) {
    printf ("Load DLL %d(%d) %p %s\n", pid, tid, dllDebugInfo->lpBaseOfDll, GetStringFromHandle(dllDebugInfo->hFile));
    // ��������� ���������� � ��� ������ ���������
    WCHAR * libName = GetStringFromHandle(dllDebugInfo->hFile);
    DWORD64 libPid = pid;
    DWORD libTid = tid;
    LPVOID libAddr = dllDebugInfo->lpBaseOfDll;
    Library* library;
    Library* currentLib;

    library = (Library*)malloc(sizeof(Library));
    library->libName = libName;
    library->libPid = libPid;
    library->libTid = libTid;
    library->libAddr = libAddr;
    library->nextLib = 0;

    if (libraryes == NULL) { // ������� ������, ���� �� ���� ��� ���������
        libraryes = library;
    } 
    else { // ��������� � ����� ������
        for (currentLib = libraryes; currentLib->nextLib != 0; currentLib = currentLib->nextLib);
        currentLib->nextLib = library;
    }

    return;
}

//--------------------


//
// ���������� ������� �������� ���������.
//
void EventUnloadDll (DWORD pid, DWORD tid, LPUNLOAD_DLL_DEBUG_INFO dllDebugInfo) {
    Library* currentLib;
    Library* selectLib;

    printf ("Unload DLL %d(%d)  %p\n", pid, tid, dllDebugInfo->lpBaseOfDll);
    for (currentLib = libraryes; currentLib!= NULL && currentLib->nextLib != 0 && currentLib->nextLib->libAddr != dllDebugInfo->lpBaseOfDll; currentLib = currentLib->nextLib);
    if (currentLib != NULL && currentLib->nextLib->libAddr == dllDebugInfo->lpBaseOfDll) {
        selectLib = currentLib->nextLib;
        currentLib->nextLib = selectLib->nextLib;
        free(selectLib);
    }
    return;
}

//--------------------


//
// ���������� ������� ������ ������.
//
void EventOutputDebugString (DWORD pid, DWORD tid, LPOUTPUT_DEBUG_STRING_INFO debugStringInfo) {

    printf ("Debug string %d(%d) %p\n", pid, tid, debugStringInfo->lpDebugStringData);
    return;
}


//--------------------


//
// ������� ����������� �������� � ������������.
//
void RequestAction (void) {

DebuggerState *debugger = &glDebugger;
char ans[100];
char response[1024];
BOOL isRet = FALSE;
Library* currentLib;
Thread* currentTh;

    // ���� �� ����� ������� ���� �� ������������ ��������
    if (debugger->isRun) {
        return;
        }

    while (!isRet) {
        gets (ans);
        switch (ans[0]) {
            case 'B':
                debugger->isRun = TRUE;
            case 'b':
                debugger->state = DEBUGGER_STATE_BREAK_SOFT;
                isRet = TRUE;
                break;

            case 'H':
                debugger->isRun = TRUE;
            case 'h':
                debugger->state = DEBUGGER_STATE_BREAK_HARD;
                isRet = TRUE;
                break;

            case 'T':
                debugger->isRun = TRUE;
            case 't':
                debugger->state = DEBUGGER_STATE_TRACE;
                debugger->isTrace = TRUE;
                isRet = TRUE;
                break;

            case 'r':
                debugger->isRun = TRUE;
                debugger->state = DEBUGGER_STATE_NOT_TRACE;
                isRet = TRUE;
                break;

            // ��������� ����������� ��������� ������ ��������� � �������
            case 'l':
            case 'L':
                if (ans[1] == 'l' || ans[1] == 'L') {
                    puts("Libs:\n");
                    for (currentLib = libraryes; currentLib != 0; currentLib = currentLib->nextLib) {
                        sprintf(response, "PID: %d;\tTID:%d;\tADDR: %x;\tName: %s\n", currentLib->libPid, currentLib->libTid, currentLib->libAddr, currentLib->libName);
                        puts(response);
                    }
                    
                } 
                else if (ans[1] == 't' || ans[1] == 'T') {
                    // ������ �������
                    puts("Threads:\n");
                    for (currentTh = threads; currentTh != 0; currentTh = currentTh->nextTh) {
                        sprintf(response, "ID: %d;\PID:%d;\tADDR: %x;\n", currentTh->thID, currentTh->thPID, currentTh->thAddr);
                        puts(response);
                    }
                }
                else {
                    puts("Use L<L|T>.\n"
                        "LL - List Library\n"
                        "LT - List Thread\n");
                }
                break;

            case '?':
                puts ("B - permanent soft breakpoint\n"
                      "b - once soft breakpoint\n"
                      "H - permanent hard breakpoint\n"
                      "h - once soft breakpoint\n"
                      "T - permanent trap flag\n"
                      "t - once trap flag\n"
                      "r - run\n"
                      "l<l|t> - list library or thread\n");
                break;

            default:
                break;

            }
        }

    return;
}


//--------------------


//
// ���������� ������� ����� ��������.
//
DWORD BreakpointEvent (DWORD tid, ULONG_PTR exceptionAddr) {

DebuggerState *debugger = &glDebugger;
DWORD continueFlag = DBG_EXCEPTION_NOT_HANDLED;
BreakEntry *breakEntry;
HANDLE thread;
unsigned int instSize;
CONTEXT context;
Instruction* instr;

    breakEntry = FindBreakByAddr (&debugger->breakpointList, exceptionAddr);

    // ���� ������� ������� �� ������ ����������,
    // �� �� ������������ ���
    if (!debugger->isTrace && !breakEntry) {
        return DBG_EXCEPTION_NOT_HANDLED;
        }

    // ���� �� ����� ������ ����������� ����� ��������, ������� �
    if (breakEntry) {
        DeleteBreakpoint (tid, exceptionAddr);
        }

    instr = DisasDebugProcess ((PVOID)exceptionAddr, 1);
    instSize = instr->offset;


    thread = OpenThread (THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, tid);
    if (thread != INVALID_HANDLE_VALUE) {
        context.ContextFlags = CONTEXT_ALL;
        GetThreadContext (thread, &context);
        if (breakEntry) {
#ifdef _AMD64_
            context.Rip = exceptionAddr;
#else   // _AMD64_
            context.Eip = exceptionAddr;
#endif  // _AMD64_
            if (breakEntry->type == BREAK_TYPE_HARD) {
                context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                context.Dr6 = 0;
                SetThreadContext (thread, &context);
                }
            }
        }
	// DumpRegisterContext (&context);

    if (instr->instruction != 0) {
        writeLog(logger, instr->instruction);
        writeLog(logger, DumpRegisterContext(&context));
    }
    

    // ����������� ���������� ��������
    RequestAction();

    switch (debugger->state) {
        case DEBUGGER_STATE_BREAK_HARD:
            SetBreakpoint (tid, exceptionAddr + instSize, BREAK_TYPE_HARD);
            break;
        case DEBUGGER_STATE_BREAK_SOFT:
            SetBreakpoint (tid, exceptionAddr + instSize, BREAK_TYPE_SOFT);
            break;
        case DEBUGGER_STATE_TRACE:
            context.EFlags |= 0x100;
            break;
        }

    if (thread != INVALID_HANDLE_VALUE) {
        context.ContextFlags = CONTEXT_CONTROL;
        SetThreadContext (thread, &context);
        CloseHandle (thread);
        }
   
    return DBG_CONTINUE;
}


//--------------------


//
// ���������� ������� ����������.
//
DWORD EventException (DWORD pid, DWORD tid, LPEXCEPTION_DEBUG_INFO exceptionDebugInfo) {

DWORD continueFlag = (DWORD)DBG_EXCEPTION_NOT_HANDLED;

    switch (exceptionDebugInfo->ExceptionRecord.ExceptionCode) {

        // ����������� ����� ��������
        case EXCEPTION_BREAKPOINT:
            continueFlag = BreakpointEvent (tid, (ULONG_PTR)exceptionDebugInfo->ExceptionRecord.ExceptionAddress);
            break;

        // ���� ����������� ��� ���������� ����� ��������
        case EXCEPTION_SINGLE_STEP:
            continueFlag = BreakpointEvent (tid, (ULONG_PTR)exceptionDebugInfo->ExceptionRecord.ExceptionAddress);
            break;

        case EXCEPTION_PRIV_INSTRUCTION:
            /*
            DisasDebugProcess (exceptionDebugInfo->ExceptionRecord.ExceptionAddress, 1);
            thread = OpenThread (THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, tid);
            if (thread != INVALID_HANDLE_VALUE) {
                context.ContextFlags = CONTEXT_ALL;
                GetThreadContext (thread, &context);
                //context.Eip = (DWORD)exceptionDebugInfo->ExceptionRecord.ExceptionAddress;
                context.EFlags |= 0x100;
                DumpRegisterContext (&context);
                SetThreadContext (thread, &context);
                CloseHandle (thread);
                }
            SetBreakpoint (tid, 0x00401000, BREAK_TYPE_SOFT);
            {
            char buf[512];
            //DumpDebugProcessMemory (context.Esp, 512);
            //ReadProcessMemory (glDebugProcess, context.Esp, buf, 512, NULL);

            }
            //*/
            break;
        
        }

    printf ("Exception %d(%d) %p: %x\n", pid, tid, exceptionDebugInfo->ExceptionRecord.ExceptionAddress, exceptionDebugInfo->ExceptionRecord.ExceptionCode);

    return continueFlag;
}


//--------------------


//
// ������� ��������������� ������� �������.
//
void DebugRun (void) {
  
BOOL completed = FALSE;
BOOL attached = FALSE;
DebuggerState *debugger = &glDebugger;

    while (!completed) {
        DEBUG_EVENT debugEvent;
        DWORD continueFlag = DBG_CONTINUE;

        // ������� ����������� ����������� ���������
        if (!WaitForDebugEvent (&debugEvent, INFINITE)) {
            break;
            }
        
        switch (debugEvent.dwDebugEventCode) {
            case CREATE_PROCESS_DEBUG_EVENT:
                EventCreateProcess (debugEvent.dwProcessId, debugEvent.dwThreadId, &debugEvent.u.CreateProcessInfo);
                break;

            case EXIT_PROCESS_DEBUG_EVENT:
                EventExitProcess (debugEvent.dwProcessId, debugEvent.dwThreadId, &debugEvent.u.ExitProcess);
                completed = TRUE;
                break;

            case CREATE_THREAD_DEBUG_EVENT:
                EventCreateThread (debugEvent.dwProcessId, debugEvent.dwThreadId, &debugEvent.u.CreateThread);
                break;

            case EXIT_THREAD_DEBUG_EVENT:
                EventExitThread (debugEvent.dwProcessId, debugEvent.dwThreadId, &debugEvent.u.ExitThread);
                break;

            case LOAD_DLL_DEBUG_EVENT:
                EventLoadDll (debugEvent.dwProcessId, debugEvent.dwThreadId, &debugEvent.u.LoadDll);
                break;

            case UNLOAD_DLL_DEBUG_EVENT:
                EventUnloadDll (debugEvent.dwProcessId, debugEvent.dwThreadId, &debugEvent.u.UnloadDll);
                break;

            case OUTPUT_DEBUG_STRING_EVENT:
                EventOutputDebugString (debugEvent.dwProcessId, debugEvent.dwThreadId, &debugEvent.u.DebugString);
                break;

            case EXCEPTION_DEBUG_EVENT:
                if (!attached) {
                    // ������ ���������� ��� ������ �������
                    attached = TRUE;
                    }
                else {
                    continueFlag = EventException (debugEvent.dwProcessId, debugEvent.dwThreadId, &debugEvent.u.Exception);
                    }
                break;

            default:
                printf ("Unexpected debug event: %d\n", debugEvent.dwDebugEventCode);
            }

        if (!ContinueDebugEvent (debugEvent.dwProcessId, debugEvent.dwThreadId, continueFlag)) {
            printf ("Error continuing debug event\n");
            }
        }

    CloseHandle (debugger->debugProcess);

    return;
}


//--------------------

//--------------------
