# CC = compiler to use 
CC = clang

# -std=c99: Use the C99 language standard
# -Wall -Wextra: Usefulle compiler warnings
# -g: Include information for debugging
CFLAGS = -std=c99 -Wall -Wextra -g -Iinclude

TARGET = app.exe

# SOURCES: .c files to compile
SOURCES = main.c 

# HEADERS: Headers to watch for changes
HEADERS = $(wildcard  include/*.h)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)
