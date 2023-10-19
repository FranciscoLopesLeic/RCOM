CFLAGS = -Wall

SRC = $(wildcard *.c)

LIBS =

download: $(SRC)
	gcc -o $@ $^ $(LIBS) $(CFLAGS)

clean:
	rm ./download
