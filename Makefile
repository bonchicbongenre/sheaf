CC      = cc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic
PROG    = sheaf
EXAMPLES = $(wildcard examples/*.sheaf)

.PHONY: all clean test

all: $(PROG)

$(PROG): sheaf.c
	$(CC) $(CFLAGS) -o $@ $<

test: $(PROG)
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
	printf '\n%d passed, %d failed\n' $$pass $$fail; \
	[ $$fail -eq 0 ]

clean:
	rm -f $(PROG)
