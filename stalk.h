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

/* ---- memory ---- */

static long long mem[MEM_SIZE];

/* ---- program ---- */

static char *lines[MAX_LINES];
static int nlines;
static int pc;

static void index_source(void);

/* read -- the one impurity. the source is the initial object. */
static int read_source(const char *file)
{
    FILE *f = fopen(file, "r");
    if (!f) return -1;
    char buf[4096];
    while (fgets(buf, sizeof buf, f) && nlines < MAX_LINES) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            buf[--len] = '\0';
        lines[nlines++] = strdup(buf);
    }
    fclose(f);
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
    CALL_, RET_, LOOP_, REPEAT_
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
} Event;

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
 * Read one line and perform it. Obstructions leak to `derived`
 * (the author passes stderr; the referee passes NULL, and keeps
 * its own counsel). Returns 0 when the proof is over.
 */
static int step(FILE *derived, Observer obs, void *ctx)
{
    if (pc < 0 || pc >= nlines)
        return 0;

    const char *ln = lines[pc];
    Event e;
    memset(&e, 0, sizeof e);
    e.line = pc;
    e.text = ln;
    e.degree = -1;
    e.sp_before = sp;
    e.top_before = sp > 0 ? stk[sp - 1] : 0;
    e.target = NOWHERE;
    long u0 = underflow;
    int running = 1;
    long degree;

    pc++;

    if (skip_to[e.line] != NOWHERE) {
        /* a lemma nobody has cited yet. it is not read. */
        pc = skip_to[e.line];
        e.deferred = 1;
    } else if ((degree = try_derived(ln)) >= 0) {
        /* derived functors: R^i observe/publish */
        long long v = pop();
        e.degree = degree;
        e.gave = 1;
        e.given = v;
        if (degree >= 1 && derived)
            fprintf(derived, "H^%ld(X,F) = %lld\n", degree, v);
        /* R^0 = Gamma. pop. discard. not implemented. */
    } else {
        const char *at;
        const Phrase *p = match(ln, &at);
        e.p = p;
        e.at = at;

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
                         e.gave = 1; e.given = pop();         break;
            case JMP_:
                e.target = destination(at + strlen(p->text), &e);
                if (e.target != NOWHERE) pc = e.target;
                break;
            case JZ_:
                e.target = destination(at + strlen(p->text), &e);
                if (!pop() && e.target != NOWHERE) pc = e.target;
                break;
            case JNZ_:
                e.target = destination(at + strlen(p->text), &e);
                if (pop() && e.target != NOWHERE) pc = e.target;
                break;
            case HALT_:  running = 0;                         break;
            case NOP_:                                        break;
            case CALL_: {
                /* by Lemma 2.3: go there, and come back */
                int k = kind_named(p->text + 3);
                char id[32];
                read_id(at + strlen(p->text), id, sizeof id);
                snprintf(e.cite, sizeof e.cite, "%s%s%s",
                         KINDS[k], *id ? " " : "", id);
                int l = find_label(k, id);
                if (l == NOWHERE) {
                    e.unresolved = 1;
                } else {
                    if (rsp < STACK_CAP) rstk[rsp++] = pc;
                    pc = e.target = l + 1;
                }
                break;
            }
            case RET_:   if (rsp > 0) pc = rstk[--rsp];       break;
            case LOOP_:                                       break;
            case REPEAT_:
                if (loop_head[e.line] != NOWHERE && peek())
                    pc = loop_head[e.line] + 1;
                break;
            }
        }
        /* unrecognized lines: silence. */
    }

    e.underflows = underflow - u0;
    e.next = pc;
    if (obs) obs(&e, ctx);
    return running && pc >= 0 && pc < nlines;
}

#endif
