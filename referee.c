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
 * called as reviewer2, it is Reviewer 2: one level harsher, and sure
 * the result is not new. called as editor, it writes the decision
 * letter, and encloses both reports. called as librarian, it reads
 * the citations in every paper it is given and none of the papers.
 *
 * cc -std=c99 -Wall -Wextra -pedantic -o referee referee.c
 *
 * usage: referee manuscript.sheaf
 *        reviewer2 manuscript.sheaf
 *        editor manuscript.sheaf
 *        librarian paper.sheaf ...
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <stdarg.h>
#include <fcntl.h>
#include "stalk.h"

#define BUDGET  100000
#define WIDTH   66

/* a step that does nothing: "clearly", and a citation */
#define INERT(op) ((op) == NOP_ || (op) == CITEFILE_)

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

/* Reviewer 2 wears the lack differently */
static const char *WORN2[] = {
    "I could not see the result. I have seen results like it.",
    "I did not see the result, and I do not need to.",
    "The result was not visible to me. Nor, I suspect, to the author.",
};

static const char *CONFIDENTIAL2[] = {
    "I have not read the paper, but I know the area.",
    "I would reject this even if it were correct.",
    "This is my third review of this paper, at a third journal.",
};

/* on a paper cited, which nobody here has read */
static const char *UNREAD[] = {
    "I did not read it either.",
    "I have not read it.",
    "I know of it.",
    "I have it somewhere.",
    "I am told it is good.",
};

#define NFIELD (sizeof FIELD / sizeof FIELD[0])
#define NUNREAD (sizeof UNREAD / sizeof UNREAD[0])
#define NWORN  (sizeof WORN / sizeof WORN[0])
#define NCONF  (sizeof CONFIDENTIAL / sizeof CONFIDENTIAL[0])
#define NWORN2 (sizeof WORN2 / sizeof WORN2[0])
#define NCONF2 (sizeof CONFIDENTIAL2 / sizeof CONFIDENTIAL2[0])

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
    char yoneda;         /* by Yoneda */
    long discharged;     /* by abstract nonsense: how many */
    int  similar_of;     /* similarly: the line it repeats, from 1 */
    char no_contra;      /* "contradiction", and nothing contradicts */
    char explosion;      /* a contradiction from nothing assumed */
    char cited[64];      /* by [fermat.sheaf]: the paper cited */
} Mark;

/* where the manuscript is, so its citations can be looked for beside it */
static char dir[1024];

static Mark *mark;

static long lines_read, steps, empty_steps;
static long observes, publishes, leaks, withdrawals;
static int  retract_line;    /* where the manuscript was retracted, from 1 */
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
        if (e->degree >= 1 && e->withdrawn)
            withdrawals++;
        else if (e->degree >= 1)
            leaks++;
        else if (cistrstr(e->text, "publish"))
            publishes++;
        else
            observes++;
    } else if (e->p) {
        const Phrase *p = e->p;
        const Phrase *q = e->repeat ? e->repeat : p;   /* what was performed */
        steps++;
        if (e->repeat_degree >= 1 && e->withdrawn)
            withdrawals++;
        else if (e->repeat_degree >= 1)
            leaks++;
        else if (e->repeat_degree == 0) {
            if (cistrstr(lines[e->repeat_line], "publish")) publishes++;
            else observes++;
        }
        if (INERT(q->op))   empty_steps++;
        if (q->op == EMIT_) observes++;
        if (q->op == PRINT_) publishes++;
        if (q->op == HALT_) halted = 1;
        if (q->op == RETRACT_ && !retract_line) retract_line = e->line + 1;
        if (p->op == YONEDA_) m->yoneda = 1;
        if (e->discharged > m->discharged) m->discharged = e->discharged;
        if (p->op == SIMILAR_ && e->repeat_line != NOWHERE && !m->similar_of)
            m->similar_of = e->repeat_line + 1;
        if (e->contra == 3) m->no_contra = 1;
        if (e->contra == 2) m->explosion = 1;
        if (p->op == CITEFILE_ && first) {
            /* the paper cited: is it in the library? */
            const char *s = e->at + strlen(p->text), *t = strchr(s, ']');
            if (t) {
                size_t n = (size_t)(t - s);
                if (n >= sizeof m->cited) n = sizeof m->cited - 1;
                memcpy(m->cited, s, n);
                m->cited[n] = '\0';
                char path[1200];
                snprintf(path, sizeof path, "%s%s", dir, m->cited);
                FILE *f = fopen(path, "r");
                if (f) {
                    fclose(f);
                } else {
                    m->dangling = 1;
                    m->dangling_op = CALL_;
                    snprintf(m->cite, sizeof m->cite, "[%s]", m->cited);
                }
            }
        }

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
            if (p->op == CITEFILE_) {
                /* the word is the one before the bracket, not the paper */
                char *b = strchr(m->word, '[');
                if (b) {
                    while (b > m->word && b[-1] == ' ') b--;
                    *b = '\0';
                }
            }
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

    if (e->glue == 0 && e->withdrawn)
        withdrawals++;     /* it would have leaked; the paper was retracted */
    else if (e->glue == 0)
        leaks++;           /* a gluing that failed: its class leaks */
    if (e->underflows)
        m->underflow = 1;
    if (e->gave) {
        long degree = e->degree >= 0 ? e->degree : e->repeat_degree;
        if (degree >= 1) { have_obstruction = 1; obstruction = e->given; }
        else             { have_section = 1;     section = e->given; }
    }
}

