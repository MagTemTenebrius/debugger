/*
    Файл disas.h.

    Заголовочный файл модуля disas.c.

    Маткин Илья Александрович   01.10.2014
*/

#ifndef _DISAS_H_
#define _DISAS_H_

#include <windows.h>
#include <udis86.h>

//***************************************************************

void PrintDisasCode (unsigned char *code, unsigned int codeSize);

unsigned int DisasInstruction (unsigned char *instCode, unsigned int instCodeSize, uint64_t pc, char *asmString, char *hexString);

//***************************************************************

#endif  // _DISAS_H_
