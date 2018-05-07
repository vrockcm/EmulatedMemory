all:
			gcc main.c -o main -lpthread
			gcc mem.c -o mem -lpthread
clean:
			rm -f main
			rm -f mem
