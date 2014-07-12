CC=gcc
CFLAGS=-c -I./lib
LDFLAGS=-s
SOURCES=read_sd0.c lib/fat_access.c lib/fat_cache.c lib/fat_filelib.c lib/fat_format.c lib/fat_misc.c lib/fat_string.c lib/fat_table.c lib/fat_write.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=read_sd0

all: $(EXECUTABLE)
    
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f *.o ./lib/*.o
