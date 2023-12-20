all: util.c kernel.c main.c
	gcc -o Kernel-Paging-Unit util.c kernel.c main.c

clean:
	rm Kernel-Paging-Unit
