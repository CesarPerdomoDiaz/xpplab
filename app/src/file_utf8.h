/* Files by their UTF-8 names, also on Windows, where fopen() reads the
   ANSI code page. Use this for every name that comes from the user. */
#ifndef FILE_UTF8_H
#define FILE_UTF8_H

#include <stdio.h>

FILE *app_fopen_utf8(const char *path, const char *mode);

#endif
