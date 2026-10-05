#define _POSIX_C_SOURCE 200809L

/*
 * sheafc.c -- an optimizing compiler for sheaf.
 *
 * sheafc evaluates the proof at compile time: constant folding, all
 * the way down. Then it keeps what is observable. By the as-if rule
 * (C11 5.1.2.3) a translation need preserve only observable behaviour,
 * and stdout is empty, so nothing is kept. The obstructions are on
 * stderr, which is observable. They are kept.
 *
 * A loop that does nothing observable may be assumed to terminate
 * (C11 6.8.5p6). sheafc assumes it. A proof that never ends compiles
 * to one that does.
 *
 * A proof that asks the reader cannot be folded. The reader is not a
 * constant.
 *
 * sheafc is the only part of sheaf with output. Its output is a
 * program with none.
 *
 * cc -std=c99 -Wall -Wextra -pedantic -o sheafc sheafc.c
 *
 * usage: sheafc program.sheaf > program.c
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "stalk.h"

#define BUDGET 10000000L

static long lines_read, steps;
static int asks_reader = NOWHERE;

static void fold(const Event *e, void *ctx)
{
    (void)ctx;
    lines_read++;
    if (e->p || e->degree >= 0)
        steps++;
    if (e->p && e->p->op == READ_ && asks_reader == NOWHERE)
        asks_reader = e->line;
}

/* a C string literal for what leaked */
static void literal(const char *s, size_t n)
{
    putchar('"');
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\n') fputs("\\n", stdout);
        else if (c == '"' || c == '\\') printf("\\%c", c);
        else if (c < 32 || c > 126) printf("\\%03o", c);
        else putchar(c);
    }
    putchar('"');
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: sheafc program.sheaf > program.c\n");
        return 1;
    }
    if (read_source(argv[1]) != 0) {
        fprintf(stderr, "sheafc: cannot open %s\n", argv[1]);
        return 1;
    }

    char *leaked = NULL;
    size_t nleaked = 0;
    FILE *derived = open_memstream(&leaked, &nleaked);
    if (!derived) return 1;

    int ended = 0;
    while (asks_reader == NOWHERE) {
        if (!step(derived, fold, NULL)) { ended = 1; break; }
        if (lines_read >= BUDGET) break;
    }
    fclose(derived);

    const char *name = strrchr(argv[1], '/');
    name = name ? name + 1 : argv[1];

    if (asks_reader != NOWHERE) {
        fprintf(stderr, "%s:%d: error: left to the reader. The reader is not a constant.\n",
                name, asks_reader + 1);
        free(leaked);
        free_source();
        return 1;
    }
    if (!ended && nleaked) {
        fprintf(stderr, "%s: error: the obstruction leaks and does not stop. It cannot be folded.\n",
                name);
        free(leaked);
        free_source();
        return 1;
    }
    if (!ended)
        fprintf(stderr, "%s: warning: the loop does nothing observable. "
                "It is assumed to terminate (C11 6.8.5p6).\n", name);

    printf("/*\n * %s, compiled by sheafc.\n *\n", name);
    if (ended)
        printf(" * %d lines, %ld steps, evaluated at compile time.\n", nlines, steps);
    else
        printf(" * %d lines. sheafc read %ld of them and did not reach the end.\n"
               " * The loop does nothing observable, so it is assumed to\n"
               " * terminate (C11 6.8.5p6).\n", nlines, lines_read);
    if (nleaked)
        printf(" * stdout is empty, so nothing is kept (C11 5.1.2.3). The\n"
               " * obstruction is on stderr, which is observable. It is kept.\n */\n\n"
               "#include <stdio.h>\n\n");
    else
        printf(" * Nothing observable happens, so nothing is kept (C11 5.1.2.3).\n */\n\n");

    printf("int main(void)\n{\n");
    if (nleaked) {
        printf("    fputs(");
        literal(leaked, nleaked);
        printf(", stderr);\n");
    }
    printf("    return 0;\n}\n");

    free(leaked);
    free_source();
    return 0;
}
