all: example

CPP=g++
CC=gcc

example: example.cpp basexcpp.a
	${CPP} -o example example.cpp basexcpp.a -lcrypto -lSDL2 -lSDL2_net

basexcpp.a: BaseXSession.o basexdbc.o readstring.o md5.o
	ar r $@ $?

%.o: %.cpp Makefile
	${CPP} -c $< -o $@

%.o: %.c Makefile
	${CC} -c $< -o $@

clean:
	rm *.o *.a example