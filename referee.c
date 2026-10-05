#define _POSIX_C_SOURCE 200809L

/*
 * referee.c -- the referee.
 *
 * the referee is not the author. it reads the manuscript while the
 * proof runs beside it, one line at a time, and writes a report.
 * it sees what happens on the stack. it does not see the result.
 *
 * the report goes to stderr, which is where reports go. the
 * recommendation is the exit code:
 *
 *   0  accept
 *   1  minor revision
 *   2  major revision
 *   3  reject
 *
 * the referee reads at most BUDGET lines. then it stops reading.
 *
 * confidential comments to the editor go to file descriptor 3. the
 * editor may not have opened it.
 *
 * cc -std=c99 -Wall -Wextra -pedantic -o referee referee.c
 *
 * usage: referee manuscript.sheaf
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <stdarg.h>
#include <fcntl.h>
#include "stalk.h"

#define BUDGET  100000
#define WIDTH   66

enum { ACCEPT, MINOR, MAJOR, REJECT };

static const char *DECISION_FORM[] = {
    "ACCEPT", "MINOR REVISION", "MAJOR REVISION", "REJECT"
};
static const char *DECISION_PROSE[] = {
    "accept", "minor revision", "major revision", "reject"
};

/* ---- the lack, dressed (the form) and worn (the prose) ---- */

static const char *FIELD[] = {
    "not visible",
    "could not tell",
    "unable to see",
    "see attached",
    "N/A",
    "pending",
    "on file",
};

static const char *WORN[] = {
    "I could not see the result. My comments concern the text.",
    "The result was not visible to me. What follows concerns the text.",
    "I did not see what the proof arrives at. I have read the rest.",
    "The result did not reach me. I have checked what I could.",
    "I have not seen the result. The comments below are on the text alone.",
    "The result was not available to me. I have read the proof closely.",
    "I read the proof without its result. My comments follow.",
};

/* confidential comments to the editor, on file descriptor 3 */
static const char *CONFIDENTIAL[] = {
    "I am not an expert in this area.",
    "I did not check the proofs. Nobody can.",
    "The author seems competent.",
    "I read this on a train.",
    "Please do not send me this author's next paper.",
    "I could not see the result. I do not think the author could either.",
    "This took me four minutes.",
};

#define NFIELD (sizeof FIELD / sizeof FIELD[0])
#define NWORN  (sizeof WORN / sizeof WORN[0])
#define NCONF  (sizeof CONFIDENTIAL / sizeof CONFIDENTIAL[0])

/* ---- what the referee marks, line by line ---- */

typedef struct {
    char seen;
    char prose;          /* a phrase performs from inside a sentence */
    char underflow;      /* a hypothesis used, never introduced */
    char code;           /* 'W' well known, 'C' clearly, 'N' need to show */
    char false_claim;
    int  op;
    const char *phrase;
    int  target;         /* where a jump from here sends the reader */
    long sent;           /* how many times it did */
    char word[64];       /* the word the phrase hides in */
    char claim[160];     /* the claim, as written */
    char denial[64];     /* and its denial */
    char cite[48];       /* the heading a citation or jump names */
    char dangling;       /* and it is not there */
    int  dangling_op;
    char called;         /* a heading someone cited */
} Mark;

static Mark *mark;

static long lines_read, steps, empty_steps;
static long observes, publishes, leaks;
static long claims_checked, claims_false;
static int  have_section, have_obstruction;
static long long section, obstruction;
static int  unfinished, halted;

/* ---- reading ---- */

/*
 * A phrase is an instruction when it is a capitalized whole word at
 * the start of a sentence. Anywhere else it is commentary, and it
 * performs anyway.
 */
static int is_instruction(const char *ln, const char *at, const Phrase *p)
{
    size_t n = strlen(p->text);
    char last = p->text[n - 1];

    if (at > ln && isalpha((unsigned char)at[-1]))
        return 0;
    if (isalpha((unsigned char)last) && isalpha((unsigned char)at[n]))
        return 0;
    if (!isupper((unsigned char)at[0]))
        return 0;

    const char *q = at;
    while (q > ln && (q[-1] == '"' || q[-1] == '('))
        q--;
    const char *r = q;
    while (r > ln && isspace((unsigned char)r[-1]))
        r--;
    if (r == ln)
        return 1;
    return r < q && strchr(".?!:", r[-1]) != NULL;
}

