CC      = cc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic
PROG    = sheaf
REFEREE = referee
SHEAFC  = sheafc
EXAMPLES = $(wildcard examples/*.sheaf)
SUBMISSIONS = $(wildcard submissions/*.sheaf)

LETTERS = $(wildcard examples/*.letter)

.PHONY: all clean test vacuous derived reports letters compiled rel quines fixpoint site site-test

all: $(PROG) $(REFEREE) $(SHEAFC)

$(PROG): sheaf.c stalk.h
	$(CC) $(CFLAGS) -o $@ $<

# One program, three names: referee, reviewer2, editor.
$(REFEREE): referee.c stalk.h
	$(CC) $(CFLAGS) -o $@ $<
	ln -sf referee reviewer2
	ln -sf referee editor

$(SHEAFC): sheafc.c stalk.h
	$(CC) $(CFLAGS) -o $@ $<

test: vacuous derived reports letters compiled rel quines

# The vacuous test. Every program's stdout must be empty. Every
# program's stdout is empty. This test has never failed and cannot
# fail. It is kept for its honesty. In every test the reader gives
# nothing: stdin is /dev/null.
vacuous: $(PROG)
	@printf 'Gamma (vacuous):\n'
	@pass=0; fail=0; \
	for f in $(EXAMPLES); do \
		name=$$(basename $$f .sheaf); \
		out=$$(./$(PROG) $$f < /dev/null 2>/dev/null); \
		if [ -z "$$out" ]; then \
			printf '  %-16s PASS\n' "$$name:"; \
			pass=$$((pass + 1)); \
		else \
			printf '  %-16s FAIL (output: %s)\n' "$$name:" "$$out"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	printf '\n%d passed, %d failed\n\n' $$pass $$fail; \
	[ $$fail -eq 0 ]

# The derived test. stdout is empty, so the only witness is stderr.
# Each program's stderr must equal examples/NAME.derived, or be empty
# where there is no such file. The obstruction is checked. The answer
# is not.
derived: $(PROG)
	@printf 'R^i (derived):\n'
	@pass=0; fail=0; \
	for f in $(EXAMPLES); do \
		name=$$(basename $$f .sheaf); \
		got=$$(./$(PROG) $$f < /dev/null 2>&1 >/dev/null); \
		want=$$(cat examples/$$name.derived 2>/dev/null); \
		if [ "$$got" = "$$want" ]; then \
			printf '  %-16s PASS\n' "$$name:"; \
			pass=$$((pass + 1)); \
		else \
			printf '  %-16s FAIL\n' "$$name:"; \
			printf '    want: %s\n' "$$want"; \
			printf '    got:  %s\n' "$$got"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	printf '\n%d passed, %d failed\n\n' $$pass $$fail; \
	[ $$fail -eq 0 ]

# The reports. Each manuscript's report must equal NAME.report, and
# the referee's exit code must be the recommendation on it. The
# rejections are kept. A manuscript that is corrected will no longer
# be rejected, and this test will fail.
reports: $(REFEREE)
	@printf 'Referee (reports):\n'
	@pass=0; fail=0; \
	for f in $(EXAMPLES) $(SUBMISSIONS); do \
		name=$$(basename $$f .sheaf); \
		golden=$$(dirname $$f)/$$name.report; \
		got=$$(./$(REFEREE) $$f 3>&- 2>&1 >/dev/null); code=$$?; \
		want=$$(cat $$golden 2>/dev/null); \
		case "$$(sed -n 's/^recommendation *//p' $$golden 2>/dev/null)" in \
			ACCEPT) wcode=0 ;; \
			"MINOR REVISION") wcode=1 ;; \
			"MAJOR REVISION") wcode=2 ;; \
			REJECT) wcode=3 ;; \
			*) wcode=none ;; \
		esac; \
		if [ "$$got" = "$$want" ] && [ "$$code" = "$$wcode" ]; then \
			printf '  %-16s PASS  (%s)\n' "$$name:" \
				"$$(sed -n 's/^recommendation *//p' $$golden)"; \
			pass=$$((pass + 1)); \
		else \
			printf '  %-16s FAIL  (exit %s, want %s)\n' "$$name:" $$code $$wcode; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	printf '\n%d passed, %d failed\n' $$pass $$fail; \
	[ $$fail -eq 0 ]

# The letters. The editor's decision letter, with both reports
# enclosed, must equal NAME.letter, and the exit code the decision.
letters: $(REFEREE)
	@printf '\nEditor (letters):\n'
	@pass=0; fail=0; \
	for g in $(LETTERS); do \
		name=$$(basename $$g .letter); \
		got=$$(./editor examples/$$name.sheaf 3>&- 2>&1 >/dev/null); code=$$?; \
		case "$$(sed -n 's/^decision *//p' $$g)" in \
			ACCEPT) wcode=0 ;; "MINOR REVISION") wcode=1 ;; \
			"MAJOR REVISION") wcode=2 ;; REJECT) wcode=3 ;; *) wcode=none ;; \
		esac; \
		if [ "$$got" = "$$(cat $$g)" ] && [ "$$code" = "$$wcode" ]; then \
			printf '  %-16s PASS  (%s)\n' "$$name:" "$$(sed -n 's/^decision *//p' $$g)"; \
			pass=$$((pass + 1)); \
		else \
			printf '  %-16s FAIL\n' "$$name:"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	printf '\n%d passed, %d failed\n' $$pass $$fail; \
	[ $$fail -eq 0 ]

