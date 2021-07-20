CC = gcc

CFLAGS  = -g -Wall -std=c99 -D_GNU_SOURCE -pthread

# the build target executable:
SR = sr
MITM = mitm

all: $(SR) $(MITM)

# assume the C code for TARGET is TARGET.c
$(SR): $(SR).c
	$(CC) $(CFLAGS) -o $(SR) $(SR).c

$(MITM): $(MITM).c
	$(CC) $(CFLAGS) -o $(MITM) $(MITM).c

clean:
	$(RM) $(SR) $(MITM)
