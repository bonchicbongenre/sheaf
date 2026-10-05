// Check the browser port against the C goldens, byte for byte.
// usage: node tools/site-test.js
'use strict';
const fs = require('fs');
const path = require('path');
const sheaf = require('../docs/sheaf.js');

const root = path.join(__dirname, '..');
let pass = 0, fail = 0;
const read = (p) => (fs.existsSync(p) ? fs.readFileSync(p, 'utf8') : '');

for (const dir of ['examples', 'submissions']) {
  for (const f of fs.readdirSync(path.join(root, dir)).sort()) {
    if (!f.endsWith('.sheaf')) continue;
    const name = f.slice(0, -6);
    const text = fs.readFileSync(path.join(root, dir, f), 'utf8');

    const r = sheaf.referee(text, f);
    const want = read(path.join(root, dir, name + '.report'));
    let ok1 = r.report === want;

    // the editor's letter, where there is one
    const letter = path.join(root, dir, name + '.letter');
    if (fs.existsSync(letter) && sheaf.editor(text, f).report !== read(letter)) {
      ok1 = false;
      console.log('  ' + name + ': the letter differs');
    }

    // the author, with a reader who gives nothing; submissions do not end
    let ok2 = true;
    if (dir === 'examples') {
      const run = sheaf.run(text, '');
      ok2 = run.finished && run.stdout === '' && run.stderr === read(path.join(root, dir, name + '.derived'));
    }

    if (ok1 && ok2) { pass++; console.log('  ' + (name + ':').padEnd(16) + ' PASS'); }
    else {
      fail++;
      console.log('  ' + (name + ':').padEnd(16) + ' FAIL' + (ok1 ? '' : ' (report)') + (ok2 ? '' : ' (author)'));
      if (!ok1) {
        const a = r.report.split('\n'), b = want.split('\n');
        for (let i = 0; i < Math.max(a.length, b.length); i++)
          if (a[i] !== b[i]) { console.log('    line ' + (i + 1) + '\n    want: ' + b[i] + '\n    got:  ' + a[i]); break; }
      }
    }
  }
}
console.log('\n' + pass + ' passed, ' + fail + ' failed');
process.exit(fail ? 1 : 0);