# The compiled test. sheafc folds each manuscript into C; the C is
# compiled and run, and must leak what the interpreter leaks, and
# print what it prints. A manuscript that asks the reader cannot be
# folded, and is refused. A manuscript that does not terminate is
# compiled into one that does (C11 6.8.5p6).
compiled: $(SHEAFC)
	@printf '\nsheafc (compiled):\n'
	@dir=$$(mktemp -d); pass=0; fail=0; \
	for f in $(EXAMPLES) $(SUBMISSIONS); do \
		name=$$(basename $$f .sheaf); \
		if ./$(SHEAFC) $$f > $$dir/p.c 2> $$dir/warn; then \
			$(CC) $(CFLAGS) -o $$dir/p $$dir/p.c; \
			out=$$($$dir/p < /dev/null 2>/dev/null); \
			got=$$($$dir/p < /dev/null 2>&1 >/dev/null); \
			want=$$(cat $$(dirname $$f)/$$name.derived 2>/dev/null); \
			if [ -z "$$out" ] && [ "$$got" = "$$want" ]; then \
				if [ -s $$dir/warn ]; then note='  (terminates; the interpreted proof does not)'; else note=''; fi; \
				printf '  %-16s PASS%s\n' "$$name:" "$$note"; \
				pass=$$((pass + 1)); \
			else \
				printf '  %-16s FAIL\n' "$$name:"; \
				fail=$$((fail + 1)); \
			fi; \
		elif grep -qi 'left to the reader' $$f; then \
			printf '  %-16s PASS  (refused: the reader is not a constant)\n' "$$name:"; \
			pass=$$((pass + 1)); \
		else \
			printf '  %-16s FAIL  (refused)\n' "$$name:"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	rm -f $$dir/p.c $$dir/p $$dir/warn; \
	rmdir $$dir; \
	printf '\n%d passed, %d failed\n' $$pass $$fail; \
	[ $$fail -eq 0 ]

# The REL test. Each manuscript is typed into the REL on stdin, and
# must leak what it leaks when read from its file.
rel: $(PROG)
	@printf '\nREL (typed):\n'
	@pass=0; fail=0; \
	for f in $(EXAMPLES); do \
		name=$$(basename $$f .sheaf); \
		out=$$(./$(PROG) < $$f 2>/dev/null); \
		got=$$(./$(PROG) < $$f 2>&1 >/dev/null); \
		want=$$(cat examples/$$name.derived 2>/dev/null); \
		if [ -z "$$out" ] && [ "$$got" = "$$want" ]; then \
			pass=$$((pass + 1)); \
		else \
			printf '  %-16s FAIL\n' "$$name:"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	printf '  %d typed, %d failed\n' $$pass $$fail; \
	[ $$fail -eq 0 ]

# The quine test. A quine prints its own source. Every program prints
# nothing, so a program is a quine exactly when its source is nothing.
quines: $(PROG)
	@printf '\nQuines:\n'
	@n=0; bad=0; \
	for f in $(EXAMPLES); do \
		name=$$(basename $$f .sheaf); \
		if ./$(PROG) $$f < /dev/null 2>/dev/null | cmp -s - $$f; then \
			printf '  %-16s a quine\n' "$$name:"; \
			n=$$((n + 1)); \
			[ -s $$f ] && bad=$$((bad + 1)); \
		else \
			[ -s $$f ] || bad=$$((bad + 1)); \
		fi; \
	done; \
	printf '  %d quine(s), every one of them empty; %d exception(s)\n' $$n $$bad; \
	[ $$bad -eq 0 ]

# The fixed point of peer review. A report is prose, so it is a
# manuscript. The referee reads its own report, and the report on
# that, until a report comes round again: once (a fixed point) or
# after some rounds (a cycle). Where each review ends is kept in
# fixpoint/NAME.report.
fixpoint: $(REFEREE)
	@printf 'Referee (fixed point):\n'
	@mkdir -p fixpoint; \
	dir=$$(mktemp -d); \
	for f in $(EXAMPLES) $(SUBMISSIONS); do \
		name=$$(basename $$f .sheaf); \
		mkdir $$dir/0; \
		./$(REFEREE) $$f 3>&- 2> $$dir/0/report.sheaf; \
		n=0; hit=; \
		while [ $$n -lt 16 ] && [ -z "$$hit" ]; do \
			m=$$((n + 1)); \
			mkdir $$dir/$$m; \
			./$(REFEREE) $$dir/$$n/report.sheaf 3>&- 2> $$dir/$$m/report.sheaf; \
			j=0; \
			while [ $$j -le $$n ]; do \
				if cmp -s $$dir/$$j/report.sheaf $$dir/$$m/report.sheaf; then hit=$$j; break; fi; \
				j=$$((j + 1)); \
			done; \
			n=$$m; \
		done; \
		if [ -z "$$hit" ]; then \
			printf '  %-16s nothing came round in 16 rounds; %d lines, then %d\n' \
				"$$name:" $$(wc -l < $$dir/$$((n - 1))/report.sheaf) \
				$$(wc -l < $$dir/$$n/report.sheaf); \
		elif [ $$hit -eq $$((n - 1)) ]; then \
			printf '  %-16s fixed at round %d\n' "$$name:" $$hit; \
		else \
			printf '  %-16s a cycle of period %d, from round %d\n' "$$name:" $$((n - hit)) $$hit; \
		fi; \
		cp $$dir/$$n/report.sheaf fixpoint/$$name.report; \
		rm -f $$dir/*/report.sheaf; \
		rmdir $$dir/*; \
	done; \
	rmdir $$dir

# The site, theresultisonthenext.page, is docs/. The browser runs a port
# of the machine and the referee. `site` bundles the corpus into the
# page; `site-test` checks the port against every golden file, byte for
# byte. Both need node.
site:
	node tools/site-examples.js

site-test:
	node tools/site-test.js

clean:
	rm -f $(PROG) $(REFEREE) reviewer2 editor $(SHEAFC) fixpoint/*.report
