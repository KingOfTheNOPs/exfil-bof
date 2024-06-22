all:
	x86_64-w64-mingw32-gcc -c exfil.c -o exfil.o
	x86_64-w64-mingw32-strip --strip-unneeded exfil.o 	
clean:
	rm exfil.o 
