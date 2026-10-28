/* Text for figures: matplotlib's fonts and its mathtext, laid out here and
   drawn as the letters' outlines (fig.h).

   A label is plain text with mathematics between dollar signs, as in
   matplotlib: "$V_W$ [m/s]", "$\tilde{V}_{W,H}$", "$T/(mg)$". In
   mathematics letters are italic, digits and capital Greek upright;
   ^ and _ raise and lower, {} group, and the usual TeX commands give Greek
   letters, symbols, \frac, \sqrt, accents (\tilde, \hat, \bar, \dot,
   \ddot, \vec), spaces (\, \; \quad \!) and upright text (\mathrm,
   \text). */
#include "fig.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
/* Vertices in floats: the rasteriser also fills the figure's lines, whose
   points are fractions of a point. */
typedef struct {
  float x, y, cx, cy, cx1, cy1;
  unsigned char type, padding;
} stbtt_vertex;
#define stbtt_vertex stbtt_vertex
#define stbtt_vertex_type float
#include "imstb_truetype.h"

extern const unsigned char font_dejavu[], font_dejavu_it[], font_stix[],
    font_stix_it[];

/* Four faces: [family * 2 + italic]. */
static stbtt_fontinfo faces[4];
static int faces_state; /* 0 not tried, 1 ready, -1 failed */

static bool load_faces(void) {
  const unsigned char *data[4] = {font_dejavu, font_dejavu_it, font_stix,
                                  font_stix_it};
  int i;
  if (faces_state) return faces_state > 0;
  faces_state = 1;
  for (i = 0; i < 4; i++)
    if (!stbtt_InitFont(&faces[i], data[i], stbtt_GetFontOffsetForIndex(data[i], 0)))
      faces_state = -1;
  return faces_state > 0;
}

bool fig_fonts_ok(void) { return load_faces(); }

/* ---- Laid-out items --------------------------------------------------------- */

typedef struct {
  int rule;          /* 0: a glyph, 1: a filled rectangle */
  int face, glyph;
  float size;        /* glyph: font size in points */
  float x, y;        /* glyph: origin on the baseline; rule: lower left */
  float w, h;        /* rule */
} Item;

typedef struct {
  Item *it;
  int n, cap;
} List;

typedef struct {
  float w, asc, desc; /* advance, extent above and below the baseline */
} Box;

static void push(List *l, Item it) {
  if (l->n == l->cap) {
    int cap = l->cap ? 2 * l->cap : 32;
    Item *t = realloc(l->it, sizeof(Item) * (size_t)cap);
    if (!t) return;
    l->it = t;
    l->cap = cap;
  }
  l->it[l->n++] = it;
}

/* Move every item from src to dst, shifted. */
static void append(List *dst, List *src, float dx, float dy) {
  int i;
  for (i = 0; i < src->n; i++) {
    Item it = src->it[i];
    it.x += dx;
    it.y += dy;
    push(dst, it);
  }
  free(src->it);
  src->it = NULL;
  src->n = src->cap = 0;
}

static float scale_of(int face, float size) {
  return stbtt_ScaleForMappingEmToPixels(&faces[face], size);
}

/* The face that has the code point: the one asked for, then the family's
   upright face, then the other family. */
static int find_glyph(int face, int cp, int *glyph) {
  int order[4], i, fam = face / 2;
  order[0] = face;
  order[1] = fam * 2;
  order[2] = (1 - fam) * 2 + (face & 1);
  order[3] = (1 - fam) * 2;
  for (i = 0; i < 4; i++) {
    int g = stbtt_FindGlyphIndex(&faces[order[i]], cp);
    if (g) {
      *glyph = g;
      return order[i];
    }
  }
  *glyph = stbtt_FindGlyphIndex(&faces[face], '?');
  return face;
}

/* Add a character with its origin at (x, 0); returns its box. */
static Box glyph(List *l, int face, int cp, float size, float x) {
  Box b = {0, 0, 0};
  int g, adv, lsb, x0, y0, x1, y1;
  float sc;
  Item it;
  face = find_glyph(face, cp, &g);
  sc = scale_of(face, size);
  stbtt_GetGlyphHMetrics(&faces[face], g, &adv, &lsb);
  b.w = adv * sc;
  if (stbtt_GetGlyphBox(&faces[face], g, &x0, &y0, &x1, &y1)) {
    b.asc = y1 * sc;
    b.desc = -y0 * sc;
  }
  memset(&it, 0, sizeof it);
  it.face = face;
  it.glyph = g;
  it.size = size;
  it.x = x;
  if (cp != ' ') push(l, it);
  return b;
}

