# target: dependencies
# <tab> rule

# make (without arguments) executes first rule in file
# Ideally, one target for every object file and a target for final binary. 

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
$(info This is my Ubuntu)
yash: main.c
	gcc -g  -L/usr/include/readline -lreadline -o yash main.c 

clean:
	rm *.o yash
endif
ifeq ($(UNAME_S),Darwin)
$(info This is my local machine)
yash: yash.o
	gcc -g -lreadline yash.o -o yash

yash.o: main.c
	gcc -g   -c -o yash.o main.c 

clean:
	rm *.o yash

endif



