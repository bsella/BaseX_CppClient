all: example

example: readstring.o md5.o basexdbc.o example.o
	cc readstring.o md5.o basexdbc.o example.o -o example -lcrypto -lSDL2 -lSDL2_net

%.o: %.cpp Makefile
	cc -c $< -o $@
clean:
	rm *.o example