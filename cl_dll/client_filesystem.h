#ifndef CLIENT_FILESYSTEM_H
#define CLIENT_FILESYSTEM_H

#include "FileSystem.h"

bool InitializeFileSystem(void);
void CloseFileSystem(void);
FileHandle_t OpenGameFile(const char *fileName, const char *options);

#endif
