CC = gcc

all: tsudpreceive tsudpsend

tsudpsend: 
	${CC} -s -O3 -DNDBUG tsudpsend.c -o tsudpsend

tsudpreceive:
	${CC} -s -O3 -DNDBUG tsudpreceive.c -o tsudpreceive
 
clean: 
	rm tsudpsend
	rm tsudpreceive