.PHONY: example
example: example/build/example
	./example/build/example

.PHONY: clean
clean:
	$(RM) example/nob
	$(RM) -r example/build

.PHONY: format
format:
	clang-format -i nob.h example/nob.c example/main.c

example/build/example: example/nob
	(cd example && ./nob)

example/nob: example/nob.c nob.h
	cc -o $@ $< -std=c99 -Wall -Wextra -Wpedantic -g3 -fsanitize=address,undefined
