/* The app's actions in one table. The menus (ui_menu.c) are drawn from it
   and the command palette (ui_palette.c) searches it together with the
   command window's commands, so an action cannot be in one and not the
   other. No window code here: xpplab-batch uses it too, for `palette`. */
#ifndef XPPS_ACTIONS_H
#define XPPS_ACTIONS_H

#include "app.h"

/* The shortcut modifier as shown in menus. ImGui maps its Ctrl shortcuts
   to the Command key on macOS. */
#ifdef __APPLE__
#define MOD "Cmd+"
#else
#define MOD "Ctrl+"
#endif

typedef struct Action {
  const char *id;          /* stable and unique: "run.nullclines" */
  const char *menu;        /* "File", "Run", "View", "Help" */
  const char *submenu;     /* inside the menu, or NULL */
  bool sep_before;         /* a separator above it in the menu */
  const char *name;        /* the menu item's label */
  const char *shortcut;    /* as shown, or NULL */
  const char *description; /* one line, for the palette */
  const char *tip;         /* a longer hover text in the menu, or NULL */
  void (*run)(App *a, int arg);
  int arg;
  long flag;               /* offset of a bool in App that the item toggles, or -1 */
  bool (*enabled)(App *a, const char **reason); /* NULL: always */
  bool (*checked)(App *a, int arg);             /* NULL: a toggle's flag, or not */
} Action;

extern const Action app_actions[];
extern const int app_nactions;

/* A command of the command window. args is NULL when it runs as it is
   (it may still take options); else the palette puts "name " in the command
   line for the user to complete. */
typedef struct {
  const char *name;
  const char *args;
  const char *description;
  const char *shortcut;    /* XPPAUT's keys, or NULL */
  int needs;               /* NEED_* */
} CommandInfo;

enum { NEED_NONE, NEED_MODEL, NEED_RUN, NEED_BRANCH };

extern const CommandInfo app_command_table[];
extern const int app_ncommands;
const char *app_command_usage(void); /* the text of `help` */

/* Whether the action's item is shown ticked. */
bool action_checked(App *a, const Action *x);
/* Whether it can run now; if not, why (never NULL). */
bool action_available(App *a, const Action *x, const char **reason);
/* Do what the menu item does. */
void action_run(App *a, const Action *x);

/* ---- The palette ------------------------------------------------------
   Entries are numbered: the actions, then the commands, then the recent
   files of File > Open recent. */
#define PALETTE_MAX_HITS 512

int palette_entries(App *a);
void palette_label(App *a, int entry, char *buf, size_t n);
const char *palette_description(App *a, int entry);
const char *palette_shortcut(App *a, int entry); /* "" if none */
bool palette_available(App *a, int entry, const char **reason);
/* The entries matching the query, best first; the number found. With an
   empty query: the recent ones first, then all. */
int palette_search(App *a, const char *query, int *hits, int max);
/* Run an entry as its menu item or command does. A command that takes
   arguments is only put in the command line. */
void palette_run(App *a, int entry);
void palette_open(App *a);
/* `palette ...` of the command window (see the manual). */
void palette_command(App *a, char *rest);

/* In ui_console.c / ui_menu.c / ui_common.c: what actions.c needs from the
   window, with stand-ins in xpplab-batch. */
void ui_console_prefill(App *a, const char *text);
void ui_show_help(App *a, const char *page);
void ui_apply_theme(App *a);
void ui_dialog_open(App *a);
void ui_dialog_save_as(App *a);
void ui_dialog_export_csv(App *a);
void ui_dialog_save_script(App *a);

#endif