/* the whole word (or words) a phrase was found in */
static void hiding_place(const char *ln, const char *at, const Phrase *p,
                         char *out, size_t cap)
{
    const char *s = at;
    const char *e = at + strlen(p->text);
    while (s > ln && isalpha((unsigned char)s[-1]))
        s--;
    while (*e && isalpha((unsigned char)*e))
        e++;
    while (e > s && !isalpha((unsigned char)e[-1]))
        e--;
    size_t n = (size_t)(e - s);
    if (n >= cap) n = cap - 1;
    memcpy(out, s, n);
    out[n] = '\0';
}

static int word_at(const char *ln, const char *at, const char *w)
{
    size_t n = strlen(w);
    if (at > ln && isalpha((unsigned char)at[-1]))
        return 0;
    if (strncasecmp(at, w, n) != 0)
        return 0;
    return !isalnum((unsigned char)at[n]);
}

/*
 * A claim is a subject, a verb and a number, in one sentence:
 * "the sum is 12", "this is zero", "the stack contains 4".
 * The referee checks it against the stack as the line is reached.
 * If the stack is empty, against the last value to leave it.
 */
static const char *SUBJECTS[] = {
    "result", "answer", "sum", "value", "this", "stack", "obstruction", NULL
};
static const char *VERBS[] = { "is", "was", "equals", "contains", NULL };

static void check_claim(const Event *e, Mark *m)
{
    const char *ln = e->text;

    for (const char *v = ln; *v; v++) {
        const char *verb = NULL;
        for (const char **w = VERBS; *w; w++)
            if (word_at(ln, v, *w)) { verb = *w; break; }
        if (!verb || v[strlen(verb)] != ' ')
            continue;

        /* the number */
        const char *num = v + strlen(verb) + 1;
        long long claimed;
        const char *end;
        if (word_at(ln, num, "zero")) {
            claimed = 0;
            end = num + 4;
        } else {
            const char *d = num;
            if (*d == '-') d++;
            if (!isdigit((unsigned char)*d))
                continue;
            while (isdigit((unsigned char)*d)) d++;
            if (isalnum((unsigned char)*d))
                continue;
            claimed = strtoll(num, NULL, 10);
            end = d;
        }

        /* the subject, nearest the verb, in the same sentence */
        const char *start = ln;
        for (const char *c = ln; c < v; c++)
            if (strchr(".?!:", *c) && c[1] == ' ')
                start = c + 2;
        const char *subj = NULL, *which = NULL;
        for (const char *c = start; c < v; c++)
            for (const char **s = SUBJECTS; *s; s++)
                if (word_at(ln, c, *s)) { subj = c; which = *s; }
        if (!subj)
            continue;

        /* the stack as the line is reached */
        long long actual;
        if (strcmp(which, "obstruction") == 0) {
            if (!have_obstruction) return;
            actual = obstruction;
        } else if (e->sp_before > 0) {
            actual = e->top_before;
        } else if (have_section) {
            actual = section;
        } else {
            return;
        }

        claims_checked++;
        if (actual == claimed)
            return;

        claims_false++;
        m->false_claim = 1;
        const char *q = subj;
        if (q - ln >= 4 && strncasecmp(q - 4, "the ", 4) == 0)
            q -= 4;
        size_t n = (size_t)(end - q);
        if (n >= sizeof m->claim) n = sizeof m->claim - 1;
        memcpy(m->claim, q, n);
        m->claim[n] = '\0';
        m->claim[0] = (char)tolower((unsigned char)m->claim[0]);

        char shown[32];
        if (claimed == 0) snprintf(shown, sizeof shown, "zero");
        else snprintf(shown, sizeof shown, "%lld", claimed);
        if (strcmp(verb, "is") == 0)
            snprintf(m->denial, sizeof m->denial, "It is not %s.", shown);
        else if (strcmp(verb, "was") == 0)
            snprintf(m->denial, sizeof m->denial, "It was not %s.", shown);
        else
            snprintf(m->denial, sizeof m->denial, "It does not.");
        return;
    }
}

