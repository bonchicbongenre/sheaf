CC      = cc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic
PROG    = sheaf
EXAMPLES = $(wildcard examples/*.sheaf)

.PHONY: all clean test vacuous derived

all: $(PROG)

$(PROG): sheaf.c
	$(CC) $(CFLAGS) -o $@ $<

test: vacuous derived

# The vacuous test. Every program's stdout must be empty. Every
# program's stdout is empty. This test has never failed and cannot
# fail. It is kept for its honesty.
vacuous: $(PROG)
	@printf 'Gamma (vacuous):\n'
	@pass=0; fail=0; \
	for f in $(EXAMPLES); do \
		name=$$(basename $$f .sheaf); \
		out=$$(./$(PROG) $$f 2>/dev/null); \
		if [ -z "$$out" ]; then \
			printf '  %-14s PASS\n' "$$name:"; \
			pass=$$((pass + 1)); \
		else \
			printf '  %-14s FAIL (output: %s)\n' "$$name:" "$$out"; \
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
		got=$$(./$(PROG) $$f 2>&1 >/dev/null); \
		want=$$(cat examples/$$name.derived 2>/dev/null); \
		if [ "$$got" = "$$want" ]; then \
			printf '  %-14s PASS\n' "$$name:"; \
			pass=$$((pass + 1)); \
		else \
			printf '  %-14s FAIL\n' "$$name:"; \
			printf '    want: %s\n' "$$want"; \
			printf '    got:  %s\n' "$$got"; \
			fail=$$((fail + 1)); \
		fi; \
	done; \
	printf '\n%d passed, %d failed\n' $$pass $$fail; \
	[ $$fail -eq 0 ]

clean:
	rm -f $(PROG)
