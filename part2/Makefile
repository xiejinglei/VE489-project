# use c++ 11 standard here (c++17 would be fine?)
CC=g++ -g -Wall -std=c++11

# List of source files for iPerfer
FS_SOURCES=ftrans.cc

# Generate the names of the iPerfer's object files
FS_OBJS=${FS_SOURCES:.cc=.o}

all: ftrans

ftrans: ${FS_OBJS}
	${CC} -o $@ $^

# Generic rules for compiling a source file to an object file
%.o: %.cpp
	${CC} -c $<
%.o: %.cc
	${CC} -c $<

clean:
	rm -f ${FS_OBJS} ftrans