static Box rule(List *l, float x, float y, float w, float h) {
  Box b = {w, y + h, -y};
  Item it;
  memset(&it, 0, sizeof it);
  it.rule = 1;
  it.x = x;
  it.y = y;
  it.w = w;
  it.h = h;
  push(l, it);
  return b;
}

static void grow(Box *b, Box c, float dx, float dy) {
  if (c.asc + dy > b->asc) b->asc = c.asc + dy;
  if (c.desc - dy > b->desc) b->desc = c.desc - dy;
  if (dx + c.w > b->w) b->w = dx + c.w;
}

/* ---- UTF-8 ---------------------------------------------------------------- */

static int utf8(const char **ps) {
  const unsigned char *s = (const unsigned char *)*ps;
  int cp, n, i;
  if (s[0] < 0x80) {
    *ps += 1;
    return s[0];
  }
  if ((s[0] & 0xE0) == 0xC0) cp = s[0] & 0x1F, n = 1;
  else if ((s[0] & 0xF0) == 0xE0) cp = s[0] & 0x0F, n = 2;
  else if ((s[0] & 0xF8) == 0xF0) cp = s[0] & 0x07, n = 3;
  else {
    *ps += 1;
    return 0xFFFD;
  }
  for (i = 1; i <= n; i++) {
    if ((s[i] & 0xC0) != 0x80) {
      *ps += i;
      return 0xFFFD;
    }
    cp = (cp << 6) | (s[i] & 0x3F);
  }
  *ps += n + 1;
  return cp;
}

/* ---- Symbols ---------------------------------------------------------------- */

enum { ORD, BIN, REL, OPEN, CLOSE, PUNCT, OP };

typedef struct {
  const char *name;
  int cp, cls, italic; /* italic: in the italic face by default */
} Sym;

static const Sym syms[] = {
    {"alpha", 0x3B1, ORD, 1}, {"beta", 0x3B2, ORD, 1}, {"gamma", 0x3B3, ORD, 1},
    {"delta", 0x3B4, ORD, 1}, {"epsilon", 0x3F5, ORD, 1},
    {"varepsilon", 0x3B5, ORD, 1}, {"zeta", 0x3B6, ORD, 1}, {"eta", 0x3B7, ORD, 1},
    {"theta", 0x3B8, ORD, 1}, {"vartheta", 0x3D1, ORD, 1}, {"iota", 0x3B9, ORD, 1},
    {"kappa", 0x3BA, ORD, 1}, {"lambda", 0x3BB, ORD, 1}, {"mu", 0x3BC, ORD, 1},
    {"nu", 0x3BD, ORD, 1}, {"xi", 0x3BE, ORD, 1}, {"pi", 0x3C0, ORD, 1},
    {"varpi", 0x3D6, ORD, 1}, {"rho", 0x3C1, ORD, 1}, {"varrho", 0x3F1, ORD, 1},
    {"sigma", 0x3C3, ORD, 1}, {"varsigma", 0x3C2, ORD, 1}, {"tau", 0x3C4, ORD, 1},
    {"upsilon", 0x3C5, ORD, 1}, {"phi", 0x3D5, ORD, 1}, {"varphi", 0x3C6, ORD, 1},
    {"chi", 0x3C7, ORD, 1}, {"psi", 0x3C8, ORD, 1}, {"omega", 0x3C9, ORD, 1},
    {"Gamma", 0x393, ORD, 0}, {"Delta", 0x394, ORD, 0}, {"Theta", 0x398, ORD, 0},
    {"Lambda", 0x39B, ORD, 0}, {"Xi", 0x39E, ORD, 0}, {"Pi", 0x3A0, ORD, 0},
    {"Sigma", 0x3A3, ORD, 0}, {"Upsilon", 0x3A5, ORD, 0}, {"Phi", 0x3A6, ORD, 0},
    {"Psi", 0x3A8, ORD, 0}, {"Omega", 0x3A9, ORD, 0},
    {"infty", 0x221E, ORD, 0}, {"partial", 0x2202, ORD, 0},
    {"nabla", 0x2207, ORD, 0}, {"hbar", 0x210F, ORD, 0}, {"ell", 0x2113, ORD, 0},
    {"prime", 0x2032, ORD, 0}, {"degree", 0xB0, ORD, 0}, {"circ", 0x2218, BIN, 0},
    {"cdot", 0x22C5, BIN, 0}, {"times", 0xD7, BIN, 0}, {"div", 0xF7, BIN, 0},
    {"pm", 0xB1, BIN, 0}, {"mp", 0x2213, BIN, 0}, {"ast", 0x2217, BIN, 0},
    {"leq", 0x2264, REL, 0}, {"le", 0x2264, REL, 0}, {"geq", 0x2265, REL, 0},
    {"ge", 0x2265, REL, 0}, {"neq", 0x2260, REL, 0}, {"ne", 0x2260, REL, 0},
    {"approx", 0x2248, REL, 0}, {"sim", 0x223C, REL, 0}, {"simeq", 0x2243, REL, 0},
    {"equiv", 0x2261, REL, 0}, {"propto", 0x221D, REL, 0}, {"ll", 0x226A, REL, 0},
    {"gg", 0x226B, REL, 0}, {"to", 0x2192, REL, 0}, {"rightarrow", 0x2192, REL, 0},
    {"leftarrow", 0x2190, REL, 0}, {"Rightarrow", 0x21D2, REL, 0},
    {"mapsto", 0x21A6, REL, 0}, {"in", 0x2208, REL, 0},
    {"ldots", 0x2026, ORD, 0}, {"cdots", 0x22EF, ORD, 0},
    {"sum", 0x2211, OP, 0}, {"prod", 0x220F, OP, 0}, {"int", 0x222B, OP, 0},
    {"langle", 0x27E8, OPEN, 0}, {"rangle", 0x27E9, CLOSE, 0},
    {"lbrace", '{', OPEN, 0}, {"rbrace", '}', CLOSE, 0},
    {"{", '{', OPEN, 0}, {"}", '}', CLOSE, 0}, {"$", '$', ORD, 0},
    {"%", '%', ORD, 0}, {"_", '_', ORD, 0}, {"#", '#', ORD, 0},
    {"&", '&', ORD, 0}, {"|", 0x2016, ORD, 0}, {"backslash", '\\', ORD, 0},
    {NULL, 0, 0, 0}};

