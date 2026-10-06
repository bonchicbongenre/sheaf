/*
 * stalk.h -- the data at a point.
 *
 * the stack, the memory, the phrases, and one step. the author
 * (sheaf.c) runs the steps. the referee (referee.c) reads alongside
 * them. both reach the same stalk. neither reaches the global section.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef STALK_H
#define STALK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#define STACK_CAP  1024
#define MEM_SIZE   256
#define MAX_LINES  65536

/* ---- stack ---- */

/*
 * underflow counts the pops from an empty stack: hypotheses used
 * and never introduced. the author does not look at it.
 */
static long long stk[STACK_CAP];
static int sp;
static long underflow;

static void push(long long v) { if (sp < STACK_CAP) stk[sp++] = v; }
static long long pop(void)    { if (sp > 0) return stk[--sp]; underflow++; return 0; }
static long long peek(void)   { if (sp > 0) return stk[sp - 1]; underflow++; return 0; }

/* where each citation was made, so the reader can come back */
static int rstk[STACK_CAP];
static int rsp;

/* each assumption made for contradiction, and how deep the stack was */
static int frames[STACK_CAP];
static int nframes;

/* ---- memory ---- */

static long long mem[MEM_SIZE];

/* ---- program ---- */

static char *lines[MAX_LINES];
static int nlines;
static int pc;

/*
 * The reader. "Left to the reader." reads one number from here.
 * The author sets it to stdin. The referee leaves it empty: the
 * referee does not do exercises.
 */
static FILE *the_reader;

static void index_source(void);

/* the lines of a file, as written. -1 if it is not there. */
static int read_lines(const char *file, char ***out)
{
    FILE *f = fopen(file, "r");
    if (!f) return -1;
    int n = 0, cap = 64;
    char **v = malloc((size_t)cap * sizeof *v);
    char buf[4096];
    while (v && fgets(buf, sizeof buf, f) && n < MAX_LINES) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            buf[--len] = '\0';
        if (n == cap) {
            char **w = realloc(v, (size_t)(cap *= 2) * sizeof *v);
            if (!w) break;
            v = w;
        }
        v[n++] = strdup(buf);
    }
    fclose(f);
    *out = v;
    return v ? n : -1;
}

/*
 * An erratum is the paper it corrects. Its first line names the
 * paper: "Erratum to [storage.sheaf]." (or "Corrigendum to [...]").
 * Each line "Line 13 should read: ..." replaces that line of the
 * paper, and "Line 13 should be deleted." leaves it blank. Nothing
 * else in an erratum is part of the paper. An erratum to an erratum
 * corrects the erratum, which then corrects its paper: eight deep at
 * most.
 */
#define ERRATA_DEPTH 8

static char erratum_chain[ERRATA_DEPTH][64];  /* what the manuscript corrects, in turn */
static int  erratum_depth;
static int  erratum_missing;                  /* and the last of them is not there */
static int  corrections;                      /* lines the manuscript itself corrects */

/*
 * A response to the referee is the paper too, as revised by its own
 * "Line N should read:" lines. With none, it is the paper unchanged.
 */
static int  responding;

/* 1 an erratum, 2 a response to the referee, 0 neither; and the paper's name */
static int erratum_heading(const char *ln, char *name, size_t cap)
{
    const char *s = NULL;
    int kind = 1;
    while (isspace((unsigned char)*ln)) ln++;
    if (strncasecmp(ln, "erratum to [", 12) == 0) s = ln + 12;
    else if (strncasecmp(ln, "corrigendum to [", 16) == 0) s = ln + 16;
    else if (strncasecmp(ln, "response to the referee, on [", 29) == 0) { s = ln + 29; kind = 2; }
    const char *t = s ? strchr(s, ']') : NULL;
    if (!t) return 0;
    size_t n = (size_t)(t - s);
    if (n >= cap) n = cap - 1;
    memcpy(name, s, n);
    name[n] = '\0';
    return kind;
}

/* "Line 13 should read: ...": 13, and the text. 0 if it is not a correction. */
static long correction(const char *ln, const char **text)
{
    while (isspace((unsigned char)*ln)) ln++;
    if (strncasecmp(ln, "line ", 5) != 0 || !isdigit((unsigned char)ln[5]))
        return 0;
    char *end;
    long k = strtol(ln + 5, &end, 10);
    if (k < 1 || k > MAX_LINES)
        return 0;
    if (strncasecmp(end, " should read:", 13) == 0) {
        end += 13;
        while (*end == ' ') end++;
        *text = end;
        return k;
    }
    if (strncasecmp(end, " should be deleted", 18) == 0) {
        *text = "";
        return k;
    }
    return 0;
}

/* the text as it should read, out of its quotation marks */
static char *as_corrected(const char *s)
{
    size_t n = strlen(s);
    if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
        char *t = malloc(n - 1);
        if (t) { memcpy(t, s + 1, n - 2); t[n - 2] = '\0'; }
        return t;
    }
    return strdup(s);
}

