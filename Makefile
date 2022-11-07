CC = gcc

all: tsudpreceive tsudpsend tsudpsendSleep

tsudpsend: 
	${CC} -s -O3 -DNDBUG tsudpsend.c -o tsudpsend

tsudpreceive:
	${CC} -s -O3 -DNDBUG tsudpreceive.c -o tsudpreceive

tsudpsendSleep: 
	${CC} -s -O3 -DNDBUG tsudpsendSleep.c -o tsudpsendSleep
 
clean: 
	rm tsudpsend
	rm tsudpreceive
	rm tsudpsendSleep