/* Upright function names: \sin x. */
static const char *funcs[] = {"sin", "cos", "tan", "exp", "log", "ln", "max",
                              "min", "sup", "inf", "lim", "det", "arg", "sinh",
                              "cosh", "tanh", "arctan", "arcsin", "arccos", "Re",
                              "Im", NULL};

/* ---- The parser -------------------------------------------------------------- */

typedef struct {
  const char *s;
  int family;
  bool error;
  char msg[160];
} Parser;

typedef struct {
  float size;
  bool math, upright, textmode; /* textmode: \text{} keeps spaces */
} Style;

static void fail(Parser *ps, const char *fmt, const char *arg) {
  if (!ps->error) {
    ps->error = true;
    snprintf(ps->msg, sizeof ps->msg, fmt, arg);
  }
}

static Box parse_list(Parser *ps, List *out, Style st, char end, int *last_cls);

/* A {group} or a single atom, at x = 0. */
static Box parse_arg(Parser *ps, List *out, Style st) {
  Box b = {0, 0, 0};
  int cls = -1;
  while (*ps->s == ' ') ps->s++;
  if (*ps->s == '{') {
    ps->s++;
    b = parse_list(ps, out, st, '}', &cls);
    if (*ps->s == '}') ps->s++;
    else fail(ps, "a { is not closed%s", "");
    return b;
  }
  {
    /* one character or command: parse a list that stops after one atom */
    const char *start = ps->s;
    char one[64];
    size_t n;
    Parser sub = *ps;
    if (*start == '\\') {
      const char *e = start + 1;
      if (*e && !((*e >= 'a' && *e <= 'z') || (*e >= 'A' && *e <= 'Z'))) e++;
      else
        while ((*e >= 'a' && *e <= 'z') || (*e >= 'A' && *e <= 'Z')) e++;
      n = (size_t)(e - start);
    } else {
      const char *e = start;
      if (*e) utf8(&e);
      n = (size_t)(e - start);
    }
    if (n >= sizeof one) n = sizeof one - 1;
    memcpy(one, start, n);
    one[n] = '\0';
    sub.s = one;
    b = parse_list(&sub, out, st, '\0', &cls);
    if (sub.error) fail(ps, "%s", sub.msg);
    ps->s = start + n;
  }
  return b;
}

static float space_for(int prev, int cls, float size) {
  float mu = size / 18.0f;
  if (prev < 0) return 0;
  if (cls == BIN && (prev == BIN || prev == REL || prev == OPEN || prev == PUNCT ||
                     prev == OP))
    return 0; /* a unary sign */
  if (cls == BIN || prev == BIN) return 4 * mu;
  if (cls == REL || prev == REL) return 5 * mu;
  if (prev == PUNCT) return 3 * mu;
  if (cls == OP || prev == OP) return 3 * mu;
  return 0;
}