/* read -- an impurity. the source is the initial object. the reader is the other. */
static int read_source(const char *file)
{
    char **v, name[64], path[1200];
    int n = read_lines(file, &v);
    if (n < 0) return -1;
    const char *slash = strrchr(file, '/');
    int dir = slash ? (int)(slash - file) + 1 : 0;

    int kind;
    while (n > 0 && erratum_depth < ERRATA_DEPTH && !erratum_missing &&
           (kind = erratum_heading(v[0], name, sizeof name))) {
        if (erratum_depth == 0 && kind == 2) responding = 1;
        snprintf(erratum_chain[erratum_depth++], sizeof erratum_chain[0], "%s", name);
        snprintf(path, sizeof path, "%.*s%s", dir, file, name);
        char **t;
        int m = read_lines(path, &t);
        if (m < 0) { t = NULL; m = 0; erratum_missing = 1; }
        for (int i = 1; i < n; i++) {
            const char *text;
            long k = correction(v[i], &text);
            if (!k) continue;
            if (k > m) {
                /* a line the paper does not have: it is extended to it */
                char **w = realloc(t, (size_t)k * sizeof *t);
                if (!w) continue;
                t = w;
                while (m < k) t[m++] = strdup("");
            }
            free(t[k - 1]);
            t[k - 1] = as_corrected(text);
            if (erratum_depth == 1) corrections++;
        }
        for (int i = 0; i < n; i++) free(v[i]);
        free(v);
        v = t;
        n = m;
    }

    for (int i = 0; i < n; i++) {
        if (nlines < MAX_LINES) lines[nlines++] = v[i];
        else free(v[i]);
    }
    free(v);
    index_source();
    return 0;
}

static void free_source(void)
{
    for (int i = 0; i < nlines; i++)
        free(lines[i]);
}

/* ---- helpers ---- */

static const char *cistrstr(const char *h, const char *n)
{
    size_t nl = strlen(n);
    if (!nl) return h;
    for (; *h; h++)
        if (strncasecmp(h, n, nl) == 0)
            return h;
    return NULL;
}

static long long scanint(const char *s, int *ok)
{
    *ok = 0;
    for (; *s; s++) {
        if (s[0] == '-' && isdigit((unsigned char)s[1])) {
            *ok = 1;
            return strtoll(s, NULL, 10);
        }
        if (isdigit((unsigned char)*s)) {
            *ok = 1;
            return strtoll(s, NULL, 10);
        }
    }
    return 0;
}

/* ---- phrase table ---- */

/*
 * The language of mathematics. Each phrase is a standard piece of
 * mathematical prose. The computation is a proof. The proof is
 * correct. The proof has no observable consequences.
 */

enum {
    PUSH, PUSH0, POP_, DUP_, SWAP_,
    ADD_, SUB_, MUL_, DIV_,
    LOAD_, STORE_,
    EMIT_, PRINT_,
    JMP_, JZ_, JNZ_,
    HALT_, NOP_,
    CALL_, RET_, LOOP_, REPEAT_,
    COVER_, TRANS_, GLUE_,
    READ_,
    YONEDA_, NONSENSE_, SIMILAR_,
    ASSUME_, CONTRA_,
    CITEFILE_, RETRACT_,
    TRIPLE_, COCYCLE_, SHEAFIFY_
};

typedef struct {
    const char *text;
    int         op;
    int         has_arg;
} Phrase;

