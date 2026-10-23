/* The manual pages embedded in the executable (docs/manual/*.md). */
#ifndef XPPS_DOCS_H
#define XPPS_DOCS_H

typedef struct {
  const char *name; /* file name without .md */
  const char *text; /* Markdown */
} doc_page;

extern const doc_page doc_pages[]; /* ends with {0, 0} */

const doc_page *docs_find(const char *name);
/* The page title (its first "# " heading) copied into buf. */
const char *docs_title(const doc_page *p, char *buf, int len);
int docs_count(void);

#endif
