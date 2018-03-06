BIN := bin/timewarp-test
CC := gcc

# Include all C files in the src directory
SRCS := $(shell find src -name '*.c')

# List of object files to link
OBJS := $(patsubst %.c,%.o,$(SRCS))

# Link rule
$(BIN): $(OBJS)
	$(CC) -o $@ $(OBJS)

# Include Dependency files
-include $(OBJS:.o=.d)

# Builds object file and generates dependency
%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o
	$(CC) -MM $(CFLAGS) $*.c > $*.d
	mv -f $*.d $*.d.tmp
	sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	rm $*.d.tmp