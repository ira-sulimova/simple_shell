CC=gcc

simple_shell: 
	$(CC) -o simple_shell simple_shell.c 

clean:
	rm simple_shell