/*
    ������ �������� ���������, ���������������
    ����������� � ���������� ����� ��������
    � �����������.

    ������ ���� �������������   01.10.2014
*/


#include <string.h>

#include "disas.h"
#include "debugger.h"

//***************************************************************


//***************************************************************

int main (unsigned int argc, char *argv[], char *evnp[]) {

    if (argc > 1) {
        if (CreateDebugProcess (argv[1])) {
            DebugRun();
            }
        }

    return 0;
}


//--------------------

//--------------------

//--------------------
