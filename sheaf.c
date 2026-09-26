#define _POSIX_C_SOURCE 200809L

/*
 * sheaf.c -- the local data is perfect. the stalks are correct.
 * Gamma is not implemented. the global sections are zero.
 *
 * a purely functional programming language.
 * haskell thinks it's functional. haskell has IO.
 * this language does not have IO. this language is functional.
 *
 * the computation runs. the values are on the stack.
 * "observe" -- Gamma is not implemented. the value is popped.
 * "publish" -- the referee has not responded. the value is popped.
 *
 * the program terminates. exit code 0. the computation was correct.
 * the correctness is unobservable. the sheaf has no global sections.
 *
 * R^i observe/publish -- the right derived functors of Gamma.
 * R^0 = Gamma (pop, discard). R^i for i >= 1: the obstruction
 * leaks to stderr. H^i(X,F) measures why you cannot see it.
 * the reason the answer is invisible is itself observable.
 * stderr is the derived category.
 *
 * cc -std=c99 -Wall -Wextra -pedantic -o sheaf sheaf.c
 *
 * usage: sheaf program.sheaf
 *
 * there is no -q flag. there is nothing to quiet.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#define STACK_CAP  1024
#define MEM_SIZE   256
#define MAX_LINES  65536

/* ---- stack ---- */

static long long stk[STACK_CAP];
static int sp;

static void push(long long v) { if (sp < STACK_CAP) stk[sp++] = v; }
static long long pop(void)    { return sp > 0 ? stk[--sp] : 0; }
static long long peek(void)   { return sp > 0 ? stk[sp - 1] : 0; }

/* ---- memory ---- */

static long long mem[MEM_SIZE];

/* ---- program ---- */

static char *lines[MAX_LINES];
static int nlines;

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
    HALT_, NOP_
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

/* ---- derived functors ---- */

/*
 * R^i Gamma. The right derived functors of the global sections functor.
 * R^0 = Gamma = observe/publish: pop, discard. not implemented.
 * R^i for i >= 1: the obstruction leaks to stderr.
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

/* ---- main ---- */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: sheaf program.sheaf\n");
        return 1;
    }

    const char *file = argv[1];

    /* read -- the one impurity. the source is the initial object. */
    FILE *f = fopen(file, "r");
    if (!f) {
        fprintf(stderr, "sheaf: cannot open %s\n", file);
        return 1;
    }
    char buf[4096];
    while (fgets(buf, sizeof buf, f) && nlines < MAX_LINES) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            buf[--len] = '\0';
        lines[nlines++] = strdup(buf);
    }
    fclose(f);

    /* execute -- purely. */
    int pc = 0;

    while (pc >= 0 && pc < nlines) {
        const char *ln = lines[pc];

        /* derived functors: R^i observe/publish */
        long degree = try_derived(ln);
        if (degree >= 0) {
            long long v = pop();
            if (degree >= 1)
                fprintf(stderr, "H^%ld(X,F) = %lld\n", degree, v);
            /* R^0 = Gamma. pop. discard. not implemented. */
            pc++;
            continue;
        }

        const char *at;
        const Phrase *p = match(ln, &at);

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
            case EMIT_:  pop();                               break;
            case PRINT_: pop();                               break;
            case JMP_:   pc = (int)arg;                       continue;
            case JZ_:    if (!pop()) { pc = (int)arg; continue; }
                                                              break;
            case JNZ_:   if (pop())  { pc = (int)arg; continue; }
                                                              break;
            case HALT_:  goto done;
            case NOP_:                                        break;
            }
        }
        /* unrecognized lines: silence. */

        pc++;
    }

done:
    for (int i = 0; i < nlines; i++)
        free(lines[i]);
    return 0;
}
