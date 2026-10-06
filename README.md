# sheaf

The local data is perfect. The stalks are correct.
Gamma is not implemented.

---

## Overview

sheaf is a purely functional programming language. Haskell thinks it's
functional. Haskell has IO. sheaf does not have IO. sheaf is functional.

The computation runs. Arithmetic works. Memory works. Control flow works.
Output does not work. Output is not a bug. Output is not implemented.
Output would require the global sections functor. The global sections
functor is not implemented. The sheaf has no global sections.

The program terminates. Exit code 0. The computation was correct.
The correctness is unobservable.

## Quick Start

```
make
./sheaf examples/functorial.sheaf
./referee examples/fermat.sheaf
./librarian examples/*.sheaf
./sheafc examples/les.sheaf
```

## Instructions

Phrase matching is case-insensitive, leftmost-wins. Unrecognized
words on the same line are ignored. The English is the paper.
The computation is the proof.

| Phrase | Abbrev | Instruction | Effect |
|--------|--------|-------------|--------|
| `suppose N` | | PUSH N | Introduce a hypothesis. |
| `trivially` | | PUSH 0 | The trivial case. |
| `without loss of generality` | `WLOG` | POP | Discard. It doesn't matter. |
| `recall` | `resp.` | DUP | Recall what we just established. |
| `by duality` | `op.` | SWAP | The dual statement. |
| `direct sum` | | ADD | Combine. |
| `restrict` | | SUB | Restrict to a smaller space. |
| `tensor` | | MUL | Tensor product. |
| `localize` | | DIV | Localize at a prime. |
| `pullback` | | LOAD | Pull back from the base. |
| `pushforward` | | STORE | Push forward to the base. |
| `observe` | | EMIT | Pop. Discard. Gamma is not implemented. |
| `publish` | | PRINT | Pop. Discard. The referee is silent. |
| `R^i observe` | | H^i | Right derived functor. See below. |
| `R^i publish` | | H^i | Right derived functor. See below. |
| `see N` | `cf. N` | JMP N | See proposition N. |
| `vacuously N` | `iff N` | JZ N | Vacuously true. The premise is zero. |
| `nontrivially N` | | JNZ N | The value is nontrivial. |
| `by lemma N` | `by proposition N`, `by corollary N`, `by claim N`, `by theorem N` | CALL | Cite it. Go there, and come back. |
| `this proves the` | | RET | Come back. |
| `by induction` | | LOOP | Begin an induction. |
| `this completes the induction` | | REPEAT | Begin again, while the top of the stack is nonzero. |
| `cover` | | COVER | A new cover, by the opens named on the line: U1, U2, ... |
| `the transition` | | TRANS | The transition on two opens: an integer. |
| `by gluing` | | GLUE | 1 if the transitions are a coboundary. Otherwise 0, and the class leaks. |
| `left to the reader` | | READ | The reader gives a number. Or nothing. |
| `by yoneda` | | NOP | An object is its observations. None are kept. |
| `by abstract nonsense` | | CLEAR | Every open hypothesis, discharged at once. |
| `similarly` | | AGAIN | The last line that performed, performed again. |
| `assume for contradiction` | | ASSUME | Everything from here is derived from what is assumed. |
| `contradiction` | | CONTRA | Nonzero: the assumption is discarded, and its negation, 1, is proved. |
| `by [paper]` | | CITE | Another paper is cited. It is not read. |
| `QED` | | HALT | The proof is complete. |
| `clearly` | | NOP | Obviously. Contributes nothing. |
| `it is well known` | `TFAE` | NOP | Everyone knows this. Nobody cites it. |
| | `NTS` | NOP | Need to show. (Does not show.) |
| | `WTS` | NOP | Want to show. (Does not show.) |
| | `RTP` | NOP | Required to prove. (Does not prove.) |
| | `s.t.` | NOP | Such that. (Such what?) |

## Examples

```
Theorem. The construction is functorial.

Proof. Clearly.

QED.
```

```
Suppose 6.
Suppose 7.
Tensor.
Publish.
QED.
```

See `examples/` for 35 programs, including the seminar, the referee,
the thesis defense, the arXiv preprint, the geometric Langlands
conjecture, sheaf cohomology, vanishing theorems, Grothendieck
vanishing, the long exact sequence, Gauss at seven, Euclid read as a
descent, Collatz from 27, the circle by three arcs and by two, a
lemma used twice, the reader's square, a stopwatch, the only quine,
Yoneda, abstract nonsense, a second case that is not similar, a
proof by contradiction, a proof of everything, a survey, and a
follow-up to the survey. `submissions/` holds three more. They do not
terminate. Only the referee reads them.

