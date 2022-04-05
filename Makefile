CC = gcc

all: 
	${CC} -s -O3 -DNDBUG tsudpsend.c -o tsudpsend

clean: 
	rm tsudpsend