static void read_line(const Event *e, void *ctx)
{
    (void)ctx;
    Mark *m = &mark[e->line];
    int first = !m->seen;
    m->seen = 1;
    lines_read++;

    if (first)
        check_claim(e, m);

    if (e->degree >= 0) {
        steps++;
        if (e->degree >= 1)
            leaks++;
        else if (cistrstr(e->text, "publish"))
            publishes++;
        else
            observes++;
    } else if (e->p) {
        const Phrase *p = e->p;
        steps++;
        if (p->op == NOP_)  empty_steps++;
        if (p->op == EMIT_) observes++;
        if (p->op == PRINT_) publishes++;
        if (p->op == HALT_) halted = 1;

        if (is_instruction(e->text, e->at, p)) {
            if (first) {
                if (!strcasecmp(p->text, "it is well known") ||
                    !strcasecmp(p->text, "TFAE"))
                    m->code = 'W';
                else if (!strcasecmp(p->text, "clearly"))
                    m->code = 'C';
                else if (!strcmp(p->text, "NTS") || !strcmp(p->text, "WTS") ||
                         !strcmp(p->text, "RTP"))
                    m->code = 'N';
                else if (p->op == READ_)
                    m->code = 'E';
            }
        } else {
            m->prose = 1;
            m->op = p->op;
            m->phrase = p->text;
            hiding_place(e->text, e->at, p, m->word, sizeof m->word);
            if (p->op == JMP_ || p->op == JZ_ || p->op == JNZ_) {
                m->target = e->target;
                if (e->target != NOWHERE && e->next == e->target)
                    m->sent++;
            }
        }

        if (e->cite[0])
            snprintf(m->cite, sizeof m->cite, "%s", e->cite);
        if (e->unresolved) {
            m->dangling = 1;
            m->dangling_op = p->op;
        }
        if (p->op == CALL_ && e->target != NOWHERE)
            mark[e->target - 1].called = 1;
    }

    if (e->glue == 0)
        leaks++;           /* a gluing that failed: its class leaks */
    if (e->underflows)
        m->underflow = 1;
    if (e->gave) {
        if (e->degree >= 1) { have_obstruction = 1; obstruction = e->given; }
        else                { have_section = 1;     section = e->given; }
    }
}

/* ---- writing ---- */

static char para[16384];
static size_t plen;

static void pf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(para + plen, sizeof para - plen, fmt, ap);
    va_end(ap);
    if (n > 0) {
        plen += (size_t)n;
        if (plen >= sizeof para) plen = sizeof para - 1;
    }
}

/*
 * the paragraph, wrapped; later lines hang under the lead.
 * NB holds a word to its number ("line 1") across the wrap.
 */
#define NB "\001"

static void pend(const char *lead)
{
    size_t ind = strlen(lead), col = ind;
    const char *s = para;
    int first = 1;

    fputs(lead, stderr);
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        const char *w = s;
        while (*s && *s != ' ') s++;
        size_t wl = (size_t)(s - w);
        if (!first && col + 1 + wl > WIDTH) {
            fprintf(stderr, "\n%*s", (int)ind, "");
            col = ind;
        } else if (!first) {
            fputc(' ', stderr);
            col++;
        }
        for (size_t i = 0; i < wl; i++)
            fputc(w[i] == '\001' ? ' ' : w[i], stderr);
        col += wl;
        first = 0;
    }
    fputs("\n\n", stderr);
    plen = 0;
    para[0] = '\0';
}

static const char *WORDS[] = {
    "zero", "one", "two", "three", "four", "five",
    "six", "seven", "eight", "nine", "ten"
};

static void number(char *out, size_t cap, long k, int capital)
{
    if (k >= 0 && k <= 10) snprintf(out, cap, "%s", WORDS[k]);
    else snprintf(out, cap, "%ld", k);
    if (capital) out[0] = (char)toupper((unsigned char)out[0]);
}

static void times(char *out, size_t cap, long k)
{
    if (k == 1) snprintf(out, cap, "once");
    else if (k == 2) snprintf(out, cap, "twice");
    else {
        char n[32];
        number(n, sizeof n, k, 0);
        snprintf(out, cap, "%s times", n);
    }
}