/* Superscript and subscript after a base of box b placed at bx. */
static Box scripts(Parser *ps, List *out, Style st, Box b, float bx, bool op) {
  Style ss = st;
  List sup = {0}, sub = {0};
  Box bs = {0, 0, 0}, bb = {0, 0, 0};
  bool has_sup = false, has_sub = false;
  float up, down, x;
  ss.size = st.size * 0.7f;
  for (;;) {
    while (*ps->s == ' ') ps->s++;
    if (*ps->s == '^' && !has_sup) {
      ps->s++;
      bs = parse_arg(ps, &sup, ss);
      has_sup = true;
    } else if (*ps->s == '_' && !has_sub) {
      ps->s++;
      bb = parse_arg(ps, &sub, ss);
      has_sub = true;
    } else if (*ps->s == '\'') {
      /* a prime is a superscript */
      Box p;
      ps->s++;
      p = glyph(&sup, ps->family * 2, 0x2032, ss.size, bs.w);
      bs.w += p.w;
      if (p.asc > bs.asc) bs.asc = p.asc;
      has_sup = true;
    } else
      break;
  }
  if (!has_sup && !has_sub) return b;
  (void)op;
  up = b.asc - 0.33f * st.size;
  if (up < 0.36f * st.size) up = 0.36f * st.size;
  down = b.desc + 0.0f;
  if (down < 0.17f * st.size) down = 0.17f * st.size;
  if (has_sup && has_sub) {
    /* keep them apart */
    float gap = (up - bs.desc) - (bb.asc - down);
    if (gap < 0.1f * st.size) down += 0.1f * st.size - gap;
  }
  x = bx + b.w;
  if (has_sup) {
    append(out, &sup, x, up);
    grow(&b, bs, b.w, up);
  }
  if (has_sub) {
    append(out, &sub, x, -down);
    grow(&b, bb, b.w, -down);
  }
  b.w = (has_sup ? bs.w : 0) > (has_sub ? bb.w : 0) ? bs.w : bb.w;
  b.w += x - bx + 0.05f * st.size;
  return b;
}