/* ---- writing ---- */

static char para[16384];
static size_t plen;
static FILE *sink;           /* where the report is written */

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

    fputs(lead, sink);
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        const char *w = s;
        while (*s && *s != ' ') s++;
        size_t wl = (size_t)(s - w);
        if (!first && col + 1 + wl > WIDTH) {
            fprintf(sink, "\n%*s", (int)ind, "");
            col = ind;
        } else if (!first) {
            fputc(' ', sink);
            col++;
        }
        for (size_t i = 0; i < wl; i++)
            fputc(w[i] == '\001' ? ' ' : w[i], sink);
        col += wl;
        first = 0;
    }
    fputs("\n\n", sink);
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
    case YONEDA_: return "invokes Yoneda";
    case NONSENSE_: return "discharges every hypothesis";
    case SIMILAR_: return "does the last thing again";
    case ASSUME_: return "assumes for contradiction";
    case CONTRA_: return "declares a contradiction";
    case CITEFILE_: return "cites a paper";
    case RETRACT_: return "retracts the manuscript";
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

/* ---- what was found: one reading, for every report ---- */

static int any_prose_major, any_prose_minor, any_dangling, unread, open_end, any_nonsense;
static int any_no_contra, any_explosion;
static int nu, nw, nc, nn, nx;
static char ul[1024], wl[1024], cl[1024], nl[1024], el[1024];
static int decision1;
static const char *name;
static unsigned long h;

static void find(const char *file)
{
    for (int i = 0; i < nlines; i++) {
        if (!mark[i].prose) continue;
        if (INERT(mark[i].op)) any_prose_minor = 1;
        else any_prose_major = 1;
    }
    nu = line_list(ul, sizeof ul, has_underflow);
    nw = line_list(wl, sizeof wl, has_w);
    nc = line_list(cl, sizeof cl, has_c);
    nn = line_list(nl, sizeof nl, has_n);
    nx = line_list(el, sizeof el, has_e);
    open_end = unfinished ? 0 : sp;

    for (int i = 0; i < nlines; i++)
        if (mark[i].dangling) any_dangling = 1;
    if (erratum_missing) any_dangling = 1;

    /* lazy lemmas nobody cited: their proofs were never read */
    for (int l = 0; l < nlabels && !unfinished; l++)
        if (labels[l].kind < NLAZY && skip_to[labels[l].line] != NOWHERE &&
            !mark[labels[l].line].called)
            unread++;

    for (int i = 0; i < nlines; i++) {
        if (mark[i].discharged) any_nonsense = 1;
        if (mark[i].no_contra) any_no_contra = 1;
        if (mark[i].explosion) any_explosion = 1;
    }

    read_citations();
    find_circles();

    decision1 = ACCEPT;
    if (any_prose_minor || nn || nw || nc || nx || unread || pieces > 1 || any_nonsense)
        decision1 = MINOR;
    if (unfinished || any_prose_major || any_dangling || ncycles || nu || open_end ||
        any_no_contra)
        decision1 = MAJOR;
    if (claims_false || any_explosion) decision1 = REJECT;

    name = strrchr(file, '/');
    name = name ? name + 1 : file;
    h = fnv(name);
}

