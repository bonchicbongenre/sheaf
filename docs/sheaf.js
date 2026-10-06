/*
 * sheaf.js -- the machine and the referee, in the browser.
 *
 * A port of stalk.h and referee.c. The referee here writes the report
 * the C referee writes, byte for byte, and the author leaks the same
 * obstructions; tools/site-test.js checks every example against its
 * golden files. Values are 64-bit, as in C.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
(function (root) {
'use strict';

var STACK_CAP = 1024, MEM_SIZE = 256, MAX_LINES = 65536;
var NOWHERE = -1, MAX_LABELS = 4096, MAX_OPENS = 64, MAX_OVERLAPS = 256;
var I64MAX = (1n << 63n) - 1n, I64MIN = -(1n << 63n);

var OP = {
  PUSH: 0, PUSH0: 1, POP: 2, DUP: 3, SWAP: 4, ADD: 5, SUB: 6, MUL: 7, DIV: 8,
  LOAD: 9, STORE: 10, EMIT: 11, PRINT: 12, JMP: 13, JZ: 14, JNZ: 15,
  HALT: 16, NOP: 17, CALL: 18, RET: 19, LOOP: 20, REPEAT: 21,
  COVER: 22, TRANS: 23, GLUE: 24, READ: 25,
  YONEDA: 26, NONSENSE: 27, SIMILAR: 28, ASSUME: 29, CONTRA: 30, CITEFILE: 31
};

/* the phrase table, in the C order: the order breaks ties */
var PH = [
  ['suppose', OP.PUSH, 1], ['trivially', OP.PUSH0, 0],
  ['without loss of generality', OP.POP, 0], ['recall', OP.DUP, 0],
  ['by duality', OP.SWAP, 0], ['direct sum', OP.ADD, 0], ['restrict', OP.SUB, 0],
  ['tensor', OP.MUL, 0], ['localize', OP.DIV, 0], ['pullback', OP.LOAD, 0],
  ['pushforward', OP.STORE, 0], ['observe', OP.EMIT, 0], ['publish', OP.PRINT, 0],
  ['see', OP.JMP, 1], ['vacuously', OP.JZ, 1], ['nontrivially', OP.JNZ, 1],
  ['QED', OP.HALT, 0], ['clearly', OP.NOP, 0], ['it is well known', OP.NOP, 0],
  ['by lemma', OP.CALL, 1], ['by proposition', OP.CALL, 1], ['by corollary', OP.CALL, 1],
  ['by claim', OP.CALL, 1], ['by theorem', OP.CALL, 1], ['this proves the', OP.RET, 0],
  ['by induction', OP.LOOP, 0], ['this completes the induction', OP.REPEAT, 0],
  ['cover', OP.COVER, 0], ['the transition', OP.TRANS, 0], ['by gluing', OP.GLUE, 0],
  ['left to the reader', OP.READ, 0],
  ['by yoneda', OP.YONEDA, 0], ['by abstract nonsense', OP.NONSENSE, 0], ['similarly', OP.SIMILAR, 0],
  ['assume for contradiction', OP.ASSUME, 0], ['contradiction', OP.CONTRA, 0],
  ['by [', OP.CITEFILE, 0],
  ['WLOG', OP.POP, 0], ['wlog', OP.POP, 0], ['iff', OP.JZ, 1], ['cf.', OP.JMP, 1],
  ['cf ', OP.JMP, 1], ['op.', OP.SWAP, 0], ['resp.', OP.DUP, 0], ['TFAE', OP.NOP, 0],
  ['NTS', OP.NOP, 0], ['WTS', OP.NOP, 0], ['RTP', OP.NOP, 0], ['s.t.', OP.NOP, 0]
].map(function (r) { return { text: r[0], op: r[1], hasArg: r[2] }; });

var KINDS = ['Lemma', 'Proposition', 'Corollary', 'Claim', 'Theorem', 'Case'];
var NLAZY = 4;

/* ---- the C library, ASCII and C locale ---- */

