clean:
	$(RM) example/nob
	$(RM) -r example/build

format:
	clang-format -i nob.h example/nob.c example/main.c

example/main: example/nob
	(cd example && ./nob)

example/nob: example/nob.c nob.h
	cc -o $@ $< -Wall -Wextra -Wpedantic

.PHONY: clean format
