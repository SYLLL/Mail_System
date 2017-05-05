CC=gcc
LD=gcc
CFLAGS=-g -Wall -O2
CPPFLAGS=-I. -I/home/cs437/exercises/ex3/include
SP_LIBRARY_DIR=/home/cs437/exercises/ex3

all: sp_user client server class_user

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

sp_user:  $(SP_LIBRARY_DIR)/libspread-core.a user.o
	$(LD) -o $@ user.o  $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

client: $(SP_LIBRARY_DIR)/libspread-core.a client.o
	$(LD) -o $@ client.o $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

server: $(SP_LIBRARY_DIR)/libspread-core.a server.o
	$(LD) -o $@ server.o $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

class_user:  $(SP_LIBRARY_DIR)/libspread-core.a class_user.o
	$(LD) -o $@ class_user.o  $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a

clean:
	rm -f *.o sp_user client server class_user


