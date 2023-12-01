#pragma once

typedef struct _Logger {
	char* filename;
} Logger;

Logger* initLog(char *filename);

void writeLog(Logger* log, char* msg);