## Derived Functors

`R^i observe` and `R^i publish` compute the right derived functors
of Gamma. R^0 = Gamma (pop, discard -- not implemented, as before).
R^i for i >= 1: the obstruction leaks to stderr.

```
Suppose 6. Suppose 7. Tensor.
R^1 observe.
```

stdout: (nothing). stderr: `H^1(X,F) = 42`.

H^i(X,F) measures the obstruction to having global sections. The
reason you cannot see the answer is itself observable -- on stderr.
stderr is the derived category.

```
Trivially.
R^1 observe.
```

stderr: `H^1(X,F) = 0`. The obstruction vanishes. The sheaf is
acyclic. But Gamma is still not implemented. You proved the
obstruction is zero. The answer is still invisible.

The degree i is any natural number. `R^10 observe` computes H^10.

## Proof Technique

A heading names a line. It begins the line: `Lemma 2.3.`,
`Proposition 4.`, `Corollary 1.`, `Claim 2.`, `Theorem 1.`, `Case 2.`

`By Lemma 2.3, ...` cites it. The reader goes to Lemma 2.3, reads its
proof, and comes back at `This proves the lemma.`

Lemmas are lazy. So are propositions, corollaries and claims. A
reader who arrives at a lemma without citing it skips to the end of
its proof. A lemma is proved when it is cited, and not before.
Theorems and cases are read where they stand.

`By induction.` begins an induction. `This completes the induction.`
begins it again, while the top of the stack is nonzero.

```
Suppose 100.
By induction.
Recall.
Suppose 0.
Pullback.
Direct sum.
Suppose 0.
By duality.
Pushforward.
Suppose 1.
Restrict.
This completes the induction.
```

That is the induction in `examples/gauss.sheaf`. Gauss put his slate
on the teacher's desk and said "Ligget se": there it lies.

`see`, `vacuously` and `nontrivially` take a heading as well as a
number: `Vacuously Case 1.` A number counts lines from zero. `see 5`
sends the reader to line 6.

A dilemma is not a lemma. Headings begin the line.

## Abstract Nonsense

`By Yoneda.` An object is determined by its observations. Every
observation of a sheaf program is discarded. By Yoneda, every sheaf
program is isomorphic to every other. The referee says so, and it
weighs nothing.

`By abstract nonsense.` discharges every open hypothesis at once. The
stack is cleared, and there is nothing left for the referee to find
open. It finds the clearing instead: "Line 8 discharges two
hypotheses by abstract nonsense."

Abstract nonsense is the least nonsense there is.

`Similarly.` performs the last line that performed, again. It is
rarely what was meant. "Similarly, the second case." after a
`Tensor.` multiplies by a hypothesis that was never introduced. The
referee: "It is line 7 again."

## Proof by Contradiction

`Assume for contradiction.` Everything from here is derived from what
is assumed. `Contradiction.` takes the top of the stack: the
disagreement between what was assumed and what was derived. If it is
not zero, the contradiction is reached. What was derived is
discarded, and 1 is pushed: the negation is proved.
`examples/contradiction.sheaf` proves that seven is odd. It assumes
the remainder of seven by two is zero, computes it, and the two
disagree.

If the top is zero, nothing contradicts. "Line 5 says contradiction.
Nothing contradicts."

A contradiction derived from no assumption is not a proof by
contradiction. Everything follows. "Line 6 derives a contradiction
from no assumption. The paper proves everything." The referee rejects
it. `examples/everything.sheaf`.

"Suppose for contradiction" is a false friend. `suppose` is leftmost,
and with no number on the line it supposes 0.

## The Literature

```
By [fermat.sheaf], no three positive integers satisfy a^3 + b^3 = c^3.
```

`By [paper]` cites another paper. It does nothing. A citation is not
a reading.

The referee looks for the paper beside the manuscript. If it is
there, the referee remarks on it, and the remark weighs nothing:

```
1. Line 3 cites [fermat.sheaf]. I am told it is good.
```

`fermat.sheaf` is rejected. If the paper is not there, the citation
is to nothing: "Line 6 cites [in preparation]. There is no [in
preparation]."

