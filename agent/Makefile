# Compiler configuration
CC = x86_64-w64-mingw32-gcc
CFLAGS = -Wall -Wextra -O2

# Linker flags (Crucial for Windows networking)
LDFLAGS = -lwininet

# Target executable name
TARGET = agent.exe

# Source files
SRC = main.c

# Default rule executed when running 'make'
all: $(TARGET)

# Rule to build the target executable
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

# Rule to clean up build artifacts
clean:
	rm -f $(TARGET)