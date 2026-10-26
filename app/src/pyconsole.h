/* The command window's Python (pyconsole.c; the other side is
   xpplab/console.py). */
#ifndef XPPS_PYCONSOLE_H
#define XPPS_PYCONSOLE_H

#include <stdbool.h>
#include <stddef.h>

#include "app.h"

/* A variable of the console's namespace, for the Workspace. */
typedef struct {
  char name[64];
  char type[48];
  char summary[160];
} PyVar;

/* Run a line of Python, starting the process the first time; lines typed
   while the last one runs wait their turn. */
bool pyconsole_send(App *a, const char *line);
/* Once a frame: the process's output to the command window. */
void pyconsole_poll(App *a);
bool pyconsole_start(App *a); /* the process, without a line to run */
void pyconsole_stop(App *a);
bool pyconsole_running(void);
bool pyconsole_busy(void);
bool pyconsole_waiting(void);
const char *pyconsole_prompt(void); /* "py>", or "..." inside a statement */
const char *pyconsole_python(void); /* the interpreter in use */
/* Completion: ask for the word, then take the answer when it comes. */
void pyconsole_complete(App *a, const char *word);
bool pyconsole_completion(char *out, size_t len);
/* The namespace's variables, asked for and then read. */
void pyconsole_request_vars(App *a);
const PyVar *pyconsole_vars(int *n);

#endif
