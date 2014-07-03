SRC=$(wildcard *.ronn)
MANPAGES=$(SRC:.ronn=.2)
	
all: $(SRC)
	ronn -r $^

clean:
	rm *.2 *.2.html

