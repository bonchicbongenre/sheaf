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

See `examples/` for 22 programs, including the seminar, the referee,
the thesis defense, the arXiv preprint, the geometric Langlands
conjecture, sheaf cohomology, vanishing theorems, Grothendieck
vanishing, the long exact sequence, Gauss at seven, Euclid read as a
descent, Collatz from 27, the circle by three arcs and by two, a
lemma used twice, and the reader's square. `submissions/` holds three more. They do not
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
| R | The referee read 100000 lines and did not reach the end. | major |
| P | A phrase performs from inside a sentence. | major; minor if it does nothing |
| D | A citation names a heading that is not there. | major |
| X | The citations go round. The argument is circular. | major |
| U | A hypothesis is used that was never introduced. The stack was empty. | major |
| O | Hypotheses are still open at QED. | major |
| L | A lemma is never cited, so its proof was not read. | minor |
| S | The citations fall into pieces. The paper may be several papers. | minor |
| E | It is left to the reader. Please include it. | minor |
| N | `NTS`, `WTS` or `RTP`. The need is stated. | minor |
| W | It is well known. No reference is given. | minor |
| C | Clearly. | minor |

A phrase is an instruction when it is a capitalized whole word at the
start of a sentence. Anywhere else it is commentary. Commentary
performs anyway.

A claim is checked against the stack as the line is reached, or, if
the stack is empty, against the last value to leave it. A claim about
the obstruction is checked against the last obstruction to leak. The
referee says whether a claim holds. It does not say what holds.

Of the 26 examples, 11 are accepted, 7 need minor revision, 6 need
major revision, and 2 are rejected. Among the accepted: "With two
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

Observation is safe. Publication is safe. Supposition is safe. A
dilemma is safe. A phase transition is safe.

"The proof seems correct." does not terminate. It sends the reader
back to the beginning, and the beginning leads back to it.
`submissions/seems.sheaf` is that proof. The program terminates,
unless it seems to.

`submissions/diffeomorphism.sheaf` does not get past its title.

## The Fixed Point

```
make fixpoint
```

A report is prose, so a report is a manuscript. `make fixpoint` gives
each manuscript's report to the referee, and that report to the
referee, until a report comes round again. Where each review ends is
kept in `fixpoint/`.

Of the 29 reviews, 19 reach a fixed point by the third round. The
fixed points are reports that could not finish reading themselves:
"I read 100000 lines and did not reach the end." There are 7 of
them. All 19 recommend major revision.

In 17 of the 19, the reader was sent back by the words the referee
used to say it could not see: "see attached", "unable to see", "I
could not see the result", "I have not seen the result". In the
other 2, by the manuscript's own false friend, quoted.

The other 10 reviews do not come round in 16 rounds. Their referee
wore the lack in other words: "could not tell", "pending", "did not
reach me". Each report publishes. Each "publishes" publishes, and has
to be reported. The report grows by 6 lines a round.

## Tests

```
make test
```

There are three tests.

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
