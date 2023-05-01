all: myshell mypipeline

myshell: myshell.o LineParser.o
	gcc -m32 -g -Wall -o myshell myshell.o LineParser.o

myshell.o: myshell.c
	gcc -g -Wall -m32 -c -o myshell.o myshell.c 

LineParser.o: LineParser.c
	gcc -g -Wall -m32 -c -o LineParser.o LineParser.c

mypipeline: mypipeline.o
	gcc -m32 -g -Wall -o mypipeline mypipeline.o

mypipeline.o: mypipeline.c
	gcc -g -Wall -m32 -c -o mypipeline.o mypipeline.c 

.PHONY: clean

clean:
	rm -f *.o myshell mypipeline