static const Phrase PH[] = {
    /* full forms */
    { "suppose",                       PUSH,   1 },
    { "trivially",                     PUSH0,  0 },
    { "without loss of generality",    POP_,   0 },
    { "recall",                        DUP_,   0 },
    { "by duality",                    SWAP_,  0 },
    { "direct sum",                    ADD_,   0 },
    { "restrict",                      SUB_,   0 },
    { "tensor",                        MUL_,   0 },
    { "localize",                      DIV_,   0 },
    { "pullback",                      LOAD_,  0 },
    { "pushforward",                   STORE_, 0 },
    { "observe",                       EMIT_,  0 },
    { "publish",                       PRINT_, 0 },
    { "see",                           JMP_,   1 },
    { "vacuously",                     JZ_,    1 },
    { "nontrivially",                  JNZ_,   1 },
    { "QED",                           HALT_,  0 },
    { "clearly",                       NOP_,   0 },
    { "it is well known",              NOP_,   0 },
    /* proof technique */
    { "by lemma",                      CALL_,  1 },
    { "by proposition",                CALL_,  1 },
    { "by corollary",                  CALL_,  1 },
    { "by claim",                      CALL_,  1 },
    { "by theorem",                    CALL_,  1 },
    { "this proves the",               RET_,   0 },
    { "by induction",                  LOOP_,  0 },
    { "this completes the induction",  REPEAT_, 0 },
    /* gluing */
    { "cover",                         COVER_, 0 },
    { "the transition",                TRANS_, 0 },
    { "by gluing",                     GLUE_,  0 },
    { "the triple overlap",            TRIPLE_, 0 },
    { "one checks the cocycle condition", COCYCLE_, 0 },
    { "sheafify",                      SHEAFIFY_, 0 },
    { "sheafification",                SHEAFIFY_, 0 },
    /* the reader */
    { "left to the reader",            READ_,  0 },
    /* abstract nonsense */
    { "by yoneda",                     YONEDA_,   0 },
    { "by abstract nonsense",          NONSENSE_, 0 },
    { "similarly",                     SIMILAR_,  0 },
    /* proof by contradiction */
    { "assume for contradiction",      ASSUME_,   0 },
    { "contradiction",                 CONTRA_,   0 },
    /* the literature: a citation is not a reading */
    { "by [",                          CITEFILE_, 0 },
    /* the record: a retraction withdraws what has not yet leaked */
    { "retract",                       RETRACT_,  0 },
    /* abbreviations -- the working mathematician's shorthand */
    { "WLOG",                          POP_,   0 },
    { "wlog",                          POP_,   0 },
    { "iff",                           JZ_,    1 },
    { "cf.",                           JMP_,   1 },
    { "cf ",                           JMP_,   1 },
    { "op.",                           SWAP_,  0 },
    { "resp.",                         DUP_,   0 },
    { "TFAE",                          NOP_,   0 },
    { "NTS",                           NOP_,   0 },
    { "WTS",                           NOP_,   0 },
    { "RTP",                           NOP_,   0 },
    { "s.t.",                          NOP_,   0 },
    { NULL, 0, 0 }
};

static const Phrase *match(const char *ln, const char **where)
{
    const Phrase *best = NULL;
    const char *bpos = NULL;

    for (const Phrase *p = PH; p->text; p++) {
        const char *at = cistrstr(ln, p->text);
        if (at && (!bpos || at < bpos)) {
            best = p;
            bpos = at;
        }
    }
    *where = bpos;
    return best;
}

/* ---- headings ---- */

/*
 * A heading names a line: "Lemma 2.3." Lemmas, propositions,
 * corollaries and claims are lazy. A reader who arrives at one
 * without citing it skips to the end of its proof. Theorems and
 * cases are read where they stand.
 */
static const char *KINDS[] = {
    "Lemma", "Proposition", "Corollary", "Claim", "Theorem", "Case", NULL
};
#define NLAZY 4

#define NOWHERE    (-1)
#define MAX_LABELS 4096

typedef struct {
    int  kind;
    char id[32];
    int  line;
} Label;

static Label labels[MAX_LABELS];
static int nlabels;
static int skip_to[MAX_LINES];     /* a lazy heading: the line after its proof */
static int loop_head[MAX_LINES];   /* a completed induction: where it began */

/* "2.3." -> "2.3" */
static const char *read_id(const char *s, char *id, size_t cap)
{
    size_t n = 0;
    while (*s == ' ') s++;
    while ((isdigit((unsigned char)*s) || *s == '.') && n + 1 < cap)
        id[n++] = *s++;
    while (n > 0 && id[n - 1] == '.')
        n--;
    id[n] = '\0';
    return s;
}

/* a heading begins its line, capitalized. returns its kind. */
static int heading(const char *ln, char *id, size_t cap)
{
    while (isspace((unsigned char)*ln)) ln++;
    for (int k = 0; KINDS[k]; k++) {
        size_t n = strlen(KINDS[k]);
        if (strncmp(ln, KINDS[k], n) != 0)
            continue;
        if (ln[n] != ' ' && ln[n] != '.' && ln[n] != '\0')
            continue;
        read_id(ln + n, id, cap);
        return k;
    }
    return NOWHERE;
}

static int kind_named(const char *s)
{
    for (int k = 0; KINDS[k]; k++)
        if (strncasecmp(s, KINDS[k], strlen(KINDS[k])) == 0)
            return k;
    return NOWHERE;
}

static int find_label(int kind, const char *id)
{
    for (int i = 0; i < nlabels; i++)
        if (labels[i].kind == kind && strcmp(labels[i].id, id) == 0)
            return labels[i].line;
    return NOWHERE;
}

