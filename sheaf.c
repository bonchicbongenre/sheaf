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
 * the machine is in stalk.h. the referee reads it too.
 *
 * cc -std=c99 -Wall -Wextra -pedantic -o sheaf sheaf.c
 *
 * usage: sheaf program.sheaf
 *
 * there is no -q flag. there is nothing to quiet.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "stalk.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: sheaf program.sheaf\n");
        return 1;
    }

    if (read_source(argv[1]) != 0) {
        fprintf(stderr, "sheaf: cannot open %s\n", argv[1]);
        return 1;
    }

    /* the reader may give. the reader is never given. */
    the_reader = stdin;

    /* execute -- purely. */
    while (step(stderr, NULL, NULL))
        ;

    free_source();
    return 0;
}
