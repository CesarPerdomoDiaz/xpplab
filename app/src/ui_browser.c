/* The model browser: example models and recent files. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

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

static void scan(App *a) {
  char **files;
  int n = 0, i;
  free(examples);
  examples = NULL;
  nexamples = 0;
  files = SDL_GlobDirectory(a->examples_dir, "*.ode", 0, &n);
  if (!files) return;
  examples = calloc((size_t)n + 1, sizeof *examples);
  for (i = 0; i < n; i++) {
    char path[1300];
    snprintf(examples[i].name, sizeof examples[i].name, "%s", files[i]);
    snprintf(path, sizeof path, "%s/%s", a->examples_dir, files[i]);
    read_about(path, examples[i].about, sizeof examples[i].about);
  }
  nexamples = n;
  SDL_free(files);
  qsort(examples, (size_t)nexamples, sizeof *examples, by_name);
}

void ui_browser(App *a) {
  int i;
  if (!igBegin(WIN_BROWSER, &a->show_browser, 0)) {
    igEnd();
    return;
  }
  if (nexamples < 0) scan(a);
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