/* "1", "12 and 19", "3, 4, 5 and 9" -- lines carrying a mark */
static int line_list(char *out, size_t cap, int (*has)(const Mark *))
{
    int count = 0, total = 0;
    for (int i = 0; i < nlines; i++)
        if (has(&mark[i])) total++;
    out[0] = '\0';
    for (int i = 0; i < nlines; i++) {
        if (!has(&mark[i])) continue;
        size_t l = strlen(out);
        const char *sep = count == 0 ? NB : (count == total - 1 ? " and " : ", ");
        snprintf(out + l, cap - l, "%s%d", sep, i + 1);
        count++;
    }
    return total;
}

static int has_underflow(const Mark *m) { return m->underflow; }
static int has_w(const Mark *m) { return m->code == 'W'; }
static int has_c(const Mark *m) { return m->code == 'C'; }
static int has_n(const Mark *m) { return m->code == 'N'; }
static int has_e(const Mark *m) { return m->code == 'E'; }

static const char *performs(const Mark *m, char *buf, size_t cap)
{
    switch (m->op) {
    case PUSH:   return "makes a supposition";
    case PUSH0:  return "assumes the trivial case";
    case POP_:   return "discards a hypothesis";
    case DUP_:   return "recalls";
    case SWAP_:  return "dualizes";
    case ADD_:   return "performs a direct sum";
    case SUB_:   return "restricts";
    case MUL_:   return "tensors";
    case DIV_:   return "localizes";
    case LOAD_:  return "performs a pullback";
    case STORE_: return "performs a pushforward";
    case EMIT_:  return "observes";
    case PRINT_: return "publishes";
    case JMP_:
    case JZ_:
    case JNZ_:
        if (m->cite[0])
            snprintf(buf, cap, "%s the reader to %s",
                     m->sent ? "sends" : "may send", m->cite);
        else
            snprintf(buf, cap, "%s the reader to line" NB "%d",
                     m->sent ? "sends" : "may send", m->target + 1);
        return buf;
    case HALT_:  return "ends the proof";
    case CALL_:
        snprintf(buf, cap, "cites %s", m->cite);
        return buf;
    case RET_:    return "ends a lemma";
    case LOOP_:   return "begins an induction";
    case REPEAT_: return "completes an induction";
    case COVER_:  return "declares a cover";
    case TRANS_:  return "sets a transition";
    case GLUE_:   return "glues";
    case READ_:   return "asks the reader";
    default:
        snprintf(buf, cap, "says %s", m->phrase);
        return buf;
    }
}

static int comment_no;

static void comment(void)
{
    char lead[16];
    snprintf(lead, sizeof lead, "%d. ", ++comment_no);
    pend(lead);
}

static unsigned long fnv(const char *s)
{
    unsigned long h = 2166136261UL;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h = (h * 16777619UL) & 0xffffffffUL;
    }
    return h;
}

/* ---- the citation graph ---- */

/*
 * Read from the whole text, not from the run, as a referee reads.
 * A vertex for the text, and one for each lemma, proposition,
 * corollary and claim. An edge for each citation, from the proof it
 * sits in to what it cites. A theorem belongs to the text.
 */
#define MAX_V      (MAX_LABELS + 1)
#define MAX_CYCLES 8

static int nv, ne;
static int vlabel[MAX_V];        /* vertex -> label; the text is -1 */
static int *ea, *eb;
static int pieces, rank1;        /* H^0, H^1 */
static char cycles[MAX_CYCLES][512];
static int ncycles;
static int *color, *path;
static int depth;

static void vertex_name(int v, char *out, size_t cap, int capital)
{
    if (vlabel[v] < 0) {
        snprintf(out, cap, "%s", capital ? "The text" : "the text");
        return;
    }
    const Label *L = &labels[vlabel[v]];
    snprintf(out, cap, "%s%s%s", KINDS[L->kind], L->id[0] ? NB : "", L->id);
}

