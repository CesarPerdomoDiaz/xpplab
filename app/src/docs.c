#include "docs.h"

#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

const doc_page *docs_find(const char *name) {
  const doc_page *p;
  for (p = doc_pages; p->name; p++)
    if (SDL_strcasecmp(p->name, name) == 0) return p;
  return NULL;
}

const char *docs_title(const doc_page *p, char *buf, int len) {
  const char *t = strstr(p->text, "# ");
  const char *e;
  if (t != p->text && t && t[-1] != '\n') t = NULL;
  if (!t) {
    snprintf(buf, (size_t)len, "%s", p->name);
    return buf;
  }
  t += 2;
  e = strchr(t, '\n');
  snprintf(buf, (size_t)len, "%.*s", e ? (int)(e - t) : (int)strlen(t), t);
  return buf;
}

int docs_count(void) {
  int n = 0;
  while (doc_pages[n].name) n++;
  return n;
}

void docs_names(char *buf, size_t len) {
  const doc_page *p;
  size_t n = 0;
  if (len) buf[0] = 0;
  for (p = doc_pages; p->name && n < len; p++)
    n += (size_t)snprintf(buf + n, len - n, "%s%s", n ? ", " : "", p->name);
}