`examples/survey.sheaf` cites four papers and adds nothing. It says
its value is zero, the referee checks that it is, and the survey is
accepted. One of the four results it surveys is the false theorem of
`twoarcs.sheaf`. `examples/followup.sheaf` cites the survey, two of
the papers the survey cites, and a paper in preparation.

A citation inside a sentence is commentary, and does nothing, as
"clearly" does nothing: "Line 2 is commentary, and its "by" cites a
paper. Nothing follows from it."

```
./librarian examples/*.sheaf
```

The referee has a fourth name. `librarian` reads every citation in
the papers it is given, and none of the papers, and writes the
citation index:

```
LIBRARY -- CITATION INDEX
------------------------------------------------------------
papers            35
citations         8
papers cited      5
not cited         30
not held          1
h-index           2
------------------------------------------------------------
cited  paper               cited by
    2  fermat.sheaf        followup.sheaf, survey.sheaf
    2  gauss.sheaf         followup.sheaf, survey.sheaf
    1  collatz.sheaf       survey.sheaf
    1  survey.sheaf        followup.sheaf
    1  twoarcs.sheaf       survey.sheaf
    1  in preparation      followup.sheaf  (not held)
------------------------------------------------------------
```

It ends: "I have counted the citations. I have not read the papers."

A paper that cites another twice has cited it once. The h-index of
the library is the largest h such that h of its papers are cited at
least h times each. It is the exit code. The shell reads any exit
code but 0 as failure, so a library anyone cites fails, and a library
nobody cites succeeds.

Real papers write "cf. [12]". In sheaf `cf.` is a jump, and the
number is a line: "cf. [12]" sends the reader to line 13. In a short
paper that is past the end, and QED is not reached. "By [12]" cites a
paper called 12. There is no [12].

## Gluing

```
Cover the circle by U1, U2 and U3.
The transition on U1 and U2 is 0.
The transition on U2 and U3 is 0.
The transition on U3 and U1 is 1.
By gluing.
```

The opens are U1, U2, U3. Each transition is an integer on an
overlap: a Cech 1-cochain on the nerve. On each arc of the circle the
logarithm has a branch, and going round, the branches disagree by one
turn.

`By gluing.` asks whether the cochain is a coboundary. A spanning tree
of the nerve fixes the local sections; each overlap outside the tree
closes a cycle, and the cochain is summed around it. The sums are the
class in H^1(U, Z) = Z^b1.

If every sum is zero, the sections glue. There is a global section.
Gamma is not implemented. 1 is pushed.

If not, 0 is pushed and the class leaks:

```
H^1(U,Z) = Z^1; the class is (1)
```

That is the winding number. `examples/circle.sheaf`.

Two arcs also cover the circle, and their overlap is two pieces. State
one transition for it and the nerve is a single edge. Every cochain on
an edge is a coboundary. The sections glue, and with two arcs the
circle has no hole. State a transition on each piece and the hole is
back. `examples/twoarcs.sheaf` does both. Three arcs are the fewest
that see the hole without being told.

The nerve is a graph. Triple overlaps are not seen.

## The Reader

`Left to the reader.` reads one number from stdin. If the reader gives
nothing, nothing is pushed. Nothing is not 0.

```
echo 12 | ./sheaf examples/square.sheaf
```

The square is 144. The reader knows 12. Nobody knows 144.

sheaf has I. It does not have O. Haskell has IO. sheaf has the half
that listens.

`examples/exercise.sheaf` has always begun "Exercise 3.7.12. Left to
the reader." It now waits for the reader. The tests give the reader
nothing.

The referee does not do exercises. It reads as if the reader gave
nothing, and says what a referee says: "Line 1 is left to the reader.
Please include it." `examples/square.sheaf` uses what the reader
gives, so to the referee it uses hypotheses that were never
introduced.

## The REL

`sheaf` with no manuscript is a REL. Read, eval, loop. There is no
print. There is no prompt either. A prompt would be output.

Type a proof. Each line is evaluated as it arrives. `QED.` ends it.
In the REL the reader is the typist: `Left to the reader.` takes the
next number typed.

## The Compiler

```
./sheafc examples/les.sheaf > les.c
```

sheafc is an optimizing compiler. It evaluates the proof at compile
time, all of it: constant folding, all the way down. Then it keeps
what is observable. By the as-if rule (C11 5.1.2.3) a translation
need preserve only observable behaviour, and stdout is empty, so
nothing is kept.

```
int main(void)
{
    return 0;
}
```

