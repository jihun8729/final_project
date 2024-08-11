# Makefile
CC = gcc
CFLAGS = -Wall -g
TARGET = p2p
SRCS = p2p_server.c
HEADERS = p2p_server.h
OBJS = $(SRCS:.c=.o)

# make  
all: $(TARGET)

# compile executable file 
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# compile object files 
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# make clean
clean:
	rm -f $(TARGET) $(OBJS)

# make rebuild
rebuild: clean all
