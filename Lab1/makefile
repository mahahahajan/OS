# target: dependencies
# <tab> rule

# make (without arguments) executes first rule in file
# Ideally, one target for every object file and a target for final binary. 
yash: yash.o
	gcc -g yash.o -o yash

yash.o: main.c
	gcc -g -c -o yash.o main.c 

clean:
	rm *.o yash