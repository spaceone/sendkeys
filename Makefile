xsendkey: xsendkey.c
	gcc -o xsendkey xsendkey.c -lX11
	gcc -o xsendword xsendword.c -lX11
clean:
	rm xsendkey xsendword