/* the index: every heading, every induction and where it began */
static void index_source(void)
{
    static int open[MAX_LINES];
    int depth = 0;

    nlabels = 0;
    for (int i = 0; i < nlines; i++) {
        skip_to[i] = NOWHERE;
        loop_head[i] = NOWHERE;
    }
    for (int i = 0; i < nlines; i++) {
        char id[32];
        int k = heading(lines[i], id, sizeof id);
        if (k != NOWHERE && nlabels < MAX_LABELS) {
            labels[nlabels].kind = k;
            strcpy(labels[nlabels].id, id);
            labels[nlabels].line = i;
            nlabels++;
        }
        const char *at;
        const Phrase *p = match(lines[i], &at);
        if (p && p->op == LOOP_)
            open[depth++] = i;
        if (p && p->op == REPEAT_ && depth > 0)
            loop_head[i] = open[--depth];
    }
    for (int l = 0; l < nlabels; l++) {
        if (labels[l].kind >= NLAZY)
            continue;
        for (int j = labels[l].line + 1; j < nlines; j++) {
            const char *at;
            const Phrase *p = match(lines[j], &at);
            if (p && p->op == RET_) {
                skip_to[labels[l].line] = j + 1;
                break;
            }
        }
    }
}

/* ---- derived functors ---- */

/*
 * R^i Gamma. The right derived functors of the global sections functor.
 * R^0 = Gamma = observe/publish: pop, discard. not implemented.
 * R^i for i >= 1: the obstruction leaks.
 * H^i(X,F) measures why you cannot see the global section.
 * the reason the answer is invisible is itself observable.
 * stderr is the derived category.
 */
static long try_derived(const char *ln)
{
    const char *p = strstr(ln, "R^");
    if (!p) return -1;
    p += 2;
    if (!isdigit((unsigned char)*p)) return -1;
    char *end;
    long degree = strtol(p, &end, 10);
    if (!cistrstr(end, "observe") && !cistrstr(end, "publish"))
        return -1;
    return degree;
}

/* ---- covers ---- */

/*
 * A cover is a list of opens, U1, U2, ..., and a transition on each
 * overlap: an integer 1-cochain on the nerve. "By gluing." asks
 * whether it is a coboundary. An overlap is an edge, one per
 * transition stated. A triple overlap, declared, is a triangle: it
 * implies its three overlaps (a transition not stated is 0), the
 * transitions around it must sum to zero (the cocycle condition), and
 * a cycle that bounds it no longer counts.
 */
#define MAX_OPENS    64
#define MAX_OVERLAPS 256
#define MAX_TRIPLES  256

static char opens[MAX_OPENS][16];
static int nopens;
static int ov_a[MAX_OVERLAPS], ov_b[MAX_OVERLAPS];
static long long ov_c[MAX_OVERLAPS];
static char ov_implied[MAX_OVERLAPS];     /* by a triple overlap; no transition stated */
static int noverlaps;
static int tri_a[MAX_TRIPLES], tri_b[MAX_TRIPLES], tri_c[MAX_TRIPLES];
static int ntriples;

/* the next open named on the line at or after s: "U2" */
static const char *next_open(const char *ln, const char *s, char *name, size_t cap)
{
    for (; *s; s++) {
        if (*s != 'U' || !isdigit((unsigned char)s[1]))
            continue;
        if (s > ln && isalnum((unsigned char)s[-1]))
            continue;
        const char *e = s + 1;
        while (isdigit((unsigned char)*e)) e++;
        if (isalnum((unsigned char)*e))
            continue;
        size_t n = (size_t)(e - s);
        if (n >= cap) n = cap - 1;
        memcpy(name, s, n);
        name[n] = '\0';
        return e;
    }
    return NULL;
}

static int open_named(const char *name)
{
    for (int i = 0; i < nopens; i++)
        if (strcmp(opens[i], name) == 0)
            return i;
    if (nopens == MAX_OPENS)
        return NOWHERE;
    snprintf(opens[nopens], sizeof opens[0], "%s", name);
    return nopens++;
}

/* the value of a transition: the first number that is not an open's name */
static long long transition_value(const char *s)
{
    for (; *s; s++) {
        if (*s == 'U' && isdigit((unsigned char)s[1])) {
            s++;
            while (isdigit((unsigned char)s[1])) s++;
            continue;
        }
        if ((*s == '-' && isdigit((unsigned char)s[1])) || isdigit((unsigned char)*s))
            return strtoll(s, NULL, 10);
    }
    return 0;
}

/* the first overlap of x and y, and which way round it was stated */
static int edge_between(int m, const int *a, const int *b, int x, int y, int *sign)
{
    for (int e = 0; e < m; e++) {
        if (a[e] == x && b[e] == y) { *sign = 1; return e; }
        if (a[e] == y && b[e] == x) { *sign = -1; return e; }
    }
    return NOWHERE;
}

/* the sum of the transitions around a triple overlap */
static long long around(int t)
{
    int v[4] = { tri_a[t], tri_b[t], tri_c[t], tri_a[t] };
    long long s = 0;
    for (int i = 0; i < 3; i++) {
        int sign, e = edge_between(noverlaps, ov_a, ov_b, v[i], v[i + 1], &sign);
        if (e != NOWHERE) s += sign > 0 ? ov_c[e] : -ov_c[e];
    }
    return s;
}

