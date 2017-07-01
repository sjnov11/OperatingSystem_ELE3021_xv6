# Compiler and Compile options.
CC = gcc
CFLAGS = -g -Wall

# Macros specifying path for compile.
SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

TARGET = shell
$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS)

# Delete binary & object files
clean:
	rm $(TARGET) $(OBJS)
