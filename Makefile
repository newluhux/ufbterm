CFLAGS += -Wall -Wextra -O3 -g3

all:	ufbterm

ufbdesk:
	$(CC) $(CFLAGS) ufbterm.c -o ufbterm

clean:
	rm -vf *~ *.o *.out ufbterm