The obstructions are on stderr, which is observable. They are kept.

A loop that does nothing observable may be assumed to terminate (C11
6.8.5p6). sheafc assumes it. `submissions/seems.sheaf` does not
terminate. Compiled, it does. So does circular reasoning.

A proof that asks the reader cannot be folded. "The reader is not a
constant."

sheafc is the only part of sheaf with output. Its output is a program
with none.

## Quines

A quine prints its own source. Every sheaf program prints nothing, so
a sheaf program is a quine exactly when its source is nothing.
`examples/quine.sheaf` is the empty file. It is the only quine. A file
holding one newline prints nothing, and is not one.

The referee accepts it. "The manuscript begins: ""."

## The Referee

```
./referee examples/fermat.sheaf
```

The referee is a second program. It is not the author. It reads the
manuscript while the proof runs beside it, one line at a time, and
writes a report. The report goes to stderr. The recommendation is the
exit code: 0 accept, 1 minor revision, 2 major revision, 3 reject.

```
EDITORIAL OFFICE -- REFEREE REPORT
------------------------------------------------------------
manuscript        fermat.sheaf
lines             30
steps             20 (2 of them empty)
hypotheses open   3
claims            1 checked, 1 false
citation graph    V 1, E 0, H^0 1, H^1 0, chi 1
codes             F O W C
result            see attached
recommendation    REJECT
------------------------------------------------------------

Comments to the author

The manuscript begins: "It is well known that for n > 2, no three
positive integers". It takes 20 steps and publishes once.

The result did not reach me. I have checked what I could.

1. Line 28 says this is zero. It is not zero.

2. Three hypotheses are introduced and not discharged. They are
   still open at QED.

3. Line 1 is well known. Please give a reference.

4. Line 28 says "clearly". Please show the step.

Recommendation: reject.
```

Nothing is attached.

The codes:

| Code | Finding | Weight |
|------|---------|--------|
| F | A claim is false: "the sum is 12", "this is zero", "the stack contains 4". | reject |
| I | A contradiction from no assumption. The paper proves everything. | reject |
| R | The referee read 100000 lines and did not reach the end. | major |
| P | A phrase performs from inside a sentence. | major; minor if it does nothing |
| D | A citation names a heading, or a paper, that is not there. | major |
| X | The citations go round. The argument is circular. | major |
| K | It says contradiction. Nothing contradicts. | major |
| U | A hypothesis is used that was never introduced. The stack was empty. | major |
| O | Hypotheses are still open at QED. | major |
| L | A lemma is never cited, so its proof was not read. | minor |
| S | The citations fall into pieces. The paper may be several papers. | minor |
| A | Hypotheses are discharged by abstract nonsense. | minor |
| E | It is left to the reader. Please include it. | minor |
| N | `NTS`, `WTS` or `RTP`. The need is stated. | minor |
| W | It is well known. No reference is given. | minor |
| C | Clearly. | minor |

A phrase is an instruction when it is a capitalized whole word at the
start of a sentence. Anywhere else it is commentary. Commentary
performs anyway.

Confidential comments to the editor go to file descriptor 3. If the
editor has not opened it, they go nowhere.

```
./referee examples/fermat.sheaf 3>&1 2>/dev/null
Confidential comments to the editor, on fermat.sheaf: This took me four minutes.
```

The referee is one program with four names. Called `reviewer2`, it
is Reviewer 2. It reads the same paper, from the same run, and is
one level harsher. Its form says the result is "not new". Its first
comment is "The result is not new." Its last is "The author should
cite the work of Reviewer 2." It cannot see the result either. "I
have seen results like it."

Called `editor`, it writes the decision letter and encloses both
reports:

```
Referee 1 recommends accept. Reviewer 2 recommends minor revision.
I see no reason to disagree with Reviewer 2.
```

That is `examples/twoarcs.sheaf`, whose theorem is false. Called
`librarian`, it reads the citations and not the papers (see The
Literature). There are no flags. There are names.

A claim is checked against the stack as the line is reached, or, if
the stack is empty, against the last value to leave it. A claim about
the obstruction is checked against the last obstruction to leak. The
referee says whether a claim holds. It does not say what holds.

Of the 35 examples, 16 are accepted, 8 need minor revision, 8 need
major revision, and 3 are rejected. Among the accepted: "With two
arcs, the logarithm has a global branch on the circle." The referee
sees the nerve, as the interpreter does.