/* the first triple overlap where the cocycle condition fails, or NOWHERE */
static int cocycle_fails(long long *sum)
{
    for (int t = 0; t < ntriples; t++)
        if ((*sum = around(t)) != 0)
            return t;
    return NOWHERE;
}

/* "U1, U2 and U3" */
static void triple_named(int t, char *out, size_t cap)
{
    snprintf(out, cap, "%s, %s and %s", opens[tri_a[t]], opens[tri_b[t]], opens[tri_c[t]]);
}

/*
 * The class of an integer 1-cochain on a graph, in H^1(graph; Z) = Z^b1.
 * A spanning forest fixes a potential f. Each edge outside the forest
 * closes one cycle, and the cochain's sum around it is f(a) + c - f(b).
 * The cochain is a coboundary iff every sum is zero. Returns b1; the
 * sums go in cls, which has room for m. If tree_out is given, which
 * edges are in the forest goes there.
 */
static int cech_h1(int n, int m, const int *a, const int *b,
                   const long long *c, long long *cls, int *tree_out)
{
    int *seen = calloc((size_t)(n ? n : 1), sizeof *seen);
    int *queue = calloc((size_t)(n ? n : 1), sizeof *queue);
    int *tree = calloc((size_t)(m ? m : 1), sizeof *tree);
    long long *f = calloc((size_t)(n ? n : 1), sizeof *f);
    int b1 = 0;

    if (!seen || !queue || !tree || !f)
        goto out;
    for (int r = 0; r < n; r++) {
        if (seen[r]) continue;
        int head = 0, tail = 0;
        seen[r] = 1;
        queue[tail++] = r;
        while (head < tail) {
            int u = queue[head++];
            for (int e = 0; e < m; e++) {
                if (tree[e] || a[e] == b[e]) continue;
                if (a[e] == u && !seen[b[e]]) {
                    f[b[e]] = f[u] + c[e];
                    seen[b[e]] = 1; tree[e] = 1; queue[tail++] = b[e];
                } else if (b[e] == u && !seen[a[e]]) {
                    f[a[e]] = f[u] - c[e];
                    seen[a[e]] = 1; tree[e] = 1; queue[tail++] = a[e];
                }
            }
        }
    }
    for (int e = 0; e < m; e++)
        if (!tree[e])
            cls[b1++] = f[a[e]] + c[e] - f[b[e]];
    if (tree_out)
        memcpy(tree_out, tree, (size_t)m * sizeof *tree);
out:
    free(seen); free(queue); free(tree); free(f);
    return b1;
}

static long long gcd_ll(long long x, long long y)
{
    if (x < 0) x = -x;
    if (y < 0) y = -y;
    while (y) { long long t = x % y; x = y; y = t; }
    return x;
}

/*
 * H^1 of the nerve with its triangles filled: the cycles of the graph,
 * less those that bound. A triangle's boundary, read in the cycles of
 * the spanning forest, is a row; the rows are reduced, and each pivot
 * is a cycle that no longer counts. H^1 is free, so its rank is b1
 * less the pivots. The class, read on the cycles left, goes in out;
 * glued is whether every sum is zero. Returns the rank.
 */
static int cech_h1_filled(long long *out, int *glued)
{
    long long cls[MAX_OVERLAPS];
    int tree[MAX_OVERLAPS], col[MAX_OVERLAPS], pivot[MAX_OVERLAPS] = { 0 };
    int b1 = cech_h1(nopens, noverlaps, ov_a, ov_b, ov_c, cls, tree);

    *glued = 1;
    for (int k = 0; k < b1; k++)
        if (cls[k]) *glued = 0;

    /* each edge outside the forest is a column */
    for (int e = 0, k = 0; e < noverlaps; e++)
        col[e] = tree[e] ? NOWHERE : k++;

    long long *rows = calloc((size_t)(ntriples ? ntriples : 1) * (size_t)(b1 ? b1 : 1), sizeof *rows);
    int *prow = calloc((size_t)(ntriples ? ntriples : 1), sizeof *prow);
    int *pcol = calloc((size_t)(ntriples ? ntriples : 1), sizeof *pcol);
    int npiv = 0;
    if (rows && prow && pcol && b1 > 0)
        for (int t = 0; t < ntriples; t++) {
            long long *r = rows + (size_t)t * (size_t)b1;
            int v[4] = { tri_a[t], tri_b[t], tri_c[t], tri_a[t] };
            for (int i = 0; i < 3; i++) {
                int sign, e = edge_between(noverlaps, ov_a, ov_b, v[i], v[i + 1], &sign);
                if (e != NOWHERE && col[e] != NOWHERE) r[col[e]] += sign;
            }
            for (int q = 0; q < npiv; q++) {
                long long *p = rows + (size_t)prow[q] * (size_t)b1;
                long long fr = r[pcol[q]], fp = p[pcol[q]];
                if (!fr) continue;
                long long g = 0;
                for (int j = 0; j < b1; j++) {
                    r[j] = r[j] * fp - p[j] * fr;
                    g = gcd_ll(g, r[j]);
                }
                if (g > 1)
                    for (int j = 0; j < b1; j++) r[j] /= g;
            }
            for (int j = 0; j < b1; j++)
                if (r[j]) { prow[npiv] = t; pcol[npiv] = j; pivot[j] = 1; npiv++; break; }
        }
    free(rows); free(prow); free(pcol);

    int rank = 0;
    for (int k = 0; k < b1; k++)
        if (!pivot[k]) out[rank++] = cls[k];
    return rank;
}

