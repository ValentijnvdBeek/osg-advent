
chimera: chimera.c
	gcc chimera.c -o chimera -g -O0

run: ./chimera
	./chimera thread

strace: ./chimera
	strace -ff ./chimera thread

gdb: ./chimera
	gdb --args ./chimera fork

valgrind: ./chimera
	valgrind ./chimera