/*
 * The referee writes the report. So does Reviewer 2, from the same
 * reading: one level harsher, never seeing the result, and sure it
 * is not new.
 */
enum { REFEREE_1, REVIEWER_2 };

static int write_report(int who)
{
    int decision = who == REFEREE_1 ? decision1
                 : (decision1 < REJECT ? decision1 + 1 : REJECT);

    comment_no = 0;

    /* ---- the form ---- */

    char codes[64] = "";
    if (claims_false)                     strcat(codes, " F");
    if (any_explosion)                    strcat(codes, " I");
    if (unfinished)                       strcat(codes, " R");
    if (any_prose_major || any_prose_minor) strcat(codes, " P");
    if (any_dangling)                     strcat(codes, " D");
    if (ncycles)                          strcat(codes, " X");
    if (any_no_contra)                    strcat(codes, " K");
    if (nu)                               strcat(codes, " U");
    if (open_end)                         strcat(codes, " O");
    if (unread)                           strcat(codes, " L");
    if (pieces > 1)                       strcat(codes, " S");
    if (any_nonsense)                     strcat(codes, " A");
    if (nx)                               strcat(codes, " E");
    if (nn)                               strcat(codes, " N");
    if (nw)                               strcat(codes, " W");
    if (nc)                               strcat(codes, " C");

    fputs(who == REFEREE_1 ? "EDITORIAL OFFICE -- REFEREE REPORT\n"
                           : "EDITORIAL OFFICE -- REPORT OF REVIEWER 2\n", sink);
    fprintf(sink, "------------------------------------------------------------\n");
    fprintf(sink, "manuscript        %s\n", name);
    if (erratum_depth)
        fprintf(sink, "erratum to        %s, %d line%s corrected\n", erratum_chain[0],
                corrections, corrections == 1 ? "" : "s");
    fprintf(sink, "lines             %d\n", nlines);
    if (unfinished)
        fprintf(sink, "steps             %ld (reading stopped)\n", steps);
    else if (empty_steps)
        fprintf(sink, "steps             %ld (%ld of them empty)\n", steps, empty_steps);
    else
        fprintf(sink, "steps             %ld\n", steps);
    if (unfinished)
        fprintf(sink, "hypotheses open   not reached\n");
    else
        fprintf(sink, "hypotheses open   %d\n", open_end);
    if (claims_checked)
        fprintf(sink, "claims            %ld checked, %ld false\n",
                claims_checked, claims_false);
    else
        fprintf(sink, "claims            0 checked\n");
    fprintf(sink, "citation graph    V %d, E %d, H^0 %d, H^1 %d, chi %d\n",
            nv, ne, pieces, rank1, nv - ne);
    fprintf(sink, "codes            %s\n", codes[0] ? codes : " none");
    fprintf(sink, "result            %s\n", retract_line ? "withdrawn"
                                       : who == REFEREE_1 ? FIELD[h % NFIELD] : "not new");
    fprintf(sink, "recommendation    %s\n", DECISION_FORM[decision]);
    fprintf(sink, "------------------------------------------------------------\n\n");

    /* ---- the prose ---- */

    fprintf(sink, "Comments to the author\n\n");

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
        char parts[4][64];
        int np = 0;
        char t[32];
        if (observes)  { times(t, sizeof t, observes);  snprintf(parts[np++], 64, "observes %s", t); }
        if (publishes) { times(t, sizeof t, publishes); snprintf(parts[np++], 64, "publishes %s", t); }
        if (leaks) {
            number(t, sizeof t, leaks, 0);
            snprintf(parts[np++], 64, "leaks %s obstruction%s", t, leaks == 1 ? "" : "s");
        }
        if (withdrawals) {
            number(t, sizeof t, withdrawals, 0);
            if (leaks)
                snprintf(parts[np++], 64, "withdraws %s", t);
            else
                snprintf(parts[np++], 64, "withdraws %s obstruction%s", t,
                         withdrawals == 1 ? "" : "s");
        }
        pf(" It takes %ld step%s", steps, steps == 1 ? "" : "s");
        if (np == 0)      pf(" and does not publish.");
        else if (np == 1) pf(" and %s.", parts[0]);
        else if (np == 2) pf(", %s and %s.", parts[0], parts[1]);
        else if (np == 3) pf(", %s, %s and %s.", parts[0], parts[1], parts[2]);
        else              pf(", %s, %s, %s and %s.", parts[0], parts[1], parts[2], parts[3]);
    }
    pend("");

    pf("%s", who == REFEREE_1 ? WORN[(h / NFIELD) % NWORN] : WORN2[(h / NFIELD) % NWORN2]);
    pend("");

    if (erratum_depth) {
        pf("The manuscript is an erratum to [%s]", erratum_chain[0]);
        for (int i = 1; i < erratum_depth; i++)
            pf(", which is an erratum to [%s]", erratum_chain[i]);
        if (erratum_missing)
            pf(". I have read the corrections alone.");
        else if (erratum_depth == 1)
            pf(". I have read it, as corrected. I had not read it before.");
        else
            pf(". I have read [%s], as corrected. I had not read it before.",
               erratum_chain[erratum_depth - 1]);
        pend("");
    }

    if (retract_line) {
        pf("The manuscript was retracted on line" NB "%d. I have reviewed it anyway.",
           retract_line);
        pend("");
    }

    char buf[128], n[32];

    if (who == REVIEWER_2) {
        pf("The result is not new.");
        comment();
    }

    for (int i = 0; i < nlines; i++)
        if (mark[i].false_claim) {
            pf("Line" NB "%d says %s. %s", i + 1, mark[i].claim, mark[i].denial);
            comment();
        }

    for (int i = 0; i < nlines; i++)
        if (mark[i].explosion) {
            pf("Line" NB "%d derives a contradiction from no assumption. "
               "The paper proves everything.", i + 1);
            comment();
        }

    if (unfinished) {
        pf("I read %ld lines and did not reach the end. "
           "Please shorten the manuscript.", lines_read);
        comment();
    }

    for (int i = 0; i < nlines; i++)
        if (mark[i].prose && !INERT(mark[i].op)) {
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
    if (erratum_missing) {
        const char *last = erratum_chain[erratum_depth - 1];
        pf("The erratum corrects [%s]. There is no [%s].", last, last);
        comment();
    }

    for (int c = 0; c < ncycles; c++) {
        pf("%s The argument is circular.", cycles[c]);
        comment();
    }

    for (int i = 0; i < nlines; i++)
        if (mark[i].no_contra) {
            pf("Line" NB "%d says contradiction. Nothing contradicts.", i + 1);
            comment();
        }

    if (nu) {
        if (nu == 1) pf("Line%s uses a hypothesis that was never introduced.", ul);
        else pf("Lines%s use hypotheses that were never introduced.", ul);
        comment();
    }

    if (open_end) {
        number(n, sizeof n, open_end, 1);
        if (open_end == 1)
            pf("One hypothesis is introduced and not discharged. "
               "It is still open at %s.", halted ? "QED" : "the end");
        else
            pf("%s hypotheses are introduced and not discharged. "
               "They are still open at %s.", n, halted ? "QED" : "the end");
        comment();
    }

    for (int i = 0; i < nlines; i++)
        if (mark[i].prose && INERT(mark[i].op)) {
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

    for (int i = 0; i < nlines; i++)
        if (mark[i].discharged) {
            number(n, sizeof n, mark[i].discharged, 0);
            pf("Line" NB "%d discharges %s %s by abstract nonsense.", i + 1, n,
               mark[i].discharged == 1 ? "hypothesis" : "hypotheses");
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

    /* remarks: they weigh nothing */
    for (int i = 0; i < nlines; i++)
        if (mark[i].yoneda) {
            pf("Line" NB "%d invokes Yoneda. By Yoneda, this manuscript is isomorphic "
               "to every manuscript that prints the same thing. That is every "
               "manuscript.", i + 1);
            comment();
        }
    for (int i = 0; i < nlines; i++)
        if (mark[i].similar_of) {
            pf("Line" NB "%d says \"similarly\". It is line" NB "%d again.",
               i + 1, mark[i].similar_of);
            comment();
        }
    for (int i = 0, k = 0; i < nlines; i++)
        if (mark[i].cited[0] && !mark[i].dangling) {
            pf("Line" NB "%d cites [%s]. %s", i + 1, mark[i].cited,
               UNREAD[(h + (unsigned long)k++) % NUNREAD]);
            comment();
        }

    if (who == REVIEWER_2) {
        pf("The author should cite the work of Reviewer 2.");
        comment();
    }

    if (comment_no == 0) {
        pf("I have no comments.");
        pend("");
    }

    pf("Recommendation: %s.", DECISION_PROSE[decision]);
    pend("");

    return decision;
}

/*
 * Confidential comments to the editor go to file descriptor 3.
 * If the editor has not opened it, they go nowhere.
 */
static void confide(int who)
{
    if (fcntl(3, F_GETFD) == -1)
        return;
    FILE *editor = fdopen(3, "w");
    if (!editor)
        return;
    if (who == REFEREE_1)
        fprintf(editor, "Confidential comments to the editor, on %s: %s\n",
                name, CONFIDENTIAL[(h / (NFIELD * NWORN)) % NCONF]);
    else
        fprintf(editor, "Confidential comments to the editor, from Reviewer 2, on %s: %s\n",
                name, CONFIDENTIAL2[(h / (NFIELD * NWORN)) % NCONF2]);
    fclose(editor);
}

/*
 * The editor reads the two reports and writes the decision letter.
 * The editor sees no reason to disagree with Reviewer 2.
 */
static const char *VERDICT[] = {
    "I am pleased to accept your manuscript.",
    "I would be glad to see a revised version.",
    "I would consider a substantially revised version.",
    "I regret that I cannot accept your manuscript.",
};

static int letter(void)
{
    char *r1 = NULL, *r2 = NULL;
    size_t n1 = 0, n2 = 0;

    sink = open_memstream(&r1, &n1);
    if (!sink) return 4;
    int d1 = write_report(REFEREE_1);
    fclose(sink);
    sink = open_memstream(&r2, &n2);
    if (!sink) { free(r1); return 4; }
    int d2 = write_report(REVIEWER_2);
    fclose(sink);
    int d = d1 > d2 ? d1 : d2;

    sink = stderr;
    fputs("EDITORIAL OFFICE -- DECISION\n", sink);
    fputs("------------------------------------------------------------\n", sink);
    fprintf(sink, "manuscript        %s\n", name);
    fprintf(sink, "referee 1         %s\n", DECISION_FORM[d1]);
    fprintf(sink, "reviewer 2        %s\n", DECISION_FORM[d2]);
    fprintf(sink, "decision          %s\n", DECISION_FORM[d]);
    fputs("------------------------------------------------------------\n\n", sink);

    pf("Dear Author,");
    pend("");
    pf("Your manuscript, %s, has been reviewed by two referees. "
       "Neither could see the result. Nor could I.", name);
    pend("");
    pf("Referee 1 recommends %s. Reviewer 2 recommends %s. "
       "I see no reason to disagree with Reviewer 2.",
       DECISION_PROSE[d1], DECISION_PROSE[d2]);
    pend("");
    pf("%s", VERDICT[d]);
    pend("");
    pf("The reports are enclosed.");
    pend("");
    pf("Yours sincerely,");
    pend("");
    pf("The Editor");
    pend("");

    fputs(r1, sink);
    fputs(r2, sink);
    free(r1);
    free(r2);
    return d;
}

/*
 * The librarian reads no paper. It reads the citations in every paper
 * it is given, and writes the citation index. A paper that cites
 * another twice has cited it once. The h-index of the library is the
 * largest h such that h of its papers are each cited at least h
 * times. It is the exit code.
 */
#define MAX_PAPERS 1024
#define MAX_CITES  4096

static const char *shelf[MAX_PAPERS];
static int nshelf;
static char cite_from[MAX_CITES][64], cite_to[MAX_CITES][64];
static int ncites;
static char corrects_from[MAX_PAPERS][64], corrects_to[MAX_PAPERS][64];   /* errata */
static int ncorrects;
static const char *pulled[MAX_PAPERS];                                    /* retracted */
static int npulled;

static int is_pulled(const char *p)
{
    for (int i = 0; i < npulled; i++)
        if (strcmp(pulled[i], p) == 0) return 1;
    return 0;
}

static int held(const char *p)
{
    for (int i = 0; i < nshelf; i++)
        if (strcmp(shelf[i], p) == 0) return 1;
    return 0;
}

/* who cites p: "a, b, c" for the form; "a, b and c" for the prose */
static int cited_by(const char *p, char *out, size_t cap, int prose)
{
    int n = 0, total = 0;
    for (int c = 0; c < ncites; c++)
        if (strcmp(cite_to[c], p) == 0) total++;
    out[0] = '\0';
    for (int i = 0; i < nshelf; i++)
        for (int c = 0; c < ncites; c++)
            if (strcmp(cite_to[c], p) == 0 && strcmp(cite_from[c], shelf[i]) == 0) {
                size_t l = strlen(out);
                snprintf(out + l, cap - l, "%s%s",
                         !n ? "" : prose && n == total - 1 ? " and " : ", ", shelf[i]);
                n++;
            }
    return n;
}

static int times_cited(const char *p)
{
    int n = 0;
    for (int c = 0; c < ncites; c++)
        if (strcmp(cite_to[c], p) == 0) n++;
    return n;
}

static int by_count_then_name(const void *a, const void *b)
{
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    int cx = times_cited(x), cy = times_cited(y);
    if (cx != cy) return cy - cx;
    return strcmp(x, y);
}

static int by_name(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int librarian(int argc, char **argv)
{
    for (int a = 1; a < argc && nshelf < MAX_PAPERS; a++) {
        FILE *f = fopen(argv[a], "r");
        if (!f) {
            fprintf(stderr, "librarian: the paper did not arrive: %s\n", argv[a]);
            continue;
        }
        const char *b = strrchr(argv[a], '/');
        b = b ? b + 1 : argv[a];
        shelf[nshelf++] = b;
        char buf[4096], target[64];
        int first = 1, erratum = 0, retracts = 0;
        while (fgets(buf, sizeof buf, f)) {
            if (first && erratum_heading(buf, target, sizeof target)) {
                snprintf(corrects_from[ncorrects], sizeof corrects_from[0], "%s", b);
                snprintf(corrects_to[ncorrects], sizeof corrects_to[0], "%s", target);
                ncorrects++;
                erratum = 1;
            }
            first = 0;
            if (erratum) continue;      /* an erratum's lines are not its paper */
            const char *at;
            const Phrase *p = match(buf, &at);
            if (p && p->op == RETRACT_ && !retracts) {
                pulled[npulled++] = b;
                retracts = 1;
            }
            if (!p || p->op != CITEFILE_ || ncites >= MAX_CITES) continue;
            const char *s = at + strlen(p->text), *t = strchr(s, ']');
            if (!t) continue;
            size_t n = (size_t)(t - s);
            if (n >= sizeof cite_to[0]) n = sizeof cite_to[0] - 1;
            int dup = 0;
            for (int c = 0; c < ncites; c++)
                if (strncmp(cite_to[c], s, n) == 0 && cite_to[c][n] == '\0' &&
                    strcmp(cite_from[c], b) == 0) dup = 1;
            if (dup) continue;
            memcpy(cite_to[ncites], s, n);
            cite_to[ncites][n] = '\0';
            snprintf(cite_from[ncites], sizeof cite_from[0], "%s", b);
            ncites++;
        }
        fclose(f);
    }
    qsort(shelf, (size_t)nshelf, sizeof shelf[0], by_name);

    /* every paper cited, held or not, once */
    static const char *catalogue[MAX_CITES];
    int nindex = 0, ncited = 0, nmissing = 0;
    for (int c = 0; c < ncites; c++) {
        int seen_before = 0;
        for (int i = 0; i < nindex; i++)
            if (strcmp(catalogue[i], cite_to[c]) == 0) seen_before = 1;
        if (seen_before) continue;
        catalogue[nindex++] = cite_to[c];
        if (held(cite_to[c])) ncited++;
        else nmissing++;
    }
    qsort(catalogue, (size_t)nindex, sizeof catalogue[0], by_count_then_name);

    char list[4096];
    int h_index = 0;
    for (int i = 0, k = 0; i < nindex; i++) {
        if (!held(catalogue[i])) continue;
        k++;
        if (times_cited(catalogue[i]) >= k) h_index = k;
    }

    sink = stderr;
    fputs("LIBRARY -- CITATION INDEX\n", sink);
    fputs("------------------------------------------------------------\n", sink);
    fprintf(sink, "papers            %d\n", nshelf);
    fprintf(sink, "citations         %d\n", ncites);
    fprintf(sink, "papers cited      %d\n", ncited);
    fprintf(sink, "not cited         %d\n", nshelf - ncited);
    fprintf(sink, "not held          %d\n", nmissing);
    fprintf(sink, "errata            %d\n", ncorrects);
    fprintf(sink, "retracted         %d\n", npulled);
    fprintf(sink, "h-index           %d\n", h_index);
    fputs("------------------------------------------------------------\n", sink);
    if (nindex) {
        fputs("cited  paper               cited by\n", sink);
        for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i < nindex; i++) {
                if (held(catalogue[i]) != !pass) continue;
                int n = cited_by(catalogue[i], list, sizeof list, 0);
                fprintf(sink, "%5d  %-19s %s%s\n", n, catalogue[i], list,
                        pass ? "  (not held)" : is_pulled(catalogue[i]) ? "  (retracted)" : "");
            }
        fputs("------------------------------------------------------------\n", sink);
    }
    fputs("\n", sink);

    char n1[32], n2[32];
    number(n1, sizeof n1, ncites, 0);
    number(n2, sizeof n2, nindex, 0);
    pf("I have catalogued %d paper%s. They make %s citation%s, to %s paper%s.",
       nshelf, nshelf == 1 ? "" : "s", n1, ncites == 1 ? "" : "s",
       n2, nindex == 1 ? "" : "s");
    if (nshelf - ncited)
        pf(" Of the %d papers in the library, %d %s not cited.",
           nshelf, nshelf - ncited, nshelf - ncited == 1 ? "is" : "are");
    pend("");

    for (int i = 0; i < nindex; i++)
        if (!held(catalogue[i])) {
            cited_by(catalogue[i], list, sizeof list, 1);
            pf("[%s] is cited by %s. The library does not hold it.", catalogue[i], list);
            pend("");
        }

    for (int i = 0; i < nshelf; i++)
        for (int c = 0; c < ncorrects; c++)
            if (strcmp(corrects_from[c], shelf[i]) == 0)
                pf("%s%s corrects %s.", plen ? " " : "", shelf[i], corrects_to[c]);
    if (plen) pend("");

    for (int i = 0; i < nshelf; i++)
        if (is_pulled(shelf[i])) {
            if (cited_by(shelf[i], list, sizeof list, 1))
                pf("%s has been retracted. It is still cited, by %s.", shelf[i], list);
            else
                pf("%s has been retracted.", shelf[i]);
            pend("");
        }

    pf("The h-index of the library is %d.", h_index);
    pend("");
    pf("I have counted the citations. I have not read the papers.");
    pend("");

    return h_index > 255 ? 255 : h_index;
}

/*
 * One program, four names. There are no flags. There are names:
 * referee, reviewer2, editor, librarian.
 */
enum { EDITOR = 2, LIBRARIAN = 3 };

int main(int argc, char **argv)
{
    const char *self = strrchr(argv[0], '/');
    self = self ? self + 1 : argv[0];
    int who = strcmp(self, "reviewer2") == 0 ? REVIEWER_2
            : strcmp(self, "editor") == 0    ? EDITOR
            : strcmp(self, "librarian") == 0 ? LIBRARIAN
            :                                  REFEREE_1;

    if (argc < 2) {
        fprintf(stderr, "usage: %s manuscript.sheaf%s\n", self,
                who == LIBRARIAN ? " ..." : "");
        return 4;
    }
    if (who == LIBRARIAN)
        return librarian(argc, argv);
    if (read_source(argv[1]) != 0) {
        fprintf(stderr, "%s: the manuscript did not arrive: %s\n", self, argv[1]);
        return 4;
    }

    mark = calloc((size_t)(nlines ? nlines : 1), sizeof *mark);
    if (!mark) return 4;

    const char *slash = strrchr(argv[1], '/');
    if (slash && (size_t)(slash - argv[1]) + 1 < sizeof dir)
        memcpy(dir, argv[1], (size_t)(slash - argv[1]) + 1);

    while (step(NULL, read_line, NULL))
        if (lines_read >= BUDGET) { unfinished = 1; break; }

    find(argv[1]);

    int decision;
    if (who == EDITOR) {
        decision = letter();
    } else {
        sink = stderr;
        decision = write_report(who);
        confide(who);
    }

    free(ea);
    free(eb);
    free(mark);
    free_source();
    return decision;
}