function isdigit(c) { return c !== undefined && c >= '0' && c <= '9'; }
function isalpha(c) { return c !== undefined && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')); }
function isalnum(c) { return isalpha(c) || isdigit(c); }
function isupper(c) { return c !== undefined && c >= 'A' && c <= 'Z'; }
function isspace(c) { return c === ' ' || c === '\t' || c === '\n' || c === '\v' || c === '\f' || c === '\r'; }
function lower(s) {
  return s.replace(/[A-Z]/g, function (c) { return String.fromCharCode(c.charCodeAt(0) + 32); });
}
function ncaseeq(s, i, w) { return lower(s.substr(i, w.length)) === lower(w); }
function cistrstr(s, w, from) {
  var i = lower(s).indexOf(lower(w), from || 0);
  return i;
}
function wrap64(x) { return BigInt.asIntN(64, x); }
function int32(x) { return Number(BigInt.asIntN(32, x)); }

/* strtoll at i: sign, digits, clamped */
function strtoll(s, i) {
  var neg = false, v = 0n, any = false;
  while (isspace(s[i])) i++;
  if (s[i] === '+' || s[i] === '-') { neg = s[i] === '-'; i++; }
  while (isdigit(s[i])) { v = v * 10n + BigInt(s.charCodeAt(i) - 48); i++; any = true;
    if (v > I64MAX + 1n) v = I64MAX + 1n; }
  if (!any) return 0n;
  v = neg ? -v : v;
  if (v > I64MAX) v = I64MAX;
  if (v < I64MIN) v = I64MIN;
  return v;
}

function scanint(s, i) {
  for (; i < s.length; i++) {
    if (s[i] === '-' && isdigit(s[i + 1])) return strtoll(s, i);
    if (isdigit(s[i])) return strtoll(s, i);
  }
  return 0n;
}

/* ---- the machine ---- */

function Machine(source, reader) {
  var lines = source.split('\n');
  if (lines.length && lines[lines.length - 1] === '') lines.pop();
  lines = lines.slice(0, MAX_LINES).map(function (l) { return l.replace(/[\r\n]+$/, ''); });

  var m = this;
  m.lines = lines;
  m.nlines = lines.length;
  m.stk = [];
  m.underflow = 0;
  m.rstk = [];
  m.mem = [];
  for (var i = 0; i < MEM_SIZE; i++) m.mem.push(0n);
  m.pc = 0;
  m.labels = [];
  m.skipTo = [];
  m.loopHead = [];
  m.opens = [];
  m.ovA = []; m.ovB = []; m.ovC = [];
  m.reader = reader;     /* a string the reader gives, or null */
  m.readAt = 0;
  m.lastLine = NOWHERE;  /* what "Similarly." performs again */
  m.frames = [];         /* each assumption made for contradiction */
  m.index();
}

Machine.prototype.push = function (v) { if (this.stk.length < STACK_CAP) this.stk.push(wrap64(v)); };
Machine.prototype.pop = function () {
  if (this.stk.length > 0) return this.stk.pop();
  this.underflow++; return 0n;
};
Machine.prototype.peek = function () {
  if (this.stk.length > 0) return this.stk[this.stk.length - 1];
  this.underflow++; return 0n;
};

function matchLine(ln) {
  var best = null, bpos = -1;
  for (var k = 0; k < PH.length; k++) {
    var at = cistrstr(ln, PH[k].text);
    if (at >= 0 && (bpos < 0 || at < bpos)) { best = PH[k]; bpos = at; }
  }
  return { p: best, at: bpos };
}
Machine.prototype.match = matchLine;

function readId(s, i) {
  var id = '';
  while (s[i] === ' ') i++;
  while ((isdigit(s[i]) || s[i] === '.') && id.length < 31) { id += s[i]; i++; }
  while (id.length > 0 && id[id.length - 1] === '.') id = id.slice(0, -1);
  return id;
}

function heading(ln) {
  var i = 0;
  while (isspace(ln[i])) i++;
  for (var k = 0; k < KINDS.length; k++) {
    var n = KINDS[k].length;
    if (ln.substr(i, n) !== KINDS[k]) continue;
    var c = ln[i + n];
    if (c !== ' ' && c !== '.' && c !== undefined) continue;
    return { kind: k, id: readId(ln, i + n) };
  }
  return null;
}

function kindNamed(s, i) {
  for (var k = 0; k < KINDS.length; k++)
    if (ncaseeq(s, i, KINDS[k])) return k;
  return NOWHERE;
}

Machine.prototype.findLabel = function (kind, id) {
  for (var i = 0; i < this.labels.length; i++)
    if (this.labels[i].kind === kind && this.labels[i].id === id) return this.labels[i].line;
  return NOWHERE;
};

Machine.prototype.index = function () {
  var open = [], i, r;
  for (i = 0; i < this.nlines; i++) { this.skipTo[i] = NOWHERE; this.loopHead[i] = NOWHERE; }
  for (i = 0; i < this.nlines; i++) {
    var h = heading(this.lines[i]);
    if (h && this.labels.length < MAX_LABELS)
      this.labels.push({ kind: h.kind, id: h.id, line: i });
    r = this.match(this.lines[i]);
    if (r.p && r.p.op === OP.LOOP) open.push(i);
    if (r.p && r.p.op === OP.REPEAT && open.length > 0) this.loopHead[i] = open.pop();
  }
  for (var l = 0; l < this.labels.length; l++) {
    if (this.labels[l].kind >= NLAZY) continue;
    for (var j = this.labels[l].line + 1; j < this.nlines; j++) {
      r = this.match(this.lines[j]);
      if (r.p && r.p.op === OP.RET) { this.skipTo[this.labels[l].line] = j + 1; break; }
    }
  }
};

function tryDerived(ln) {
  var p = ln.indexOf('R^');
  if (p < 0) return -1;
  p += 2;
  if (!isdigit(ln[p])) return -1;
  var e = p;
  while (isdigit(ln[e])) e++;
  var degree = strtoll(ln, p);
  if (cistrstr(ln, 'observe', e) < 0 && cistrstr(ln, 'publish', e) < 0) return -1;
  return Number(degree);
}

Machine.prototype.destination = function (s, i, e) {
  while (s[i] === ' ') i++;
  var k = kindNamed(s, i);
  if (k !== NOWHERE && s[i + KINDS[k].length] === ' ') {
    var id = readId(s, i + KINDS[k].length);
    e.cite = KINDS[k] + (id ? ' ' : '') + id;
    var l = this.findLabel(k, id);
    if (l === NOWHERE) { e.unresolved = 1; return NOWHERE; }
    return l + 1;
  }
  return int32(scanint(s, i));
};

/* ---- covers ---- */

function nextOpen(ln, i) {
  for (; i < ln.length; i++) {
    if (ln[i] !== 'U' || !isdigit(ln[i + 1])) continue;
    if (i > 0 && isalnum(ln[i - 1])) continue;
    var e = i + 1;
    while (isdigit(ln[e])) e++;
    if (isalnum(ln[e])) continue;
    return { name: ln.slice(i, Math.min(e, i + 15)), end: e };
  }
  return null;
}

Machine.prototype.openNamed = function (name) {
  for (var i = 0; i < this.opens.length; i++) if (this.opens[i] === name) return i;
  if (this.opens.length === MAX_OPENS) return NOWHERE;
  this.opens.push(name);
  return this.opens.length - 1;
};

function transitionValue(s, i) {
  for (; i < s.length; i++) {
    if (s[i] === 'U' && isdigit(s[i + 1])) {
      i++;
      while (isdigit(s[i + 1])) i++;
      continue;
    }
    if ((s[i] === '-' && isdigit(s[i + 1])) || isdigit(s[i])) return strtoll(s, i);
  }
  return 0n;
}

/* the class of an integer 1-cochain on a graph, in H^1(graph; Z) = Z^b1 */
function cechH1(n, m, a, b, c) {
  var seen = [], tree = [], f = [], cls = [], i;
  for (i = 0; i < n; i++) { seen.push(false); f.push(0n); }
  for (i = 0; i < m; i++) tree.push(false);
  for (var r = 0; r < n; r++) {
    if (seen[r]) continue;
    var queue = [r], head = 0;
    seen[r] = true;
    while (head < queue.length) {
      var u = queue[head++];
      for (var e = 0; e < m; e++) {
        if (tree[e] || a[e] === b[e]) continue;
        if (a[e] === u && !seen[b[e]]) {
          f[b[e]] = wrap64(f[u] + c[e]); seen[b[e]] = true; tree[e] = true; queue.push(b[e]);
        } else if (b[e] === u && !seen[a[e]]) {
          f[a[e]] = wrap64(f[u] - c[e]); seen[a[e]] = true; tree[e] = true; queue.push(a[e]);
        }
      }
    }
  }
  for (var k = 0; k < m; k++)
    if (!tree[k]) cls.push(wrap64(f[a[k]] + c[k] - f[b[k]]));
  return cls;
}

/* ---- one step ---- */

Machine.prototype.fresh = function (line) {
  var m = this;
  return {
    line: line, text: m.lines[line], p: null, at: -1, degree: -1,
    spBefore: m.stk.length,
    topBefore: m.stk.length > 0 ? m.stk[m.stk.length - 1] : 0n,
    underflows: 0, gave: 0, given: 0n, next: 0, deferred: 0,
    target: NOWHERE, unresolved: 0, cite: '', glue: NOWHERE, b1: 0,
    discharged: 0, repeat: null, repeatLine: NOWHERE, repeatDegree: -1, contra: 0
  };
};

Machine.prototype.step = function (derived, obs) {
  var m = this;
  if (m.pc < 0 || m.pc >= m.nlines) return false;

  var e = m.fresh(m.pc), u0 = m.underflow, run = { running: true };

  m.pc++;

  if (m.skipTo[e.line] !== NOWHERE) {
    m.pc = m.skipTo[e.line];
    e.deferred = 1;
  } else {
    m.perform(e.line, e, derived, run);
    if (e.degree >= 0 || (e.p && e.p.op !== OP.SIMILAR)) m.lastLine = e.line;
  }

  e.underflows = m.underflow - u0;
  e.next = m.pc;
  if (obs) obs(e);
  return run.running && m.pc >= 0 && m.pc < m.nlines;
};

/* perform one line: a derived functor, or the leftmost phrase */
Machine.prototype.perform = function (line, e, derived, run) {
  var m = this, ln = m.lines[line], degree;

  if ((degree = tryDerived(ln)) >= 0) {
    var v = m.pop();
    e.degree = degree; e.gave = 1; e.given = v;
    if (degree >= 1 && derived) derived.push('H^' + degree + '(X,F) = ' + v + '\n');
  } else {
    var r = m.match(ln), p = r.p, at = r.at;
    e.p = p; e.at = at;
    if (p) {
      var arg = 0n;
      if (p.hasArg) arg = scanint(ln, at + p.text.length);
      var a, b, rest = at + p.text.length;
      switch (p.op) {
      case OP.PUSH: m.push(arg); break;
      case OP.PUSH0: m.push(0n); break;
      case OP.POP: m.pop(); break;
      case OP.DUP: m.push(m.peek()); break;
      case OP.SWAP: a = m.pop(); b = m.pop(); m.push(a); m.push(b); break;
      case OP.ADD: a = m.pop(); b = m.pop(); m.push(b + a); break;
      case OP.SUB: a = m.pop(); b = m.pop(); m.push(b - a); break;
      case OP.MUL: a = m.pop(); b = m.pop(); m.push(b * a); break;
      case OP.DIV: a = m.pop(); b = m.pop(); m.push(a ? b / a : 0n); break;
      case OP.LOAD: a = m.pop(); m.push(a >= 0n && a < BigInt(MEM_SIZE) ? m.mem[Number(a)] : 0n); break;
      case OP.STORE: a = m.pop(); b = m.pop(); if (b >= 0n && b < BigInt(MEM_SIZE)) m.mem[Number(b)] = a; break;
      case OP.EMIT: case OP.PRINT: e.gave = 1; e.given = m.pop(); break;
      case OP.JMP:
        e.target = m.destination(ln, rest, e);
        if (e.target !== NOWHERE) m.pc = e.target;
        break;
      case OP.JZ:
        e.target = m.destination(ln, rest, e);
        if (!m.pop() && e.target !== NOWHERE) m.pc = e.target;
        break;
      case OP.JNZ:
        e.target = m.destination(ln, rest, e);
        if (m.pop() && e.target !== NOWHERE) m.pc = e.target;
        break;
      case OP.HALT: run.running = false; break;
      case OP.NOP: break;
      case OP.CALL: {
        var k = kindNamed(p.text, 3), id = readId(ln, rest);
        e.cite = KINDS[k] + (id ? ' ' : '') + id;
        var l = m.findLabel(k, id);
        if (l === NOWHERE) e.unresolved = 1;
        else { if (m.rstk.length < STACK_CAP) m.rstk.push(m.pc); m.pc = e.target = l + 1; }
        break;
      }
      case OP.RET: if (m.rstk.length > 0) m.pc = m.rstk.pop(); break;
      case OP.LOOP: break;
      case OP.REPEAT:
        if (m.loopHead[line] !== NOWHERE && m.peek()) m.pc = m.loopHead[line] + 1;
        break;
      case OP.COVER: {
        var s = 0, o;
        m.opens = []; m.ovA = []; m.ovB = []; m.ovC = [];
        while ((o = nextOpen(ln, s))) { m.openNamed(o.name); s = o.end; }
        break;
      }
      case OP.TRANS: {
        var o1 = nextOpen(ln, 0), o2 = o1 ? nextOpen(ln, o1.end) : null;
        if (o1 && o2 && m.ovA.length < MAX_OVERLAPS) {
          var x = m.openNamed(o1.name), y = m.openNamed(o2.name);
          if (x !== NOWHERE && y !== NOWHERE) {
            m.ovA.push(x); m.ovB.push(y); m.ovC.push(transitionValue(ln, rest));
          }
        }
        break;
      }
      case OP.GLUE: {
        var cls = cechH1(m.opens.length, m.ovA.length, m.ovA, m.ovB, m.ovC), glued = 1;
        for (var q = 0; q < cls.length; q++) if (cls[q]) glued = 0;
        m.push(BigInt(glued));
        e.glue = glued; e.b1 = cls.length;
        if (!glued && derived)
          derived.push('H^1(U,Z) = Z^' + cls.length + '; the class is (' + cls.join(', ') + ')\n');
        break;
      }
      case OP.READ: {
        if (m.reader !== null) {
          var mt = /^\s*([+-]?\d+)/.exec(m.reader.slice(m.readAt));
          if (mt) { m.readAt += mt[0].length; m.push(strtoll(mt[1], 0)); }
          else m.reader = null;     /* the reader gave something that is not a number */
        }
        break;
      }
      case OP.YONEDA: break;
      case OP.NONSENSE: e.discharged = m.stk.length; m.stk = []; break;
      case OP.SIMILAR:
        if (m.lastLine !== NOWHERE) {
          var again = m.fresh(m.lastLine);
          m.perform(m.lastLine, again, derived, run);
          e.repeat = again.p; e.repeatLine = m.lastLine; e.repeatDegree = again.degree;
          e.gave = again.gave; e.given = again.given;
          e.glue = again.glue; e.b1 = again.b1; e.discharged = again.discharged;
          e.contra = again.contra;
        }
        break;
      case OP.ASSUME:
        if (m.frames.length < STACK_CAP) m.frames.push(m.stk.length);
        break;
      case OP.CONTRA:
        a = m.pop();
        if (!a) {
          e.contra = 3;
          if (m.frames.length > 0) m.frames.pop();
        } else if (m.frames.length > 0) {
          m.stk.length = m.frames.pop();
          m.push(1n);
          e.contra = 1;
        } else {
          m.stk = [];
          m.push(1n);
          e.contra = 2;
        }
        break;
      case OP.CITEFILE:
        /* by [fermat.sheaf]: cited. not read. */
        break;
      }
    }
  }
};

/*
 * The author. stdout is empty. stderr carries what leaks.
 * The browser stops after `limit` lines, so it can go on.
 */
function run(source, readerText, limit) {
  var m = new Machine(source, readerText === undefined ? null : readerText);
  var derived = [], read = 0, more;
  limit = limit || 1000000;
  do { more = m.step(derived, function () { read++; }); } while (more && read < limit);
  return { stdout: '', stderr: derived.join(''), finished: !more, lines: read };
}

/* ---- the referee ---- */

var BUDGET = 100000, WIDTH = 66, NB = '\u0001';
var ACCEPT = 0, MINOR = 1, MAJOR = 2, REJECT = 3;
var DECISION_FORM = ['ACCEPT', 'MINOR REVISION', 'MAJOR REVISION', 'REJECT'];
var DECISION_PROSE = ['accept', 'minor revision', 'major revision', 'reject'];
var FIELD = ['not visible', 'could not tell', 'unable to see', 'see attached', 'N/A', 'pending', 'on file'];
var WORN = [
  'I could not see the result. My comments concern the text.',
  'The result was not visible to me. What follows concerns the text.',
  'I did not see what the proof arrives at. I have read the rest.',
  'The result did not reach me. I have checked what I could.',
  'I have not seen the result. The comments below are on the text alone.',
  'The result was not available to me. I have read the proof closely.',
  'I read the proof without its result. My comments follow.'
];
var CONFIDENTIAL = [
  'I am not an expert in this area.',
  'I did not check the proofs. Nobody can.',
  'The author seems competent.',
  'I read this on a train.',
  "Please do not send me this author's next paper.",
  'I could not see the result. I do not think the author could either.',
  'This took me four minutes.'
];
var SUBJECTS = ['result', 'answer', 'sum', 'value', 'this', 'stack', 'obstruction'];
var VERBS = ['is', 'was', 'equals', 'contains'];
var WORDS = ['zero', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten'];

function isInstruction(ln, at, p) {
  var n = p.text.length, last = p.text[n - 1];
  if (at > 0 && isalpha(ln[at - 1])) return false;
  if (isalpha(last) && isalpha(ln[at + n])) return false;
  if (!isupper(ln[at])) return false;
  var q = at;
  while (q > 0 && (ln[q - 1] === '"' || ln[q - 1] === '(')) q--;
  var r = q;
  while (r > 0 && isspace(ln[r - 1])) r--;
  if (r === 0) return true;
  return r < q && '.?!:'.indexOf(ln[r - 1]) >= 0;
}

function hidingPlace(ln, at, p) {
  var s = at, e = at + p.text.length;
  while (s > 0 && isalpha(ln[s - 1])) s--;
  while (e < ln.length && isalpha(ln[e])) e++;
  while (e > s && !isalpha(ln[e - 1])) e--;
  return ln.slice(s, Math.min(e, s + 63));
}

function wordAt(ln, at, w) {
  if (at > 0 && isalpha(ln[at - 1])) return false;
  if (!ncaseeq(ln, at, w) || ln.length - at < w.length) return false;
  return !isalnum(ln[at + w.length]);
}

function number(k, capital) {
  var s = k >= 0 && k <= 10 ? WORDS[k] : String(k);
  return capital ? s[0].toUpperCase() + s.slice(1) : s;
}
function times(k) {
  if (k === 1) return 'once';
  if (k === 2) return 'twice';
  return number(k, false) + ' times';
}

function fnv(s) {
  var h = 2166136261;
  for (var i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i) & 0xff;
    h = Math.imul(h, 16777619) >>> 0;
  }
  return h >>> 0;
}

/* a step that does nothing: "clearly", and a citation */
function inert(op) { return op === OP.NOP || op === OP.CITEFILE; }

/* on a paper cited, which nobody here has read */
var UNREAD = [
  'I did not read it either.',
  'I have not read it.',
  'I know of it.',
  'I have it somewhere.',
  'I am told it is good.'
];

var REFEREE_1 = 0, REVIEWER_2 = 1;
var WORN2 = [
  'I could not see the result. I have seen results like it.',
  'I did not see the result, and I do not need to.',
  'The result was not visible to me. Nor, I suspect, to the author.'
];
var CONFIDENTIAL2 = [
  'I have not read the paper, but I know the area.',
  'I would reject this even if it were correct.',
  'This is my third review of this paper, at a third journal.'
];
var VERDICT = [
  'I am pleased to accept your manuscript.',
  'I would be glad to see a revised version.',
  'I would consider a substantially revised version.',
  'I regret that I cannot accept your manuscript.'
];

/*
 * library: the names of the papers beside the manuscript, where its
 * citations are looked for.
 */
function review(source, name, library) {
  var M = new Machine(source, null);
  var nl = M.nlines, mark = [], i;
  library = library || [];
  for (i = 0; i < (nl || 1); i++)
    mark.push({ seen: 0, prose: 0, underflow: 0, code: '', falseClaim: 0, op: 0, phrase: '',
                target: 0, sent: 0, word: '', claim: '', denial: '', cite: '',
                dangling: 0, danglingOp: 0, called: 0, yoneda: 0, discharged: 0, similarOf: 0,
                noContra: 0, explosion: 0, cited: '' });

  var linesRead = 0, steps = 0, emptySteps = 0, observes = 0, publishes = 0, leaks = 0;
  var claimsChecked = 0, claimsFalse = 0, haveSection = 0, haveObstruction = 0;
  var section = 0n, obstruction = 0n, unfinished = 0, halted = 0;

  function checkClaim(e, mk) {
    var ln = e.text;
    for (var v = 0; v < ln.length; v++) {
      var verb = null;
      for (var w = 0; w < VERBS.length; w++) if (wordAt(ln, v, VERBS[w])) { verb = VERBS[w]; break; }
      if (!verb || ln[v + verb.length] !== ' ') continue;

      var num = v + verb.length + 1, claimed, end;
      if (wordAt(ln, num, 'zero')) { claimed = 0n; end = num + 4; }
      else {
        var d = num;
        if (ln[d] === '-') d++;
        if (!isdigit(ln[d])) continue;
        while (isdigit(ln[d])) d++;
        if (isalnum(ln[d])) continue;
        claimed = strtoll(ln, num);
        end = d;
      }

      var start = 0, c, s;
      for (c = 0; c < v; c++) if ('.?!:'.indexOf(ln[c]) >= 0 && ln[c + 1] === ' ') start = c + 2;
      var subj = -1, which = null;
      for (c = start; c < v; c++)
        for (s = 0; s < SUBJECTS.length; s++)
          if (wordAt(ln, c, SUBJECTS[s])) { subj = c; which = SUBJECTS[s]; }
      if (subj < 0) continue;

      var actual;
      if (which === 'obstruction') {
        if (!haveObstruction) return;
        actual = obstruction;
      } else if (e.spBefore > 0) actual = e.topBefore;
      else if (haveSection) actual = section;
      else return;

      claimsChecked++;
      if (actual === claimed) return;

      claimsFalse++;
      mk.falseClaim = 1;
      var q = subj;
      if (q >= 4 && ncaseeq(ln, q - 4, 'the ')) q -= 4;
      var cl = ln.slice(q, Math.min(end, q + 159));
      mk.claim = cl ? lower(cl[0]) + cl.slice(1) : cl;
      var shown = claimed === 0n ? 'zero' : String(claimed);
      if (verb === 'is') mk.denial = 'It is not ' + shown + '.';
      else if (verb === 'was') mk.denial = 'It was not ' + shown + '.';
      else mk.denial = 'It does not.';
      return;
    }
  }

  function readLine(e) {
    var mk = mark[e.line], first = !mk.seen;
    mk.seen = 1;
    linesRead++;
    if (first) checkClaim(e, mk);

    if (e.degree >= 0) {
      steps++;
      if (e.degree >= 1) leaks++;
      else if (cistrstr(e.text, 'publish') >= 0) publishes++;
      else observes++;
    } else if (e.p) {
      var p = e.p, q = e.repeat || p;
      steps++;
      if (e.repeatDegree >= 1) leaks++;
      else if (e.repeatDegree === 0) {
        if (cistrstr(M.lines[e.repeatLine], 'publish') >= 0) publishes++; else observes++;
      }
      if (inert(q.op)) emptySteps++;
      if (q.op === OP.EMIT) observes++;
      if (q.op === OP.PRINT) publishes++;
      if (q.op === OP.HALT) halted = 1;
      if (p.op === OP.YONEDA) mk.yoneda = 1;
      if (e.discharged > mk.discharged) mk.discharged = e.discharged;
      if (p.op === OP.SIMILAR && e.repeatLine !== NOWHERE && !mk.similarOf) mk.similarOf = e.repeatLine + 1;
      if (e.contra === 3) mk.noContra = 1;
      if (e.contra === 2) mk.explosion = 1;
      if (p.op === OP.CITEFILE && first) {
        /* the paper cited: is it in the library? */
        var s0 = e.at + p.text.length, t0 = e.text.indexOf(']', s0);
        if (t0 >= 0) {
          mk.cited = e.text.slice(s0, Math.min(t0, s0 + 63));
          if (library.indexOf(mk.cited) < 0) {
            mk.dangling = 1;
            mk.danglingOp = OP.CALL;
            mk.cite = ('[' + mk.cited + ']').slice(0, 47);
          }
        }
      }

      if (isInstruction(e.text, e.at, p)) {
        if (first) {
          var t = lower(p.text);
          if (t === 'it is well known' || t === 'tfae') mk.code = 'W';
          else if (t === 'clearly') mk.code = 'C';
          else if (p.text === 'NTS' || p.text === 'WTS' || p.text === 'RTP') mk.code = 'N';
          else if (p.op === OP.READ) mk.code = 'E';
        }
      } else {
        mk.prose = 1;
        mk.op = p.op;
        mk.phrase = p.text;
        mk.word = hidingPlace(e.text, e.at, p);
        if (p.op === OP.CITEFILE && mk.word.indexOf('[') >= 0)
          /* the word is the one before the bracket, not the paper */
          mk.word = mk.word.slice(0, mk.word.indexOf('[')).replace(/ +$/, '');
        if (p.op === OP.JMP || p.op === OP.JZ || p.op === OP.JNZ) {
          mk.target = e.target;
          if (e.target !== NOWHERE && e.next === e.target) mk.sent++;
        }
      }
      if (e.cite) mk.cite = e.cite;
      if (e.unresolved) { mk.dangling = 1; mk.danglingOp = p.op; }
      if (p.op === OP.CALL && e.target !== NOWHERE) mark[e.target - 1].called = 1;
    }

    if (e.glue === 0) leaks++;
    if (e.underflows) mk.underflow = 1;
    if (e.gave) {
      var deg = e.degree >= 0 ? e.degree : e.repeatDegree;
      if (deg >= 1) { haveObstruction = 1; obstruction = e.given; }
      else { haveSection = 1; section = e.given; }
    }
  }

  while (M.step(null, readLine))
    if (linesRead >= BUDGET) { unfinished = 1; break; }

  /* ---- what was found ---- */

  var anyProseMajor = 0, anyProseMinor = 0;
  for (i = 0; i < nl; i++) {
    if (!mark[i].prose) continue;
    if (inert(mark[i].op)) anyProseMinor = 1; else anyProseMajor = 1;
  }
  function lineList(has) {
    var out = '', count = 0, total = 0, j;
    for (j = 0; j < nl; j++) if (has(mark[j])) total++;
    for (j = 0; j < nl; j++) {
      if (!has(mark[j])) continue;
      out += (count === 0 ? NB : (count === total - 1 ? ' and ' : ', ')) + (j + 1);
      count++;
    }
    return { text: out, n: total };
  }
  var U = lineList(function (x) { return x.underflow; });
  var W = lineList(function (x) { return x.code === 'W'; });
  var C = lineList(function (x) { return x.code === 'C'; });
  var N = lineList(function (x) { return x.code === 'N'; });
  var E = lineList(function (x) { return x.code === 'E'; });
  var open = unfinished ? 0 : M.stk.length;

  var anyDangling = 0;
  for (i = 0; i < nl; i++) if (mark[i].dangling) anyDangling = 1;

  var labels = M.labels, unread = 0, l;
  for (l = 0; l < labels.length && !unfinished; l++)
    if (labels[l].kind < NLAZY && M.skipTo[labels[l].line] !== NOWHERE && !mark[labels[l].line].called)
      unread++;

  /* the citation graph */
  var vlabel = [-1], ea = [], eb = [], owner = [];
  for (i = 0; i < nl; i++) owner.push(0);
  for (l = 0; l < labels.length && vlabel.length < MAX_LABELS + 1; l++) {
    if (labels[l].kind >= NLAZY) continue;
    var vtx = vlabel.length;
    vlabel.push(l);
    var endl = M.skipTo[labels[l].line];
    if (endl === NOWHERE) endl = labels[l].line + 1;
    for (i = labels[l].line; i < endl && i < nl; i++) owner[i] = vtx;
  }
  for (i = 0; i < nl; i++) {
    var r = M.match(M.lines[i]);
    if (!r.p || r.p.op !== OP.CALL) continue;
    var cid = readId(M.lines[i], r.at + r.p.text.length);
    var cline = M.findLabel(kindNamed(r.p.text, 3), cid);
    if (cline === NOWHERE) continue;
    var to = 0;
    for (var vv = 1; vv < vlabel.length; vv++) if (labels[vlabel[vv]].line === cline) to = vv;
    ea.push(owner[i]); eb.push(to);
  }
  var nv = vlabel.length, ne = ea.length, zero = [];
  for (i = 0; i < ne; i++) zero.push(0n);
  var rank1 = cechH1(nv, ne, ea, eb, zero).length;
  var pieces = nv - ne + rank1;

  function vertexName(v, capital) {
    if (vlabel[v] < 0) return capital ? 'The text' : 'the text';
    var L = labels[vlabel[v]];
    return KINDS[L.kind] + (L.id ? NB : '') + L.id;
  }
  var cycles = [], color = [], path = [];
  for (i = 0; i < nv; i++) color.push(0);
  function circleBack(from) {
    var buf = '';
    for (var j = from; j < path.length; j++)
      buf += (j === from ? '' : (j === from + 1 ? ' cites ' : ', which cites ')) + vertexName(path[j], j === from);
    if (path.length - from === 1) buf += ' cites itself.';
    else buf += ', which cites ' + vertexName(path[from], false) + '.';
    if (cycles.indexOf(buf) < 0 && cycles.length < 8) cycles.push(buf);
  }
  function search(u) {
    color[u] = 1;
    path.push(u);
    for (var e = 0; e < ne; e++) {
      if (ea[e] !== u) continue;
      var v = eb[e];
      if (color[v] === 1) {
        for (var j = 0; j < path.length; j++) if (path[j] === v) { circleBack(j); break; }
      } else if (color[v] === 0) search(v);
    }
    path.pop();
    color[u] = 2;
  }
  for (i = 0; i < nv; i++) if (!color[i]) search(i);

  var anyNonsense = 0, anyNoContra = 0, anyExplosion = 0;
  for (i = 0; i < nl; i++) {
    if (mark[i].discharged) anyNonsense = 1;
    if (mark[i].noContra) anyNoContra = 1;
    if (mark[i].explosion) anyExplosion = 1;
  }

  var decision1 = ACCEPT;
  if (anyProseMinor || N.n || W.n || C.n || E.n || unread || pieces > 1 || anyNonsense) decision1 = MINOR;
  if (unfinished || anyProseMajor || anyDangling || cycles.length || U.n || open || anyNoContra) decision1 = MAJOR;
  if (claimsFalse || anyExplosion) decision1 = REJECT;

  var base = name.slice(name.lastIndexOf('/') + 1);
  var h = fnv(base);

  /* the referee and Reviewer 2 write from the same reading */
  function write(who) {
  var decision = who === REFEREE_1 ? decision1 : Math.min(decision1 + 1, REJECT);

  /* ---- the form ---- */

  var codes = '';
  if (claimsFalse) codes += ' F';
  if (anyExplosion) codes += ' I';
  if (unfinished) codes += ' R';
  if (anyProseMajor || anyProseMinor) codes += ' P';
  if (anyDangling) codes += ' D';
  if (cycles.length) codes += ' X';
  if (anyNoContra) codes += ' K';
  if (U.n) codes += ' U';
  if (open) codes += ' O';
  if (unread) codes += ' L';
  if (pieces > 1) codes += ' S';
  if (anyNonsense) codes += ' A';
  if (E.n) codes += ' E';
  if (N.n) codes += ' N';
  if (W.n) codes += ' W';
  if (C.n) codes += ' C';

  var out = [];
  var dash = '------------------------------------------------------------\n';
  out.push(who === REFEREE_1 ? 'EDITORIAL OFFICE -- REFEREE REPORT\n'
                             : 'EDITORIAL OFFICE -- REPORT OF REVIEWER 2\n', dash);
  out.push('manuscript        ' + base + '\n');
  out.push('lines             ' + nl + '\n');
  if (unfinished) out.push('steps             ' + steps + ' (reading stopped)\n');
  else if (emptySteps) out.push('steps             ' + steps + ' (' + emptySteps + ' of them empty)\n');
  else out.push('steps             ' + steps + '\n');
  out.push(unfinished ? 'hypotheses open   not reached\n' : 'hypotheses open   ' + open + '\n');
  out.push(claimsChecked ? 'claims            ' + claimsChecked + ' checked, ' + claimsFalse + ' false\n'
                         : 'claims            0 checked\n');
  out.push('citation graph    V ' + nv + ', E ' + ne + ', H^0 ' + pieces + ', H^1 ' + rank1 +
           ', chi ' + (nv - ne) + '\n');
  out.push('codes            ' + (codes || ' none') + '\n');
  out.push('result            ' + (who === REFEREE_1 ? FIELD[h % FIELD.length] : 'not new') + '\n');
  out.push('recommendation    ' + DECISION_FORM[decision] + '\n');
  out.push(dash + '\n');

  /* ---- the prose ---- */

  out.push('Comments to the author\n\n');

  var para = '', commentNo = 0;
  function pf(s) { para += s; }
  function pend(lead) {
    var ind = lead.length, col = ind, first = true, txt = lead;
    var words = para.split(' ');
    for (var k = 0; k < words.length; k++) {
      var w = words[k];
      if (!w) continue;
      if (!first && col + 1 + w.length > WIDTH) { txt += '\n' + new Array(ind + 1).join(' '); col = ind; }
      else if (!first) { txt += ' '; col++; }
      txt += w.split(NB).join(' ');
      col += w.length;
      first = false;
    }
    out.push(txt + '\n\n');
    para = '';
  }
  function comment() { commentNo++; pend(commentNo + '. '); }

  var opening = '';
  for (i = 0; i < nl; i++) {
    var sx = M.lines[i], z = 0;
    while (isspace(sx[z])) z++;
    if (z < sx.length) { opening = sx.slice(z); break; }
  }
  var ol = opening.length;
  while (ol > 0 && isspace(opening[ol - 1])) ol--;
  opening = opening.slice(0, ol);
  var punct = ol > 0 && '.?!'.indexOf(opening[ol - 1]) >= 0;
  pf('The manuscript begins: "' + opening + '"' + (punct ? '' : '.'));

  if (unfinished) pf(' It takes more than ' + steps + ' step' + (steps === 1 ? '' : 's') + '.');
  else {
    var parts = [];
    if (observes) parts.push('observes ' + times(observes));
    if (publishes) parts.push('publishes ' + times(publishes));
    if (leaks) parts.push('leaks ' + number(leaks, false) + ' obstruction' + (leaks === 1 ? '' : 's'));
    pf(' It takes ' + steps + ' step' + (steps === 1 ? '' : 's'));
    if (parts.length === 0) pf(' and does not publish.');
    else if (parts.length === 1) pf(' and ' + parts[0] + '.');
    else if (parts.length === 2) pf(', ' + parts[0] + ' and ' + parts[1] + '.');
    else pf(', ' + parts[0] + ', ' + parts[1] + ' and ' + parts[2] + '.');
  }
  pend('');

  pf(who === REFEREE_1 ? WORN[Math.floor(h / FIELD.length) % WORN.length]
                       : WORN2[Math.floor(h / FIELD.length) % WORN2.length]);
  pend('');

  if (who === REVIEWER_2) { pf('The result is not new.'); comment(); }

  function performs(mk) {
    switch (mk.op) {
    case OP.PUSH: return 'makes a supposition';
    case OP.PUSH0: return 'assumes the trivial case';
    case OP.POP: return 'discards a hypothesis';
    case OP.DUP: return 'recalls';
    case OP.SWAP: return 'dualizes';
    case OP.ADD: return 'performs a direct sum';
    case OP.SUB: return 'restricts';
    case OP.MUL: return 'tensors';
    case OP.DIV: return 'localizes';
    case OP.LOAD: return 'performs a pullback';
    case OP.STORE: return 'performs a pushforward';
    case OP.EMIT: return 'observes';
    case OP.PRINT: return 'publishes';
    case OP.JMP: case OP.JZ: case OP.JNZ:
      if (mk.cite) return (mk.sent ? 'sends' : 'may send') + ' the reader to ' + mk.cite;
      return (mk.sent ? 'sends' : 'may send') + ' the reader to line' + NB + (mk.target + 1);
    case OP.HALT: return 'ends the proof';
    case OP.CALL: return 'cites ' + mk.cite;
    case OP.RET: return 'ends a lemma';
    case OP.LOOP: return 'begins an induction';
    case OP.REPEAT: return 'completes an induction';
    case OP.COVER: return 'declares a cover';
    case OP.TRANS: return 'sets a transition';
    case OP.GLUE: return 'glues';
    case OP.READ: return 'asks the reader';
    case OP.YONEDA: return 'invokes Yoneda';
    case OP.NONSENSE: return 'discharges every hypothesis';
    case OP.SIMILAR: return 'does the last thing again';
    case OP.ASSUME: return 'assumes for contradiction';
    case OP.CONTRA: return 'declares a contradiction';
    case OP.CITEFILE: return 'cites a paper';
    default: return 'says ' + mk.phrase;
    }
  }

  for (i = 0; i < nl; i++)
    if (mark[i].falseClaim) { pf('Line' + NB + (i + 1) + ' says ' + mark[i].claim + '. ' + mark[i].denial); comment(); }

  for (i = 0; i < nl; i++)
    if (mark[i].explosion) {
      pf('Line' + NB + (i + 1) + ' derives a contradiction from no assumption. The paper proves everything.');
      comment();
    }

  if (unfinished) {
    pf('I read ' + linesRead + ' lines and did not reach the end. Please shorten the manuscript.');
    comment();
  }

  for (i = 0; i < nl; i++)
    if (mark[i].prose && !inert(mark[i].op)) {
      pf('Line' + NB + (i + 1) + ' is commentary, and its "' + mark[i].word + '" ' + performs(mark[i]) + '.');
      if (mark[i].sent > 1) pf(' It did so ' + times(mark[i].sent) + '.');
      pf(' Please move it out of the proof.');
      comment();
    }

  for (i = 0; i < nl; i++)
    if (mark[i].dangling) {
      pf('Line' + NB + (i + 1) + ' ' + (mark[i].danglingOp === OP.CALL ? 'cites' : 'sends the reader to') +
         ' ' + mark[i].cite + '. There is no ' + mark[i].cite + '.');
      comment();
    }

  for (i = 0; i < cycles.length; i++) { pf(cycles[i] + ' The argument is circular.'); comment(); }

  for (i = 0; i < nl; i++)
    if (mark[i].noContra) { pf('Line' + NB + (i + 1) + ' says contradiction. Nothing contradicts.'); comment(); }

  if (U.n) {
    pf(U.n === 1 ? 'Line' + U.text + ' uses a hypothesis that was never introduced.'
                 : 'Lines' + U.text + ' use hypotheses that were never introduced.');
    comment();
  }

  if (open) {
    var at0 = halted ? 'QED' : 'the end';
    pf(open === 1 ? 'One hypothesis is introduced and not discharged. It is still open at ' + at0 + '.'
                  : number(open, true) + ' hypotheses are introduced and not discharged. They are still open at ' + at0 + '.');
    comment();
  }

  for (i = 0; i < nl; i++)
    if (mark[i].prose && inert(mark[i].op)) {
      pf('Line' + NB + (i + 1) + ' is commentary, and its "' + mark[i].word + '" ' + performs(mark[i]) +
         '. Nothing follows from it.');
      comment();
    }

  for (l = 0; l < labels.length && unread; l++)
    if (labels[l].kind < NLAZY && M.skipTo[labels[l].line] !== NOWHERE && !mark[labels[l].line].called) {
      pf(KINDS[labels[l].kind] + (labels[l].id ? NB : '') + labels[l].id +
         ' is never cited, so its proof was not read.');
      comment();
    }

  if (pieces > 1) {
    var pw = number(pieces, false);
    pf('The citations fall into ' + pw + ' pieces. The paper may be ' + pw + ' papers.');
    comment();
  }

  for (i = 0; i < nl; i++)
    if (mark[i].discharged) {
      pf('Line' + NB + (i + 1) + ' discharges ' + number(mark[i].discharged, false) + ' ' +
         (mark[i].discharged === 1 ? 'hypothesis' : 'hypotheses') + ' by abstract nonsense.');
      comment();
    }

  if (E.n) {
    pf(E.n === 1 ? 'Line' + E.text + ' is left to the reader. Please include it.'
                 : 'Lines' + E.text + ' are left to the reader. Please include them.');
    comment();
  }
  if (N.n) {
    pf(N.n === 1 ? 'Line' + N.text + ' says what needs to be shown. Please show it.'
                 : 'Lines' + N.text + ' say what needs to be shown. Please show it.');
    comment();
  }
  if (W.n) {
    pf(W.n === 1 ? 'Line' + W.text + ' is well known. Please give a reference.'
                 : 'Lines' + W.text + ' are well known. Please give references.');
    comment();
  }
  if (C.n) {
    pf(C.n === 1 ? 'Line' + C.text + ' says "clearly". Please show the step.'
                 : 'Lines' + C.text + ' say "clearly". Please show the steps.');
    comment();
  }

  /* remarks: they weigh nothing */
  for (i = 0; i < nl; i++)
    if (mark[i].yoneda) {
      pf('Line' + NB + (i + 1) + ' invokes Yoneda. By Yoneda, this manuscript is isomorphic ' +
         'to every manuscript that prints the same thing. That is every manuscript.');
      comment();
    }
  for (i = 0; i < nl; i++)
    if (mark[i].similarOf) {
      pf('Line' + NB + (i + 1) + ' says "similarly". It is line' + NB + mark[i].similarOf + ' again.');
      comment();
    }
  var k = 0;
  for (i = 0; i < nl; i++)
    if (mark[i].cited && !mark[i].dangling) {
      pf('Line' + NB + (i + 1) + ' cites [' + mark[i].cited + ']. ' + UNREAD[(h + k++) % UNREAD.length]);
      comment();
    }

  if (who === REVIEWER_2) { pf('The author should cite the work of Reviewer 2.'); comment(); }

  if (commentNo === 0) { pf('I have no comments.'); pend(''); }

  pf('Recommendation: ' + DECISION_PROSE[decision] + '.');
  pend('');

  var c = Math.floor(h / (FIELD.length * WORN.length));
  return {
    report: out.join(''), code: decision, recommendation: DECISION_FORM[decision],
    confidential: who === REFEREE_1
      ? 'Confidential comments to the editor, on ' + base + ': ' + CONFIDENTIAL[c % CONFIDENTIAL.length]
      : 'Confidential comments to the editor, from Reviewer 2, on ' + base + ': ' +
        CONFIDENTIAL2[c % CONFIDENTIAL2.length]
  };
  }

  return { write: write, base: base };
}

function referee(source, name, library) { return review(source, name, library).write(REFEREE_1); }
function reviewer2(source, name, library) { return review(source, name, library).write(REVIEWER_2); }

/* the editor sees no reason to disagree with Reviewer 2 */
function editor(source, name, library) {
  var rv = review(source, name, library), r1 = rv.write(REFEREE_1), r2 = rv.write(REVIEWER_2);
  var d = Math.max(r1.code, r2.code), out = [], para = '';
  function pf(s) { para += s; }
  function pend() {
    var col = 0, first = true, txt = '';
    para.split(' ').forEach(function (w) {
      if (!w) return;
      if (!first && col + 1 + w.length > WIDTH) { txt += '\n'; col = 0; }
      else if (!first) { txt += ' '; col++; }
      txt += w; col += w.length; first = false;
    });
    out.push(txt + '\n\n');
    para = '';
  }
  var dash = '------------------------------------------------------------\n';
  out.push('EDITORIAL OFFICE -- DECISION\n', dash,
           'manuscript        ' + rv.base + '\n',
           'referee 1         ' + DECISION_FORM[r1.code] + '\n',
           'reviewer 2        ' + DECISION_FORM[r2.code] + '\n',
           'decision          ' + DECISION_FORM[d] + '\n', dash + '\n');
  pf('Dear Author,'); pend();
  pf('Your manuscript, ' + rv.base + ', has been reviewed by two referees. ' +
     'Neither could see the result. Nor could I.'); pend();
  pf('Referee 1 recommends ' + DECISION_PROSE[r1.code] + '. Reviewer 2 recommends ' +
     DECISION_PROSE[r2.code] + '. I see no reason to disagree with Reviewer 2.'); pend();
  pf(VERDICT[d]); pend();
  pf('The reports are enclosed.'); pend();
  pf('Yours sincerely,'); pend();
  pf('The Editor'); pend();
  return { report: out.join('') + r1.report + r2.report, code: d, recommendation: DECISION_FORM[d],
           confidential: r1.confidential + '\n' + r2.confidential };
}

/*
 * The librarian reads no paper. It reads the citations in every paper
 * it is given, and writes the citation index. papers: [{ name, text }].
 * The h-index is the exit code.
 */
function librarian(papers) {
  var shelf = [], from = [], to = [];
  papers.forEach(function (pp) {
    var b = pp.name.slice(pp.name.lastIndexOf('/') + 1);
    shelf.push(b);
    pp.text.split('\n').forEach(function (ln) {
      var r = matchLine(ln);
      if (!r.p || r.p.op !== OP.CITEFILE || to.length >= 4096) return;
      var s = r.at + r.p.text.length, t = ln.indexOf(']', s);
      if (t < 0) return;
      var c = ln.slice(s, Math.min(t, s + 63));
      for (var j = 0; j < to.length; j++) if (to[j] === c && from[j] === b) return;
      to.push(c);
      from.push(b);
    });
  });
  shelf.sort(function (x, y) { return x < y ? -1 : x > y ? 1 : 0; });

  function held(p) { return shelf.indexOf(p) >= 0; }
  function timesCited(p) { return to.filter(function (x) { return x === p; }).length; }
  function citedBy(p) {
    return shelf.filter(function (s) {
      return to.some(function (x, j) { return x === p && from[j] === s; });
    });
  }

  var cat = [], ncited = 0, nmissing = 0;
  to.forEach(function (c) {
    if (cat.indexOf(c) >= 0) return;
    cat.push(c);
    if (held(c)) ncited++; else nmissing++;
  });
  cat.sort(function (x, y) {
    var cx = timesCited(x), cy = timesCited(y);
    if (cx !== cy) return cy - cx;
    return x < y ? -1 : x > y ? 1 : 0;
  });

  var hIndex = 0, k = 0;
  cat.forEach(function (c) {
    if (!held(c)) return;
    k++;
    if (timesCited(c) >= k) hIndex = k;
  });

  var out = [], para = '';
  var dash = '------------------------------------------------------------\n';
  function pad(s, n) { while (s.length < n) s += ' '; return s; }
  function lpad(s, n) { while (s.length < n) s = ' ' + s; return s; }
  function pend() {
    var col = 0, first = true, txt = '';
    para.split(' ').forEach(function (w) {
      if (!w) return;
      if (!first && col + 1 + w.length > WIDTH) { txt += '\n'; col = 0; }
      else if (!first) { txt += ' '; col++; }
      txt += w; col += w.length; first = false;
    });
    out.push(txt + '\n\n');
    para = '';
  }

  out.push('LIBRARY -- CITATION INDEX\n', dash,
           'papers            ' + shelf.length + '\n',
           'citations         ' + to.length + '\n',
           'papers cited      ' + ncited + '\n',
           'not cited         ' + (shelf.length - ncited) + '\n',
           'not held          ' + nmissing + '\n',
           'h-index           ' + hIndex + '\n', dash);
  if (cat.length) {
    out.push('cited  paper               cited by\n');
    [0, 1].forEach(function (pass) {
      cat.forEach(function (c) {
        if (held(c) === !!pass) return;
        var by = citedBy(c);
        out.push(lpad(String(by.length), 5) + '  ' + pad(c, 19) + ' ' + by.join(', ') +
                 (pass ? '  (not held)' : '') + '\n');
      });
    });
    out.push(dash);
  }
  out.push('\n');

  var notCited = shelf.length - ncited;
  para = 'I have catalogued ' + shelf.length + ' paper' + (shelf.length === 1 ? '' : 's') +
         '. They make ' + number(to.length, false) + ' citation' + (to.length === 1 ? '' : 's') +
         ', to ' + number(cat.length, false) + ' paper' + (cat.length === 1 ? '' : 's') + '.';
  if (notCited)
    para += ' Of the ' + shelf.length + ' papers in the library, ' + notCited + ' ' +
            (notCited === 1 ? 'is' : 'are') + ' not cited.';
  pend();
  cat.forEach(function (c) {
    if (held(c)) return;
    para = '[' + c + '] is cited by ' + citedBy(c).join(', ') + '. The library does not hold it.';
    pend();
  });
  para = 'The h-index of the library is ' + hIndex + '.'; pend();
  para = 'I have counted the citations. I have not read the papers.'; pend();

  return { report: out.join(''), code: Math.min(hIndex, 255), hIndex: hIndex };
}

/*
 * Which words act, line by line: the phrase each line performs, and
 * whether it is an instruction or commentary that performs anyway.
 * For the desk, which marks them as they are typed.
 */
function lint(text) {
  return text.split('\n').map(function (ln) {
    var marks = [];
    var h = heading(ln);
    if (h) {
      var s0 = 0;
      while (isspace(ln[s0])) s0++;
      var e0 = s0 + KINDS[h.kind].length;
      if (h.id) e0 = ln.indexOf(h.id, e0) + h.id.length;
      marks.push({ start: s0, end: e0, kind: 'heading' });
    }
    if (tryDerived(ln) >= 0) {
      var p = ln.indexOf('R^'), e = p + 2;
      while (isdigit(ln[e])) e++;
      marks.push({ start: p, end: e, kind: 'derived' });
      return marks;
    }
    var r = matchLine(ln);
    if (r.p) {
      if (isInstruction(ln, r.at, r.p)) {
        marks.push({ start: r.at, end: r.at + r.p.text.length, kind: 'instruction' });
      } else {
        var s = r.at, f = r.at + r.p.text.length;
        while (s > 0 && isalpha(ln[s - 1])) s--;
        if (r.p.op === OP.CITEFILE) f = r.at + 2;   /* "by", not the paper */
        else while (f < ln.length && isalpha(ln[f])) f++;
        marks.push({ start: s, end: f, kind: 'friend' });
      }
    }
    return marks.filter(function (m, i, all) {
      return !all.some(function (o, j) { return j < i && m.start < o.end && o.start < m.end; });
    });
  });
}

var api = { run: run, referee: referee, reviewer2: reviewer2, editor: editor, librarian: librarian,
            lint: lint };
if (typeof module !== 'undefined' && module.exports) module.exports = api;
else root.sheaf = api;

})(this);