/* a triple overlap implies its overlaps; a transition not stated is 0 */
static void imply(int x, int y)
{
    int sign;
    if (edge_between(noverlaps, ov_a, ov_b, x, y, &sign) != NOWHERE || noverlaps == MAX_OVERLAPS)
        return;
    ov_a[noverlaps] = x;
    ov_b[noverlaps] = y;
    ov_c[noverlaps] = 0;
    ov_implied[noverlaps] = 1;
    noverlaps++;
}

/* ---- one step ---- */

/*
 * What happened at one line. The author does not ask.
 * The referee does.
 */
typedef struct {
    int           line;        /* the line read */
    const char   *text;
    const Phrase *p;           /* the phrase performed, or NULL */
    const char   *at;          /* where in the line it was found */
    long          degree;      /* R^i: i. otherwise -1 */
    int           sp_before;
    long long     top_before;
    long          underflows;  /* hypotheses used, never introduced */
    int           gave;        /* a value left the stack for Gamma */
    long long     given;
    int           next;        /* the line read next */
    int           deferred;    /* a lazy lemma, arrived at uncited: skipped */
    int           target;      /* where a jump or citation sends the reader */
    int           unresolved;  /* it names a heading that is not there */
    char          cite[48];    /* the heading it names, as named */
    int           glue;        /* by gluing: 1 glued, 0 not, -1 no gluing */
    int           b1;          /* the rank of H^1 of the nerve */
    long          discharged;  /* by abstract nonsense: hypotheses cleared */
    const Phrase *repeat;      /* similarly: the phrase performed again */
    int           repeat_line; /* and the line it came from */
    long          repeat_degree;
    int           contra;      /* 1 reached, 2 from nothing assumed, 3 nothing contradicts */
    int           withdrawn;   /* an obstruction that would have leaked, after a retraction */
    int           cocycle;     /* 1 holds, 2 holds vacuously, 0 fails; NOWHERE not checked */
    char          where[48];   /* the triple overlap where it fails */
    long long     sum;         /* and what the transitions sum to there */
} Event;

/* the paper has been retracted. what it has leaked stays leaked. */
static int retracted;

/*
 * Where a jump sends the reader: a named line ("Case 2"), or a
 * numbered one. Numbered lines count from zero.
 */
static int destination(const char *s, Event *e)
{
    while (*s == ' ') s++;
    int k = kind_named(s);
    if (k != NOWHERE && s[strlen(KINDS[k])] == ' ') {
        char id[32];
        read_id(s + strlen(KINDS[k]), id, sizeof id);
        snprintf(e->cite, sizeof e->cite, "%s%s%s", KINDS[k], *id ? " " : "", id);
        int l = find_label(k, id);
        if (l == NOWHERE) {
            e->unresolved = 1;
            return NOWHERE;
        }
        return l + 1;
    }
    int ok;
    return (int)scanint(s, &ok);
}

typedef void (*Observer)(const Event *e, void *ctx);

/*
 * "Similarly." performs the last line that performed, again. That
 * line is never a "Similarly." itself.
 */
static int last_line = NOWHERE;

static void fresh(Event *e, int line)
{
    memset(e, 0, sizeof *e);
    e->line = line;
    e->text = lines[line];
    e->degree = -1;
    e->sp_before = sp;
    e->top_before = sp > 0 ? stk[sp - 1] : 0;
    e->target = NOWHERE;
    e->glue = NOWHERE;
    e->repeat_line = NOWHERE;
    e->repeat_degree = -1;
    e->cocycle = NOWHERE;
}

