#include "extdll.h"
#include "server_resource.h"
#include "server_support.h"
#include "FileSystem.h"
#include <string.h>

bool UTIL_ExtractDirectoryFromFilename(const char *filename, char *directory, unsigned int size, bool stripLeadingSlashes)
{
	if (!filename || !directory || !size)
		return false;

	char path[MAX_PATH + 1];

	UTIL_CopyString(path, filename, sizeof(path));
	UTIL_FixSlashes(path);

	char *slash = strrchr(path, '/');

	if (slash)
	{
		const char *start = path;

		if (stripLeadingSlashes)
		{
			while (*start == '/')
				start++;
		}

		unsigned int length = slash - start;

		if (length >= size)
			return false;

		UTIL_CopyString(directory, start, length + 1);
	}
	else if (!strcmp(filename, ".") || !strcmp(filename, ".."))
		UTIL_CopyString(directory, filename, size);
	else
		memset(directory, 0, size);

	return true;
}

char *UTIL_FindAbstractResourcePath(const char *filename, char *path, unsigned int size)
{
	if (filename && path && size
		&& ((IFileSystem_SvEngine *)g_pFileSystem)->FullPathToRelativePathEx(filename, NULL, path, size - 1))
		return path;

	return NULL;
}
