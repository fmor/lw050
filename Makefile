CFLAGS=-Wall -g -o0
LDFLAGS=-lcurl

all : lw050

lw050 : lw050.o 
		gcc -o $@ $(LDFLAGS)  $^

%.o : %.c
		gcc $(CFLAGS) -o $@ -c  $<

clean :
		rm -f lw050 *.o 

install : all
		cp -a lw050 /usr/local/bin