static void read_citations(void)
{
    int *owner = calloc((size_t)(nlines ? nlines : 1), sizeof *owner);
    ea = calloc((size_t)(nlines ? nlines : 1), sizeof *ea);
    eb = calloc((size_t)(nlines ? nlines : 1), sizeof *eb);
    if (!owner || !ea || !eb) { free(owner); return; }

    nv = 1;
    vlabel[0] = -1;
    for (int l = 0; l < nlabels && nv < MAX_V; l++) {
        if (labels[l].kind >= NLAZY) continue;
        int v = nv++;
        vlabel[v] = l;
        int end = skip_to[labels[l].line];
        if (end == NOWHERE) end = labels[l].line + 1;
        for (int i = labels[l].line; i < end && i < nlines; i++)
            owner[i] = v;
    }

    for (int i = 0; i < nlines; i++) {
        const char *at;
        const Phrase *p = match(lines[i], &at);
        if (!p || p->op != CALL_) continue;
        char id[32];
        read_id(at + strlen(p->text), id, sizeof id);
        int line = find_label(kind_named(p->text + 3), id);
        if (line == NOWHERE) continue;
        int to = 0;
        for (int v = 1; v < nv; v++)
            if (labels[vlabel[v]].line == line) to = v;
        ea[ne] = owner[i];
        eb[ne] = to;
        ne++;
    }

    long long *zero = calloc((size_t)(ne ? ne : 1), sizeof *zero);
    long long *cls = calloc((size_t)(ne ? ne : 1), sizeof *cls);
    if (zero && cls) {
        rank1 = cech_h1(nv, ne, ea, eb, zero, cls);
        pieces = nv - ne + rank1;
    }
    free(zero);
    free(cls);
    free(owner);
}

/* a directed cycle: the argument comes back to where it began */
static void circle_back(int from)
{
    char buf[512] = "", name[64];
    for (int i = from; i < depth; i++) {
        vertex_name(path[i], name, sizeof name, i == from);
        size_t l = strlen(buf);
        snprintf(buf + l, sizeof buf - l, "%s%s",
                 i == from ? "" : (i == from + 1 ? " cites " : ", which cites "), name);
    }
    size_t l = strlen(buf);
    if (depth - from == 1)
        snprintf(buf + l, sizeof buf - l, " cites itself.");
    else {
        vertex_name(path[from], name, sizeof name, 0);
        snprintf(buf + l, sizeof buf - l, ", which cites %s.", name);
    }
    for (int c = 0; c < ncycles; c++)
        if (strcmp(cycles[c], buf) == 0) return;
    if (ncycles < MAX_CYCLES)
        snprintf(cycles[ncycles++], sizeof cycles[0], "%s", buf);
}

static void search(int u)
{
    color[u] = 1;
    path[depth++] = u;
    for (int e = 0; e < ne; e++) {
        if (ea[e] != u) continue;
        int v = eb[e];
        if (color[v] == 1) {
            for (int i = 0; i < depth; i++)
                if (path[i] == v) { circle_back(i); break; }
        } else if (color[v] == 0) {
            search(v);
        }
    }
    depth--;
    color[u] = 2;
}

