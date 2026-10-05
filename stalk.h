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

/* read -- an impurity. the source is the initial object. the reader is the other. */
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
    CALL_, RET_, LOOP_, REPEAT_,
    COVER_, TRANS_, GLUE_,
    READ_,
    YONEDA_, NONSENSE_, SIMILAR_,
    ASSUME_, CONTRA_
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
    /* the reader */
    { "left to the reader",            READ_,  0 },
    /* abstract nonsense */
    { "by yoneda",                     YONEDA_,   0 },
    { "by abstract nonsense",          NONSENSE_, 0 },
    { "similarly",                     SIMILAR_,  0 },
    /* proof by contradiction */
    { "assume for contradiction",      ASSUME_,   0 },
    { "contradiction",                 CONTRA_,   0 },
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
 * whether it is a coboundary. The nerve is a graph. An overlap is an
 * edge, one per transition stated. Triple overlaps are not seen.
 */
#define MAX_OPENS    64
#define MAX_OVERLAPS 256

static char opens[MAX_OPENS][16];
static int nopens;
static int ov_a[MAX_OVERLAPS], ov_b[MAX_OVERLAPS];
static long long ov_c[MAX_OVERLAPS];
static int noverlaps;

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

/*
 * The class of an integer 1-cochain on a graph, in H^1(graph; Z) = Z^b1.
 * A spanning forest fixes a potential f. Each edge outside the forest
 * closes one cycle, and the cochain's sum around it is f(a) + c - f(b).
 * The cochain is a coboundary iff every sum is zero. Returns b1; the
 * sums go in cls, which has room for m.
 */
static int cech_h1(int n, int m, const int *a, const int *b,
                   const long long *c, long long *cls)
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
out:
    free(seen); free(queue); free(tree); free(f);
    return b1;
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
        if (degree >= 1 && derived)
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
                nopens = noverlaps = 0;
                while ((s = next_open(ln, s, name, sizeof name)))
                    open_named(name);
                break;
            }
            case TRANS_: {
                /* the transition on two opens */
                char n1[16], n2[16];
                const char *s = next_open(ln, ln, n1, sizeof n1);
                if (s && next_open(ln, s, n2, sizeof n2) && noverlaps < MAX_OVERLAPS) {
                    int i = open_named(n1), j = open_named(n2);
                    if (i != NOWHERE && j != NOWHERE) {
                        ov_a[noverlaps] = i;
                        ov_b[noverlaps] = j;
                        ov_c[noverlaps] = transition_value(at + strlen(p->text));
                        noverlaps++;
                    }
                }
                break;
            }
            case GLUE_: {
                /* is the cochain a coboundary? 1 if it glues */
                long long cls[MAX_OVERLAPS];
                int b1 = cech_h1(nopens, noverlaps, ov_a, ov_b, ov_c, cls);
                int glued = 1;
                for (int i = 0; i < b1; i++)
                    if (cls[i]) glued = 0;
                push(glued);
                e->glue = glued;
                e->b1 = b1;
                if (!glued && derived) {
                    fprintf(derived, "H^1(U,Z) = Z^%d; the class is (", b1);
                    for (int i = 0; i < b1; i++)
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
                }
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