/* perform one line: a derived functor, or the leftmost phrase */
static void perform(int line, Event *e, FILE *derived, int *running)
{
    const char *ln = lines[line];
    long degree;

    if ((degree = try_derived(ln)) >= 0) {
        /* derived functors: R^i observe/publish */
        long long v = pop();
        e->degree = degree;
        e->gave = 1;
        e->given = v;
        if (degree >= 1 && retracted)
            e->withdrawn = 1;
        else if (degree >= 1 && derived)
            fprintf(derived, "H^%ld(X,F) = %lld\n", degree, v);
        /* R^0 = Gamma. pop. discard. not implemented. */
    } else {
        const char *at;
        const Phrase *p = match(ln, &at);
        e->p = p;
        e->at = at;

        if (p) {
            long long arg = 0;
            if (p->has_arg) {
                int ok;
                arg = scanint(at + strlen(p->text), &ok);
            }

            long long a, b;
            switch (p->op) {
            case PUSH:   push(arg);                           break;
            case PUSH0:  push(0);                             break;
            case POP_:   pop();                               break;
            case DUP_:   push(peek());                        break;
            case SWAP_:  a = pop(); b = pop();
                         push(a); push(b);                    break;
            case ADD_:   a = pop(); b = pop(); push(b + a);   break;
            case SUB_:   a = pop(); b = pop(); push(b - a);   break;
            case MUL_:   a = pop(); b = pop(); push(b * a);   break;
            case DIV_:   a = pop(); b = pop();
                         push(a ? b / a : 0);                 break;
            case LOAD_:  a = pop();
                         push((a >= 0 && a < MEM_SIZE) ? mem[a] : 0);
                                                              break;
            case STORE_: a = pop(); b = pop();
                         if (b >= 0 && b < MEM_SIZE) mem[b] = a;
                                                              break;
            case EMIT_:  /* Gamma is not implemented. */
            case PRINT_: /* the referee has not responded. */
                         e->gave = 1; e->given = pop();       break;
            case JMP_:
                e->target = destination(at + strlen(p->text), e);
                if (e->target != NOWHERE) pc = e->target;
                break;
            case JZ_:
                e->target = destination(at + strlen(p->text), e);
                if (!pop() && e->target != NOWHERE) pc = e->target;
                break;
            case JNZ_:
                e->target = destination(at + strlen(p->text), e);
                if (pop() && e->target != NOWHERE) pc = e->target;
                break;
            case HALT_:  *running = 0;                        break;
            case NOP_:                                        break;
            case CALL_: {
                /* by Lemma 2.3: go there, and come back */
                int k = kind_named(p->text + 3);
                char id[32];
                read_id(at + strlen(p->text), id, sizeof id);
                snprintf(e->cite, sizeof e->cite, "%s%s%s",
                         KINDS[k], *id ? " " : "", id);
                int l = find_label(k, id);
                if (l == NOWHERE) {
                    e->unresolved = 1;
                } else {
                    if (rsp < STACK_CAP) rstk[rsp++] = pc;
                    pc = e->target = l + 1;
                }
                break;
            }
            case RET_:   if (rsp > 0) pc = rstk[--rsp];       break;
            case LOOP_:                                       break;
            case REPEAT_:
                if (loop_head[line] != NOWHERE && peek())
                    pc = loop_head[line] + 1;
                break;
            case COVER_: {
                /* a new cover: the opens named on the line */
                char name[16];
                const char *s = ln;
                nopens = noverlaps = ntriples = 0;
                while ((s = next_open(ln, s, name, sizeof name)))
                    open_named(name);
                break;
            }
            case TRANS_: {
                /* the transition on two opens */
                char n1[16], n2[16];
                const char *s = next_open(ln, ln, n1, sizeof n1);
                if (s && next_open(ln, s, n2, sizeof n2)) {
                    int i = open_named(n1), j = open_named(n2), sign;
                    int e0 = (i != NOWHERE && j != NOWHERE)
                           ? edge_between(noverlaps, ov_a, ov_b, i, j, &sign) : NOWHERE;
                    if (e0 != NOWHERE && ov_implied[e0]) {
                        /* the transition on an overlap a triple overlap implied */
                        ov_a[e0] = i;
                        ov_b[e0] = j;
                        ov_c[e0] = transition_value(at + strlen(p->text));
                        ov_implied[e0] = 0;
                    } else if (i != NOWHERE && j != NOWHERE && noverlaps < MAX_OVERLAPS) {
                        ov_a[noverlaps] = i;
                        ov_b[noverlaps] = j;
                        ov_c[noverlaps] = transition_value(at + strlen(p->text));
                        ov_implied[noverlaps] = 0;
                        noverlaps++;
                    }
                }
                break;
            }
            case TRIPLE_: {
                /* the triple overlap of three opens: a triangle in the nerve */
                char n1[16], n2[16], n3[16];
                const char *s = next_open(ln, ln, n1, sizeof n1);
                const char *s2 = s ? next_open(ln, s, n2, sizeof n2) : NULL;
                if (s2 && next_open(ln, s2, n3, sizeof n3) && ntriples < MAX_TRIPLES) {
                    int i = open_named(n1), j = open_named(n2), k = open_named(n3);
                    if (i != NOWHERE && j != NOWHERE && k != NOWHERE &&
                        i != j && j != k && i != k) {
                        tri_a[ntriples] = i;
                        tri_b[ntriples] = j;
                        tri_c[ntriples] = k;
                        ntriples++;
                        imply(i, j);
                        imply(j, k);
                        imply(i, k);
                    }
                }
                break;
            }
            case COCYCLE_: {
                /* one checks the cocycle condition. one does not. the referee does. */
                long long sum;
                int t = cocycle_fails(&sum);
                e->cocycle = !ntriples ? 2 : t == NOWHERE ? 1 : 0;
                if (t != NOWHERE) {
                    triple_named(t, e->where, sizeof e->where);
                    e->sum = sum;
                }
                break;
            }
            case SHEAFIFY_:
                /* the sheafification of a sheaf is the sheaf */
                break;
            case GLUE_: {
                /* is the cochain a cocycle, and a coboundary? 1 if it glues */
                long long cls[MAX_OVERLAPS], sum;
                int t = cocycle_fails(&sum), glued, rank = 0;
                if (t != NOWHERE) {
                    glued = 0;
                    e->cocycle = 0;
                    triple_named(t, e->where, sizeof e->where);
                    e->sum = sum;
                } else {
                    rank = cech_h1_filled(cls, &glued);
                }
                push(glued);
                e->glue = glued;
                e->b1 = rank;
                if (!glued && retracted) {
                    e->withdrawn = 1;
                } else if (!glued && derived && t != NOWHERE) {
                    fprintf(derived, "H^1(U,Z): not a cocycle on %s; the sum is %lld\n",
                            e->where, sum);
                } else if (!glued && derived) {
                    fprintf(derived, "H^1(U,Z) = Z^%d; the class is (", rank);
                    for (int i = 0; i < rank; i++)
                        fprintf(derived, "%s%lld", i ? ", " : "", cls[i]);
                    fprintf(derived, ")\n");
                }
                break;
            }
            case READ_: {
                /* the reader gives a number, or nothing. nothing is not 0. */
                long long v;
                if (the_reader && fscanf(the_reader, "%lld", &v) == 1)
                    push(v);
                break;
            }
            case YONEDA_:
                /* an object is its observations. none are kept. */
                break;
            case NONSENSE_:
                /* every open hypothesis, discharged at once */
                e->discharged = sp;
                sp = 0;
                break;
            case SIMILAR_:
                /* the last line that performed, performed again */
                if (last_line != NOWHERE) {
                    Event again;
                    fresh(&again, last_line);
                    perform(last_line, &again, derived, running);
                    e->repeat = again.p;
                    e->repeat_line = last_line;
                    e->repeat_degree = again.degree;
                    e->gave = again.gave;
                    e->given = again.given;
                    e->glue = again.glue;
                    e->b1 = again.b1;
                    e->discharged = again.discharged;
                    e->contra = again.contra;
                    e->withdrawn = again.withdrawn;
                    e->cocycle = again.cocycle;
                    e->sum = again.sum;
                    memcpy(e->where, again.where, sizeof e->where);
                }
                break;
            case CITEFILE_:
                /* by [fermat.sheaf]: cited. not read. */
                break;
            case RETRACT_:
                /* withdrawn. it is still read. */
                retracted = 1;
                break;
            case ASSUME_:
                /* everything from here is derived from what is assumed */
                if (nframes < STACK_CAP) frames[nframes++] = sp;
                break;
            case CONTRA_:
                /*
                 * The top of the stack is the disagreement between what
                 * was assumed and what was derived. Nonzero: the
                 * contradiction is reached, what was derived is
                 * discarded, and the negation, 1, is proved. With
                 * nothing assumed, everything follows.
                 */
                a = pop();
                if (a == 0) {
                    e->contra = 3;
                    if (nframes > 0) nframes--;
                } else if (nframes > 0) {
                    sp = frames[--nframes];
                    push(1);
                    e->contra = 1;
                } else {
                    sp = 0;
                    push(1);
                    e->contra = 2;
                }
                break;
            }
        }
        /* unrecognized lines: silence. */
    }
}

/*
 * Read one line and perform it. Obstructions leak to `derived`
 * (the author passes stderr; the referee passes NULL, and keeps
 * its own counsel). Returns 0 when the proof is over.
 */
static int step(FILE *derived, Observer obs, void *ctx)
{
    if (pc < 0 || pc >= nlines)
        return 0;

    Event e;
    fresh(&e, pc);
    long u0 = underflow;
    int running = 1;

    pc++;

    if (skip_to[e.line] != NOWHERE) {
        /* a lemma nobody has cited yet. it is not read. */
        pc = skip_to[e.line];
        e.deferred = 1;
    } else {
        perform(e.line, &e, derived, &running);
        if (e.degree >= 0 || (e.p && e.p->op != SIMILAR_))
            last_line = e.line;
    }

    e.underflows = underflow - u0;
    e.next = pc;
    if (obs) obs(&e, ctx);
    return running && pc >= 0 && pc < nlines;
}

#endif