The referee confirms that the sum of the first hundred numbers is
5050, that the greatest common divisor of 1071 and 462 is 21, and
that 27 reaches 1 in 111 steps. It saw none of these numbers. Of
`collatz.sheaf` it asks for a reference for "It is well known that
this terminates."

## The Cohomology of the Proof

The referee reads the citations from the whole text, not from the
run, as a referee reads. The citation graph has a vertex for the text
and one for each lemma, proposition, corollary and claim, and an edge
for each citation, from the proof it sits in to what it cites. A
theorem belongs to the text.

```
citation graph    V 3, E 3, H^0 1, H^1 1, chi 0
```

H^0 counts the pieces. If there are two, the paper may be two papers.
H^1 counts the independent cycles. chi = V - E = H^0 - H^1.

H^1 is not circularity. `examples/twice.sheaf` cites one lemma from
two places, and `submissions/circular.sheaf` has two lemmas that cite
each other. Both read V 3, E 3, H^0 1, H^1 1, chi 0. The first is
accepted. The second does not terminate, and the referee says why:

```
Lemma 1 cites Lemma 2, which cites Lemma 1. The argument is
circular.
```

The cohomology does not see the difference. The direction of the
citations does. The referee follows each citation forward until one
comes back.

## False Friends

The English is live. A phrase performs wherever it appears, inside any
word, if it is the leftmost phrase on its line. Some ordinary words
are instructions.

| Word | Contains | Does |
|------|----------|------|
| seems, seen, seed, seek, foresee | `see` | Sends the reader to line 1, unless a number follows on the line. |
| differ, different, difficult, differential, diffeomorphism | `iff` | If the stack is empty or zero, the same. |
| observed, observer, observes | `observe` | Pops. |
| published, publishes, publisher | `publish` | Pops. |
| supposed, supposedly, presuppose | `suppose` | Pushes the next number on the line, or 0. |
| restricted, restriction | `restrict` | Subtracts. |
| tensors, tensored | `tensor` | Multiplies. |
| localized, localizes | `localize` | Divides. |
| recalled, recalls | `recall` | Duplicates. |
| corresp. | `resp.` | Duplicates. |
| stop. loop. top. develop. | `op.` | Swaps. |
| comments, elements, points, coefficients | `NTS` | Nothing. It needs to show. |
| smartphone | `RTP` | Nothing. |
| the nearby lemmas | `by lemma` | Cites a lemma that is not there. |
| and this proves the point | `this proves the` | Comes back from a lemma. |
| discover, recovered, covered | `cover` | Declares a cover, by whatever opens the line names. |
| the transitions | `the transition` | Sets a transition. |
| cf. [12] | `cf.` | Sends the reader to line 13. |

Observation is safe. Publication is safe. Supposition is safe. A
dilemma is safe. A phase transition is safe.

"The proof seems correct." does not terminate. It sends the reader
back to the beginning, and the beginning leads back to it.
`submissions/seems.sheaf` is that proof. The program terminates,
unless it seems to.

`submissions/diffeomorphism.sheaf` does not get past its title.

## Side Channels

sheaf satisfies noninterference. Nothing a program knows reaches
stdout, because nothing reaches stdout. The guarantee is trivial, and
it is complete.

Channels remain. stderr was opened on purpose: the obstruction leaks
there. Another is time.

The referee never says what the result is. Its form says how many
steps the result took.

```
steps             134
```

That is `examples/stopwatch.sheaf`. It hides 42 on the stack and
counts down from it. Eight steps are fixed, and each time round the
induction takes three: 134 = 8 + 3 x 42. Anyone with the source and
the form has the answer. `examples/gauss.sheaf` takes 7 + 10n steps.
Its form says 1007, so n is 100, and the sum was 5050.

The referee also says whether a claim holds. That is one bit. Add
"The result is 41." before the `Publish.` of a proof of 42 and the
referee rejects it; "The result is 42." and it accepts. It will not
tell you the answer. It will tell you whether you have guessed it.

The referee keeps counting. It is a clerk.

## The Fixed Point

```
make fixpoint
```

A report is prose, so a report is a manuscript. `make fixpoint` gives
each manuscript's report to the referee, and that report to the
referee, until a report comes round again. Where each review ends is
kept in `fixpoint/`.

Of the 38 reviews, 21 reach a fixed point by the third round. The
fixed points are reports that could not finish reading themselves:
"I read 100000 lines and did not reach the end." There are 8 of
them. All 21 recommend major revision.

