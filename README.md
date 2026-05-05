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
| `vacuously N` | | JZ N | Vacuously true. The premise is zero. |
| `nontrivially N` | `iff N` | JNZ N | The value is nontrivial. |
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

See `examples/` for 17 programs, including the seminar, the referee,
the thesis defense, the arXiv preprint, the geometric Langlands
conjecture, sheaf cohomology, vanishing theorems, and the long exact
sequence.

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
