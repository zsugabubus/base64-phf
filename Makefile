b64phf : b64phf.c Makefile
	$(CC) -o $@ $< -O2 -g

run : b64phf
	./$<

clean :
	$(RM) b64phf

.PHONY : clean run