In 19 of the 21, the reader was sent back by the words the referee
used to say it could not see: "see attached", "unable to see", "I
could not see the result", "I have not seen the result". In the
other 2, by the manuscript's own false friend, quoted.

Of the other 17, 15 do not come round in 16 rounds. Their referee
wore the lack in other words: "could not tell", "pending", "did not
reach me". Each report says the manuscript publishes, or observes.
Each "publishes" publishes, each "observes" observes, and each has to
be reported. Most reports grow by 6 lines a round. The review of
`yoneda.sheaf` grows by 9: its remark says every manuscript "prints"
the same thing, and "prints" contains NTS.

The last 2 explode. A line that says "contradiction" declares one.
The report on `everything.sheaf` says the paper "derives a
contradiction"; the report on `contradiction.sheaf` names its
manuscript, `contradiction.sheaf`. Every comment the referee writes
about a contradiction says the word again: twice in the comment that
quotes it, once in "Nothing contradicts." The lines that say
contradiction go 1, 3, 9, and triple each round until the reader
stops at 65,536 lines. Ex falso quodlibet, in review.

## Tests

```
make test
```

There are eight tests.

The vacuous test checks that every program's stdout is empty. Every
program's stdout is empty. The test has never failed. It cannot fail.
It is kept for its honesty.

The derived test checks each program's stderr against
`examples/NAME.derived`, and checks that it is empty where there is no
such file. stdout carries nothing, so stderr is the only witness. The
obstruction is checked. The answer is not.

The report test checks each manuscript's report against `NAME.report`,
and the exit code against its recommendation. The rejections are
kept. A rejected manuscript that is corrected will no longer be
rejected, and the test will fail.

The letters test checks the editor's letter, with both reports
enclosed, against `NAME.letter`, where there is one.

The library test checks the librarian's index of every example
against `examples/library.index`, and the exit code against the
h-index on it.

The compiled test folds each manuscript with sheafc, compiles the C,
runs it, and checks that it leaks what the interpreter leaks. The
manuscripts that do not terminate pass. Compiled, they terminate.

The REL test types each manuscript into the REL and checks the same.

The quine test checks that every quine is empty, and every empty file
a quine.

## The Site

https://theresultisonthenext.page is `docs/`. It runs sheaf, the
referee and the librarian in the browser, from a port of the C. `make site` bundles the examples into the page. `make
site-test` checks the port against every report, every derived file
and the library index, byte for byte. Both need node.

The desk marks the words that act as they are typed. A report can be
resubmitted without changes, and the referee does not change its
mind. A report can be sent to the referee, and the fixed point walked
by hand. The confidential comments are in the console, and so is the
obstruction: stderr is the console.

The result is on the next page. The next pages are the Serre spectral
sequence of S^7 -> S^15 -> S^8, one page per click: E_2, E_3, and so
on. d_2 through d_7 are zero, and nothing happens for six pages. d_8
kills two classes. E_9 is E_infinity, and the result is on that page:
the cohomology of S^15. After that the sequence has degenerated, and
the next page is the same page.

## Why "sheaf"

A sheaf on a topological space X assigns data to every open set,
consistently on overlaps. The global sections functor Gamma extracts
observable data: Gamma(X, F) = F(X). Without Gamma, you have local
data everywhere -- the stalks are correct, the restriction maps are
compatible, the sheaf condition is satisfied. You cannot see any of it.

This language is a sheaf without Gamma.

A quasi-coherent sheaf is one that locally looks like a module over
a ring. sheaf programs are quasi-coherent: every subroutine locally
looks like a working program. The local computation is perfect.
The global observation is zero.

## Notes

- There is no `-q` flag. There is nothing to quiet.

- "Clearly" is the mathematician's NOP. It asserts that the step is
  obvious without performing it. In sheaf, "clearly" is literally a
  NOP. The honesty is unprecedented.

- sheaf is Turing-complete. It can compute any computable function.
  It cannot communicate the result.

- Every sheaf program produces the same stdout: nothing. From the
  outside, all sheaf programs are equivalent. From the inside, they
  differ. The inside is where the mathematics lives. (R^i leaks to
  stderr. The derived category is not stdout.)

- Haskell is applied. sheaf is pure.

## License

AGPL-3.0-or-later. What is given freely must remain free.

---

The computation was correct. You will have to take our word for it.
