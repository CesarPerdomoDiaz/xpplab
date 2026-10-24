/* The model browser: example models and recent files. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "ui.h"

typedef struct {
  char name[128];
  char about[400]; /* the leading comment of the file */
} Example;

static Example *examples;
static int nexamples = -1;
static char filter[64];

static int by_name(const void *x, const void *y) {
  return SDL_strcasecmp(((const Example *)x)->name, ((const Example *)y)->name);
}

/* The first comment lines of a model usually say what it is. */
static void read_about(const char *path, char *out, size_t len) {
  FILE *f = fopen(path, "r");
  char line[256];
  size_t k = 0;
  int lines = 0;
  out[0] = '\0';
  if (!f) return;
  while (fgets(line, sizeof line, f) && lines < 12) {
    char *s = line;
    if (s[0] != '#') {
      if (k > 0) break;
      continue;
    }
    s++;
    while (*s == ' ' || *s == '#') s++;
    if (*s == '\n' || *s == '\r' || *s == '\0') continue;
    {
      size_t n = strcspn(s, "\r\n");
      if (k + n + 2 >= len) break;
      memcpy(out + k, s, n);
      k += n;
      out[k++] = ' ';
      out[k] = '\0';
    }
    lines++;
  }
  fclose(f);
}

static int capacity;

/* On Windows the folder is listed with FindFirstFileExW: SDL's directory
   functions (SDL 3.4) failed there now and then with "cannot find the file
   specified" while the folder was in plain sight, leaving the list empty. */
static SDL_EnumerationResult SDLCALL add_example(void *ud, const char *dir,
                                                 const char *name);

#ifdef _WIN32
static bool list_folder(const char *dir) {
  wchar_t pattern[1100];
  char sep_dir[1100], name[400];
  WIN32_FIND_DATAW e;
  HANDLE h;
  snprintf(sep_dir, sizeof sep_dir, "%s\\", dir);
  if (!MultiByteToWideChar(CP_UTF8, 0, sep_dir, -1, pattern, 1090)) return false;
  wcscat(pattern, L"*");
  h = FindFirstFileExW(pattern, FindExInfoBasic, &e, FindExSearchNameMatch, NULL, 0);
  if (h == INVALID_HANDLE_VALUE) return false;
  do {
    if (e.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
    if (WideCharToMultiByte(CP_UTF8, 0, e.cFileName, -1, name, sizeof name, NULL, NULL) &&
        add_example(NULL, sep_dir, name) == SDL_ENUM_FAILURE)
      break;
  } while (FindNextFileW(h, &e));
  FindClose(h);
  return true;
}
#else
static bool list_folder(const char *dir) {
  return SDL_EnumerateDirectory(dir, add_example, NULL);
}
#endif

static SDL_EnumerationResult SDLCALL add_example(void *ud, const char *dir,
                                                 const char *name) {
  size_t len = strlen(name);
  char path[1300];
  (void)ud;
  if (len < 5 || SDL_strcasecmp(name + len - 4, ".ode") != 0 ||
      len >= sizeof examples->name)
    return SDL_ENUM_CONTINUE;
  if (nexamples == capacity) {
    Example *more = realloc(examples, (size_t)(capacity ? 2 * capacity : 256) *
                                          sizeof *examples);
    if (!more) return SDL_ENUM_FAILURE;
    examples = more;
    capacity = capacity ? 2 * capacity : 256;
  }
  snprintf(examples[nexamples].name, sizeof examples->name, "%s", name);
  snprintf(path, sizeof path, "%s%s", dir, name);
  read_about(path, examples[nexamples].about, sizeof examples->about);
  nexamples++;
  return SDL_ENUM_CONTINUE;
}

static void scan(App *a) {
  nexamples = 0;
  if (!list_folder(a->examples_dir)) nexamples = 0;
  if (nexamples > 0) qsort(examples, (size_t)nexamples, sizeof *examples, by_name);
}

void ui_browser(App *a) {
  int i;
  if (!igBegin(WIN_BROWSER, &a->show_browser, 0)) {
    igEnd();
    return;
  }
  /* An empty list is looked at again every few seconds: the folder may
     have been in the middle of being rewritten (by a build, say). */
  if (nexamples < 0 || (nexamples == 0 && igGetFrameCount() % 180 == 0)) scan(a);
  igSetNextItemWidth(-1);
  igInputTextWithHint("##find", "Search examples...", filter, sizeof filter, 0,
                      NULL, NULL);
  if (a->nrecent > 0 &&
      igCollapsingHeader_TreeNodeFlags("Recent", ImGuiTreeNodeFlags_DefaultOpen)) {
    for (i = 0; i < a->nrecent && i < 6; i++) {
      const char *name = strrchr(a->recent[i], '/');
      const char *name2 = strrchr(a->recent[i], '\\');
      if (name2 > name) name = name2;
      igPushID_Int(1000 + i);
      if (igSelectable_Bool(name ? name + 1 : a->recent[i],
                            strcmp(a->recent[i], a->model_path) == 0, 0, V2(0, 0))) {
        char p[1024];
        snprintf(p, sizeof p, "%s", a->recent[i]);
        app_open(a, p);
      }
      Tooltip(a->recent[i]);
      igPopID();
    }
  }
  if (igCollapsingHeader_TreeNodeFlags("Examples", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (nexamples == 0)
      igTextDisabled("No examples found in %s", a->examples_dir);
    for (i = 0; i < nexamples; i++) {
      Example *e = &examples[i];
      if (filter[0] && !SDL_strcasestr(e->name, filter) &&
          !SDL_strcasestr(e->about, filter))
        continue;
      igPushID_Int(i);
      if (igSelectable_Bool(e->name, strcmp(e->name, a->model_name) == 0, 0,
                            V2(0, 0))) {
        char path[1300];
        snprintf(path, sizeof path, "%s/%s", a->examples_dir, e->name);
        app_open(a, path);
      }
      if (e->about[0]) Tooltip(e->about);
      igPopID();
    }
  }
  igEnd();
}
