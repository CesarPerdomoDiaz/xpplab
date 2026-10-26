/* XPP Lab's own Python for the console, downloaded the first time it is
   wanted (pyfetch.c), and pip for whichever Python the console uses. */
#ifndef XPPS_PYFETCH_H
#define XPPS_PYFETCH_H

#include <stdbool.h>
#include <stddef.h>

#include "app.h"

/* python fetch: download, check and install the Python with NumPy and
   Matplotlib, in steps run from pyfetch_poll() so the app keeps drawing. */
bool pyfetch_start(App *a);
/* python install PACKAGES: pip install them into this Python. */
bool pyfetch_pip(App *a, const char *python, const char *packages);
void pyfetch_poll(App *a); /* once a frame */
void pyfetch_cancel(App *a);
bool pyfetch_running(void);
/* What is happening and how far along (0 to 1, or -1 when unknown). */
const char *pyfetch_status(float *fraction);
/* The installed interpreter, if XPP Lab's Python has been fetched. */
bool pyfetch_python(char *out, size_t len);
/* Where it is (or would be) installed, and what would be downloaded. */
void pyfetch_folder(char *out, size_t len);
const char *pyfetch_about(void); /* "Python 3.13.16 with NumPy and Matplotlib" */
int pyfetch_megabytes(void);     /* the download, about; 0 if there is none */

#endif
