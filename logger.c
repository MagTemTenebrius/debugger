#include "logger.h"
#include <stdio.h>
#include <malloc.h>

void writeLog(Logger* log, char* msg) {
	FILE* fPtr;
	fPtr = fopen(log->filename, "a");
	fputs(msg, fPtr);
	fclose(fPtr);
}


Logger* initLog(char* filename) {
	Logger* logger = NULL;
	logger = (Logger*)malloc(sizeof(Logger));
	char* filename_;
	FILE* fp;
	filename_ = (char*)malloc(sizeof(char) * (strlen(filename) + 1));
	logger = malloc(sizeof(Logger));
	logger->filename = filename_;
	strcpy(logger->filename, filename);
	//printf("fopen2\n");
	fp = fopen(filename, "w");
	//printf("fopen3\n");
	fclose(fp);
	return logger;
}