static void find_circles(void)
{
    color = calloc((size_t)nv, sizeof *color);
    path = calloc((size_t)nv, sizeof *path);
    if (color && path)
        for (int v = 0; v < nv; v++)
            if (!color[v]) search(v);
    free(color);
    free(path);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: referee manuscript.sheaf\n");
        return 4;
    }
    if (read_source(argv[1]) != 0) {
        fprintf(stderr, "referee: the manuscript did not arrive: %s\n", argv[1]);
        return 4;
    }

    mark = calloc((size_t)(nlines ? nlines : 1), sizeof *mark);
    if (!mark) return 4;

    while (step(NULL, read_line, NULL))
        if (lines_read >= BUDGET) { unfinished = 1; break; }

    /* ---- what was found ---- */

    int any_prose_major = 0, any_prose_minor = 0;
    for (int i = 0; i < nlines; i++) {
        if (!mark[i].prose) continue;
        if (mark[i].op == NOP_) any_prose_minor = 1;
        else any_prose_major = 1;
    }
    char ul[1024], wl[1024], cl[1024], nl[1024], el[1024];
    int nu = line_list(ul, sizeof ul, has_underflow);
    int nw = line_list(wl, sizeof wl, has_w);
    int nc = line_list(cl, sizeof cl, has_c);
    int nn = line_list(nl, sizeof nl, has_n);
    int nx = line_list(el, sizeof el, has_e);
    int open = unfinished ? 0 : sp;

    int any_dangling = 0;
    for (int i = 0; i < nlines; i++)
        if (mark[i].dangling) any_dangling = 1;

    /* lazy lemmas nobody cited: their proofs were never read */
    int unread = 0;
    for (int l = 0; l < nlabels && !unfinished; l++)
        if (labels[l].kind < NLAZY && skip_to[labels[l].line] != NOWHERE &&
            !mark[labels[l].line].called)
            unread++;

    read_citations();
    find_circles();

    int decision = ACCEPT;
    if (any_prose_minor || nn || nw || nc || nx || unread || pieces > 1)
        decision = MINOR;
    if (unfinished || any_prose_major || any_dangling || ncycles || nu || open)
        decision = MAJOR;
    if (claims_false) decision = REJECT;

    const char *name = strrchr(argv[1], '/');
    name = name ? name + 1 : argv[1];
    unsigned long h = fnv(name);

    /* ---- the form ---- */

    char codes[32] = "";
    if (claims_false)                     strcat(codes, " F");
    if (unfinished)                       strcat(codes, " R");
    if (any_prose_major || any_prose_minor) strcat(codes, " P");
    if (any_dangling)                     strcat(codes, " D");
    if (ncycles)                          strcat(codes, " X");
    if (nu)                               strcat(codes, " U");
    if (open)                             strcat(codes, " O");
    if (unread)                           strcat(codes, " L");
    if (pieces > 1)                       strcat(codes, " S");
    if (nx)                               strcat(codes, " E");
    if (nn)                               strcat(codes, " N");
    if (nw)                               strcat(codes, " W");
    if (nc)                               strcat(codes, " C");

    fprintf(stderr, "EDITORIAL OFFICE -- REFEREE REPORT\n");
    fprintf(stderr, "------------------------------------------------------------\n");
    fprintf(stderr, "manuscript        %s\n", name);
    fprintf(stderr, "lines             %d\n", nlines);
    if (unfinished)
        fprintf(stderr, "steps             %ld (reading stopped)\n", steps);
    else if (empty_steps)
        fprintf(stderr, "steps             %ld (%ld of them empty)\n", steps, empty_steps);
    else
        fprintf(stderr, "steps             %ld\n", steps);
    if (unfinished)
        fprintf(stderr, "hypotheses open   not reached\n");
    else
        fprintf(stderr, "hypotheses open   %d\n", open);
    if (claims_checked)
        fprintf(stderr, "claims            %ld checked, %ld false\n",
                claims_checked, claims_false);
    else
        fprintf(stderr, "claims            0 checked\n");
    fprintf(stderr, "citation graph    V %d, E %d, H^0 %d, H^1 %d, chi %d\n",
            nv, ne, pieces, rank1, nv - ne);
    fprintf(stderr, "codes            %s\n", codes[0] ? codes : " none");
    fprintf(stderr, "result            %s\n", FIELD[h % NFIELD]);
    fprintf(stderr, "recommendation    %s\n", DECISION_FORM[decision]);
    fprintf(stderr, "------------------------------------------------------------\n\n");

    /* ---- the prose ---- */

    fprintf(stderr, "Comments to the author\n\n");

    const char *opening = "";
    for (int i = 0; i < nlines; i++) {
        const char *s = lines[i];
        while (isspace((unsigned char)*s)) s++;
        if (*s) { opening = s; break; }
    }
    size_t ol = strlen(opening);
    while (ol > 0 && isspace((unsigned char)opening[ol - 1])) ol--;
    int punct = ol > 0 && strchr(".?!", opening[ol - 1]);
    pf("The manuscript begins: \"%.*s\"%s", (int)ol, opening, punct ? "" : ".");

    if (unfinished) {
        pf(" It takes more than %ld step%s.", steps, steps == 1 ? "" : "s");
    } else {
        char parts[3][64];
        int np = 0;
        char t[32];
        if (observes)  { times(t, sizeof t, observes);  snprintf(parts[np++], 64, "observes %s", t); }
        if (publishes) { times(t, sizeof t, publishes); snprintf(parts[np++], 64, "publishes %s", t); }
        if (leaks) {
            number(t, sizeof t, leaks, 0);
            snprintf(parts[np++], 64, "leaks %s obstruction%s", t, leaks == 1 ? "" : "s");
        }
        pf(" It takes %ld step%s", steps, steps == 1 ? "" : "s");
        if (np == 0)      pf(" and does not publish.");
        else if (np == 1) pf(" and %s.", parts[0]);
        else if (np == 2) pf(", %s and %s.", parts[0], parts[1]);
        else              pf(", %s, %s and %s.", parts[0], parts[1], parts[2]);
    }
    pend("");

    pf("%s", WORN[(h / NFIELD) % NWORN]);
    pend("");

    char buf[128], n[32];

    for (int i = 0; i < nlines; i++)
        if (mark[i].false_claim) {
            pf("Line" NB "%d says %s. %s", i + 1, mark[i].claim, mark[i].denial);
            comment();
        }

    if (unfinished) {
        pf("I read %ld lines and did not reach the end. "
           "Please shorten the manuscript.", lines_read);
        comment();
    }

    for (int i = 0; i < nlines; i++)
        if (mark[i].prose && mark[i].op != NOP_) {
            pf("Line" NB "%d is commentary, and its \"%s\" %s.",
               i + 1, mark[i].word, performs(&mark[i], buf, sizeof buf));
            if (mark[i].sent > 1) {
                times(n, sizeof n, mark[i].sent);
                pf(" It did so %s.", n);
            }
            pf(" Please move it out of the proof.");
            comment();
        }

    for (int i = 0; i < nlines; i++)
        if (mark[i].dangling) {
            pf("Line" NB "%d %s %s. There is no %s.", i + 1,
               mark[i].dangling_op == CALL_ ? "cites" : "sends the reader to",
               mark[i].cite, mark[i].cite);
            comment();
        }

    for (int c = 0; c < ncycles; c++) {
        pf("%s The argument is circular.", cycles[c]);
        comment();
    }

    if (nu) {
        if (nu == 1) pf("Line%s uses a hypothesis that was never introduced.", ul);
        else pf("Lines%s use hypotheses that were never introduced.", ul);
        comment();
    }

    if (open) {
        number(n, sizeof n, open, 1);
        if (open == 1)
            pf("One hypothesis is introduced and not discharged. "
               "It is still open at %s.", halted ? "QED" : "the end");
        else
            pf("%s hypotheses are introduced and not discharged. "
               "They are still open at %s.", n, halted ? "QED" : "the end");
        comment();
    }

    for (int i = 0; i < nlines; i++)
        if (mark[i].prose && mark[i].op == NOP_) {
            pf("Line" NB "%d is commentary, and its \"%s\" %s. Nothing follows from it.",
               i + 1, mark[i].word, performs(&mark[i], buf, sizeof buf));
            comment();
        }

    for (int l = 0; l < nlabels && unread; l++)
        if (labels[l].kind < NLAZY && skip_to[labels[l].line] != NOWHERE &&
            !mark[labels[l].line].called) {
            pf("%s%s%s is never cited, so its proof was not read.",
               KINDS[labels[l].kind], labels[l].id[0] ? NB : "", labels[l].id);
            comment();
        }

    if (pieces > 1) {
        number(n, sizeof n, pieces, 0);
        pf("The citations fall into %s pieces. The paper may be %s papers.", n, n);
        comment();
    }

    if (nx) {
        if (nx == 1) pf("Line%s is left to the reader. Please include it.", el);
        else pf("Lines%s are left to the reader. Please include them.", el);
        comment();
    }

    if (nn) {
        if (nn == 1) pf("Line%s says what needs to be shown. Please show it.", nl);
        else pf("Lines%s say what needs to be shown. Please show it.", nl);
        comment();
    }

    if (nw) {
        if (nw == 1) pf("Line%s is well known. Please give a reference.", wl);
        else pf("Lines%s are well known. Please give references.", wl);
        comment();
    }

    if (nc) {
        if (nc == 1) pf("Line%s says \"clearly\". Please show the step.", cl);
        else pf("Lines%s say \"clearly\". Please show the steps.", cl);
        comment();
    }

    if (comment_no == 0) {
        pf("I have no comments.");
        pend("");
    }

    pf("Recommendation: %s.", DECISION_PROSE[decision]);
    pend("");

    /*
     * Confidential comments to the editor go to file descriptor 3.
     * If the editor has not opened it, they go nowhere.
     */
    if (fcntl(3, F_GETFD) != -1) {
        FILE *editor = fdopen(3, "w");
        if (editor) {
            fprintf(editor, "Confidential comments to the editor, on %s: %s\n",
                    name, CONFIDENTIAL[(h / (NFIELD * NWORN)) % NCONF]);
            fclose(editor);
        }
    }

    free(ea);
    free(eb);
    free(mark);
    free_source();
    return decision;
}