static int command(const char **ps, char *name, size_t len) {
  const char *s = *ps;
  size_t n = 0;
  if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z')) {
    while (((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z')) && n + 1 < len)
      name[n++] = *s++;
  } else if (*s) {
    name[n++] = *s++;
  }
  name[n] = '\0';
  *ps = s;
  return (int)n;
}

/* One atom at x = 0 in out: a character, a command or a group. Sets *cls. */
static Box atom(Parser *ps, List *out, Style st, int *cls) {
  int fam = ps->family, face;
  Box b = {0, 0, 0};
  *cls = ORD;
  if (*ps->s == '{') {
    int c2 = -1;
    ps->s++;
    b = parse_list(ps, out, st, '}', &c2);
    if (*ps->s == '}') ps->s++;
    else fail(ps, "a { is not closed%s", "");
    return b;
  }
  if (*ps->s == '\\') {
    char name[32];
    const Sym *sy;
    int i;
    ps->s++;
    command(&ps->s, name, sizeof name);
    /* spaces */
    if (!strcmp(name, ",")) return (Box){3 * st.size / 18, 0, 0};
    if (!strcmp(name, ":") || !strcmp(name, ">")) return (Box){4 * st.size / 18, 0, 0};
    if (!strcmp(name, ";")) return (Box){5 * st.size / 18, 0, 0};
    if (!strcmp(name, "!")) return (Box){-3 * st.size / 18, 0, 0};
    if (!strcmp(name, " ")) return (Box){glyph(out, fam * 2, ' ', st.size, 0).w, 0, 0};
    if (!strcmp(name, "quad")) return (Box){st.size, 0, 0};
    if (!strcmp(name, "qquad")) return (Box){2 * st.size, 0, 0};
    if (!strcmp(name, "left") || !strcmp(name, "right") || !strcmp(name, "displaystyle")) {
      while (*ps->s == ' ') ps->s++;
      if (!strcmp(name, "displaystyle")) return b;
      if (*ps->s == '.') {
        ps->s++;
        return b;
      }
      return atom(ps, out, st, cls);
    }
    if (!strcmp(name, "mathrm") || !strcmp(name, "rm") || !strcmp(name, "text") ||
        !strcmp(name, "mathbf") || !strcmp(name, "mathsf") || !strcmp(name, "mathit") ||
        !strcmp(name, "operatorname")) {
      Style s2 = st;
      s2.upright = strcmp(name, "mathit") != 0;
      s2.textmode = !strcmp(name, "text");
      return parse_arg(ps, out, s2);
    }
    if (!strcmp(name, "frac") || !strcmp(name, "dfrac") || !strcmp(name, "tfrac")) {
      List num = {0}, den = {0};
      Style s2 = st;
      Box bn, bd;
      float axis = 0.25f * st.size, th = 0.05f * st.size, gap = 0.12f * st.size, w;
      s2.size = st.size * (strcmp(name, "dfrac") ? 0.8f : 1.0f);
      bn = parse_arg(ps, &num, s2);
      bd = parse_arg(ps, &den, s2);
      w = (bn.w > bd.w ? bn.w : bd.w) + 0.2f * st.size;
      append(out, &num, (w - bn.w) / 2, axis + th / 2 + gap + bn.desc);
      append(out, &den, (w - bd.w) / 2, axis - th / 2 - gap - bd.asc);
      rule(out, 0.05f * st.size, axis - th / 2, w - 0.1f * st.size, th);
      b.w = w;
      b.asc = axis + th / 2 + gap + bn.desc + bn.asc;
      b.desc = -(axis - th / 2 - gap - bd.asc) + bd.desc;
      *cls = ORD;
      return b;
    }
    if (!strcmp(name, "sqrt")) {
      List in = {0}, rad = {0};
      Box bi = parse_arg(ps, &in, st), br;
      float need = bi.asc + bi.desc + 0.2f * st.size, sz = st.size, th = 0.05f * st.size;
      br = glyph(&rad, fam * 2, 0x221A, sz, 0);
      if (br.asc + br.desc > 0 && br.asc + br.desc < need) {
        free(rad.it);
        rad.it = NULL;
        rad.n = rad.cap = 0;
        sz *= need / (br.asc + br.desc);
        br = glyph(&rad, fam * 2, 0x221A, sz, 0);
      }
      {
        /* the sign's foot below the content, its top at the bar */
        float dy = -(bi.desc + 0.1f * st.size) + br.desc;
        float top = br.asc + dy;
        append(out, &rad, 0, dy);
        append(out, &in, br.w, 0);
        rule(out, br.w - 0.02f * st.size, top - th, bi.w + 0.1f * st.size, th);
        b.w = br.w + bi.w + 0.12f * st.size;
        b.asc = top;
        b.desc = br.desc - dy;
      }
      return b;
    }
    {
      static const struct {
        const char *name;
        int cp;
      } accents[] = {{"tilde", 0x2DC}, {"widetilde", 0x2DC}, {"hat", 0x2C6},
                     {"widehat", 0x2C6}, {"dot", 0x2D9}, {"ddot", 0xA8},
                     {"vec", 0x2192}, {"check", 0x2C7}, {"breve", 0x2D8},
                     {"acute", 0xB4}, {"grave", 0x60}, {"bar", 0}, {"overline", 0},
                     {NULL, 0}};
      for (i = 0; accents[i].name; i++)
        if (!strcmp(name, accents[i].name)) {
          List base = {0}, acc = {0};
          Box bb = parse_arg(ps, &base, st), ba;
          float gap = 0.06f * st.size, x, y0;
          int g, af;
          float sz = accents[i].cp == 0x2192 ? st.size * 0.7f : st.size;
          append(out, &base, 0, 0);
          if (accents[i].cp == 0) {
            rule(out, 0.05f * st.size, bb.asc + gap, bb.w - 0.05f * st.size,
                 0.05f * st.size);
            b = bb;
            b.asc = bb.asc + gap + 0.05f * st.size;
            return b;
          }
          ba = glyph(&acc, fam * 2, accents[i].cp, sz, 0);
          af = find_glyph(fam * 2, accents[i].cp, &g);
          {
            int x0, y0i, x1, y1;
            float sc = scale_of(af, sz);
            if (!stbtt_GetGlyphBox(&faces[af], g, &x0, &y0i, &x1, &y1)) x0 = x1 = y0i = 0;
            /* centre the mark over the base; slant it a little for italics */
            x = bb.w / 2 - (x0 + x1) * sc / 2 + (st.upright ? 0 : 0.08f * st.size);
            y0 = bb.asc + gap - y0i * sc;
            if (y0 < 0.5f * st.size + gap - y0i * sc) y0 = 0.5f * st.size + gap - y0i * sc;
            append(out, &acc, x, y0);
            b = bb;
            if (y0 + ba.asc > b.asc) b.asc = y0 + ba.asc;
          }
          return b;
        }
    }
    for (i = 0; funcs[i]; i++)
      if (!strcmp(name, funcs[i])) {
        const char *p = funcs[i];
        while (*p) {
          Box g1 = glyph(out, fam * 2, *p++, st.size, b.w);
          grow(&b, g1, b.w, 0);
        }
        *cls = OP;
        return b;
      }
    for (sy = syms; sy->name; sy++)
      if (!strcmp(name, sy->name)) {
        float sz = sy->cls == OP ? st.size * 1.3f : st.size;
        face = fam * 2 + (sy->italic && !st.upright ? 1 : 0);
        b = glyph(out, face, sy->cp, sz, 0);
        if (sy->cls == OP) {
          /* centre a big operator on the maths axis */
          float shift = 0.25f * st.size - (b.asc - b.desc) / 2;
          out->it[out->n - 1].y += shift;
          b.asc += shift;
          b.desc -= shift;
        }
        *cls = sy->cls;
        return b;
      }
    fail(ps, "unknown command \\%s", name);
    {
      const char *p = name;
      b = glyph(out, fam * 2, '\\', st.size, 0);
      while (*p) {
        Box g1 = glyph(out, fam * 2, *p++, st.size, b.w);
        grow(&b, g1, b.w, 0);
      }
    }
    return b;
  }
  {
    int cp = utf8(&ps->s);
    bool letter = (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') ||
                  (cp >= 0x3B1 && cp <= 0x3C9);
    if (cp == '-') cp = 0x2212, *cls = BIN;
    else if (cp == '+' || cp == '*' || cp == 0xD7 || cp == 0xB1) *cls = BIN;
    else if (cp == '=' || cp == '<' || cp == '>' || cp == 0x2264 || cp == 0x2265 ||
             cp == 0x2248 || cp == 0x2192)
      *cls = REL;
    else if (cp == '(' || cp == '[') *cls = OPEN;
    else if (cp == ')' || cp == ']') *cls = CLOSE;
    else if (cp == ',' || cp == ';') *cls = PUNCT;
    if (cp == '*') cp = 0x2217;
    face = fam * 2 + (letter && !st.upright ? 1 : 0);
    return glyph(out, face, cp, st.size, 0);
  }
}

/* Lay out until end (or the string's end): text outside $...$, maths in. */
static Box parse_list(Parser *ps, List *out, Style st, char end, int *last_cls) {
  Box b = {0, 0, 0};
  int prev = -1;
  while (*ps->s && *ps->s != end) {
    if (!st.math || st.textmode) {
      int cp;
      Box g1;
      if (!st.math && *ps->s == '$') {
        Style ms = st;
        int c2 = -1;
        List m = {0};
        Box bm;
        ps->s++;
        ms.math = true;
        bm = parse_list(ps, &m, ms, '$', &c2);
        if (*ps->s == '$') ps->s++;
        else fail(ps, "a $ is not closed%s", "");
        append(out, &m, b.w, 0);
        grow(&b, bm, b.w, 0);
        continue;
      }
      if (!st.math && ps->s[0] == '\\' && ps->s[1] == '$') ps->s++;
      if (st.textmode && *ps->s == '\\') {
        int c2;
        Box ba = atom(ps, out, st, &c2);
        (void)ba;
        continue;
      }
      cp = utf8(&ps->s);
      g1 = glyph(out, ps->family * 2 + (st.upright || !st.math ? 0 : 1), cp,
                 st.size, b.w);
      grow(&b, g1, b.w, 0);
      continue;
    }
    if (*ps->s == ' ') {
      ps->s++;
      continue;
    }
    if (*ps->s == '^' || *ps->s == '_') {
      /* a script with no base */
      Box e = {0, 0, 0};
      Box s2 = scripts(ps, out, st, e, b.w, false);
      grow(&b, s2, b.w, 0);
      prev = ORD;
      continue;
    }
    if (*ps->s == '}' && end != '}') {
      fail(ps, "a } has no {%s", "");
      ps->s++;
      continue;
    }
    {
      List a = {0};
      int cls;
      float sp, x;
      Box ba = atom(ps, &a, st, &cls);
      sp = space_for(prev, cls, st.size);
      x = b.w + sp;
      append(out, &a, x, 0);
      ba = scripts(ps, out, st, ba, x, cls == OP);
      grow(&b, ba, x, 0);
      if (b.w < x + ba.w) b.w = x + ba.w;
      if (cls == BIN || cls == REL) {
        /* the space after is added before the next atom */
      }
      prev = cls;
    }
  }
  if (last_cls) *last_cls = prev;
  return b;
}

static Box layout(const char *s, int family, float pt, List *out, Parser *ps) {
  Style st;
  memset(ps, 0, sizeof *ps);
  ps->s = s ? s : "";
  ps->family = family == FIG_FONT_SERIF ? 1 : 0;
  memset(&st, 0, sizeof st);
  st.size = pt;
  return parse_list(ps, out, st, '\0', NULL);
}

bool fig_text_check(const char *s, char *msg, size_t len) {
  List l = {0};
  Parser ps;
  if (!load_faces()) {
    snprintf(msg, len, "the figure fonts are missing");
    return false;
  }
  layout(s, 0, 10, &l, &ps);
  free(l.it);
  if (ps.error) snprintf(msg, len, "%s", ps.msg);
  else if (len) msg[0] = '\0';
  return !ps.error;
}

/* ---- Drawing --------------------------------------------------------------- */

void fig_dl_cmd(FigDL *dl, int type, const float *p);
int fig_dl_op(FigDL *dl, int op, const float rgb[3]);

float fig_text(FigDL *dl, const char *s, int font, float pt, float x, float y,
               float angle, const float rgb[3], float *ascent, float *descent) {
  List l = {0};
  Parser ps;
  Box b;
  float c, sn;
  int i, k, op;
  if (!load_faces()) {
    if (ascent) *ascent = 0;
    if (descent) *descent = 0;
    return 0;
  }
  b = layout(s, font, pt, &l, &ps);
  if (ascent) *ascent = b.asc;
  if (descent) *descent = b.desc;
  if (!dl || l.n == 0) {
    free(l.it);
    return b.w;
  }
  c = cosf(angle * 3.14159265f / 180);
  sn = sinf(angle * 3.14159265f / 180);
  op = fig_dl_op(dl, FIG_OP_FILL, rgb);
#define PT(u, v) \
  (q[0] = x + (u) * c - (v) * sn, q[1] = y - (u) * sn - (v) * c, q)
  for (i = 0; i < l.n; i++) {
    const Item *it = &l.it[i];
    float q[6];
    if (it->rule) {
      float u0 = it->x, v0 = it->y, u1 = it->x + it->w, v1 = it->y + it->h;
      fig_dl_cmd(dl, FIG_MOVE, PT(u0, v0));
      fig_dl_cmd(dl, FIG_LINE, PT(u1, v0));
      fig_dl_cmd(dl, FIG_LINE, PT(u1, v1));
      fig_dl_cmd(dl, FIG_LINE, PT(u0, v1));
      fig_dl_cmd(dl, FIG_CLOSE, q);
    } else {
      stbtt_vertex *v = NULL;
      int nv = stbtt_GetGlyphShape(&faces[it->face], it->glyph, &v);
      float sc = scale_of(it->face, it->size);
      bool open = false;
      for (k = 0; k < nv; k++) {
        float u = it->x + v[k].x * sc, w = it->y + v[k].y * sc;
        if (v[k].type == STBTT_vmove) {
          if (open) fig_dl_cmd(dl, FIG_CLOSE, q);
          fig_dl_cmd(dl, FIG_MOVE, PT(u, w));
          open = true;
        } else if (v[k].type == STBTT_vline) {
          fig_dl_cmd(dl, FIG_LINE, PT(u, w));
        } else if (v[k].type == STBTT_vcurve) {
          float cu = it->x + v[k].cx * sc, cw = it->y + v[k].cy * sc, r[4];
          PT(cu, cw);
          r[0] = q[0];
          r[1] = q[1];
          PT(u, w);
          r[2] = q[0];
          r[3] = q[1];
          fig_dl_cmd(dl, FIG_QUAD, r);
        } else if (v[k].type == STBTT_vcubic) {
          float r[6];
          PT(it->x + v[k].cx * sc, it->y + v[k].cy * sc);
          r[0] = q[0];
          r[1] = q[1];
          PT(it->x + v[k].cx1 * sc, it->y + v[k].cy1 * sc);
          r[2] = q[0];
          r[3] = q[1];
          PT(u, w);
          r[4] = q[0];
          r[5] = q[1];
          fig_dl_cmd(dl, FIG_CUBIC, r);
        }
      }
      if (open) fig_dl_cmd(dl, FIG_CLOSE, q);
      stbtt_FreeShape(&faces[it->face], v);
    }
  }
#undef PT
  dl->op[op].count = dl->ncmd - dl->op[op].first;
  free(l.it);
  return b.w;
}

/* ---- Rasterising ------------------------------------------------------------ */

/* Fill a shape given as stb_truetype vertices (in points) into an 8-bit
   coverage image, then blend it onto rgba in colour rgb. */
static void fill_coverage(unsigned char *rgba, int w, int h, stbtt_vertex *v, int nv,
                          float scale, const float rgb[3], const int clip[4]) {
  float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
  int i, x0, y0, x1, y1, bw, bh, x, y;
  stbtt__bitmap bm;
  unsigned char *cov;
  if (nv <= 0) return;
  for (i = 0; i < nv; i++) {
    float px = v[i].x, py = v[i].y;
    if (px < minx) minx = px;
    if (px > maxx) maxx = px;
    if (py < miny) miny = py;
    if (py > maxy) maxy = py;
    if (v[i].type == STBTT_vcurve || v[i].type == STBTT_vcubic) {
      if (v[i].cx < minx) minx = v[i].cx;
      if (v[i].cx > maxx) maxx = v[i].cx;
      if (v[i].cy < miny) miny = v[i].cy;
      if (v[i].cy > maxy) maxy = v[i].cy;
    }
    if (v[i].type == STBTT_vcubic) {
      if (v[i].cx1 < minx) minx = v[i].cx1;
      if (v[i].cx1 > maxx) maxx = v[i].cx1;
      if (v[i].cy1 < miny) miny = v[i].cy1;
      if (v[i].cy1 > maxy) maxy = v[i].cy1;
    }
  }
  x0 = (int)floorf(minx * scale) - 1;
  y0 = (int)floorf(miny * scale) - 1;
  x1 = (int)ceilf(maxx * scale) + 1;
  y1 = (int)ceilf(maxy * scale) + 1;
  if (x0 < clip[0]) x0 = clip[0];
  if (y0 < clip[1]) y0 = clip[1];
  if (x1 > clip[2]) x1 = clip[2];
  if (y1 > clip[3]) y1 = clip[3];
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > w) x1 = w;
  if (y1 > h) y1 = h;
  bw = x1 - x0;
  bh = y1 - y0;
  if (bw <= 0 || bh <= 0) return;
  cov = calloc((size_t)bw * (size_t)bh, 1);
  if (!cov) return;
  bm.w = bw;
  bm.h = bh;
  bm.stride = bw;
  bm.pixels = cov;
  /* the points are y-down already, as the bitmap: no flip (invert = 0) */
  stbtt_Rasterize(&bm, 0.25f, v, nv, scale, scale, 0, 0, x0, y0, 0, NULL);
  for (y = 0; y < bh; y++)
    for (x = 0; x < bw; x++) {
      unsigned a = cov[(size_t)y * bw + x];
      unsigned char *p;
      if (!a) continue;
      p = rgba + 4 * ((size_t)(y + y0) * (size_t)w + (size_t)(x + x0));
      p[0] = (unsigned char)((p[0] * (255 - a) + (unsigned)(rgb[0] * 255 + 0.5f) * a) / 255);
      p[1] = (unsigned char)((p[1] * (255 - a) + (unsigned)(rgb[1] * 255 + 0.5f) * a) / 255);
      p[2] = (unsigned char)((p[2] * (255 - a) + (unsigned)(rgb[2] * 255 + 0.5f) * a) / 255);
    }
  free(cov);
}

/* The vertex type is private to this file, so the raster output builds its
   shapes through these. */
typedef struct {
  stbtt_vertex *v;
  int n, cap;
} FigShape;

void *fig_shape_new(void) { return calloc(1, sizeof(FigShape)); }

void fig_shape_free(void *sh) {
  FigShape *s = sh;
  if (!s) return;
  free(s->v);
  free(s);
}

void fig_shape_clear(void *sh) { ((FigShape *)sh)->n = 0; }

int fig_shape_count(void *sh) { return ((FigShape *)sh)->n; }

void fig_shape_add(void *sh, int type, const float *p) {
  FigShape *s = sh;
  stbtt_vertex *v;
  if (s->n == s->cap) {
    int cap = s->cap ? 2 * s->cap : 256;
    stbtt_vertex *t = realloc(s->v, sizeof(stbtt_vertex) * (size_t)cap);
    if (!t) return;
    s->v = t;
    s->cap = cap;
  }
  v = &s->v[s->n++];
  memset(v, 0, sizeof *v);
  switch (type) {
  case FIG_MOVE:
    v->type = STBTT_vmove;
    v->x = p[0];
    v->y = p[1];
    break;
  case FIG_LINE:
    v->type = STBTT_vline;
    v->x = p[0];
    v->y = p[1];
    break;
  case FIG_QUAD:
    v->type = STBTT_vcurve;
    v->cx = p[0];
    v->cy = p[1];
    v->x = p[2];
    v->y = p[3];
    break;
  case FIG_CUBIC:
    v->type = STBTT_vcubic;
    v->cx = p[0];
    v->cy = p[1];
    v->cx1 = p[2];
    v->cy1 = p[3];
    v->x = p[4];
    v->y = p[5];
    break;
  default:
    s->n--;
  }
}

void fig_shape_fill(void *sh, unsigned char *rgba, int w, int h, float scale,
                    const float rgb[3], const int clip[4]) {
  FigShape *s = sh;
  fill_coverage(rgba, w, h, s->v, s->n, scale, rgb, clip);
}
