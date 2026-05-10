CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O3 -march=native -Iinclude
TARGET  = bzip2sim

# All C sources live under src/, headers under include/
SRC_DIR = src
SRCS    = $(SRC_DIR)/main.c     \
          $(SRC_DIR)/block.c    \
          $(SRC_DIR)/rle1.c     \
          $(SRC_DIR)/bwt.c      \
          $(SRC_DIR)/mtf.c      \
          $(SRC_DIR)/rle2.c     \
          $(SRC_DIR)/huffman.c  \
          $(SRC_DIR)/ans.c      \
          $(SRC_DIR)/config.c

.PHONY: all clean encode decode roundtrip windows

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Section 6.1: Windows Cross-Compilation Target
windows: $(SRCS)
	x86_64-w64-mingw32-gcc $(CFLAGS) -o $(TARGET).exe $(SRCS)

encode: $(TARGET)
	./$(TARGET) encode sample_input.txt

decode: $(TARGET)
	./$(TARGET) decode output.bin

roundtrip: $(TARGET)
	./$(TARGET) encode sample_input.txt
	./$(TARGET) decode output.bin
	cmp -s sample_input.txt decoded.bin && echo "MATCH: decoded output equals input" || echo "MISMATCH: decoded output differs"

clean:
	rm -f $(TARGET) $(TARGET).exe output.bin decoded.bin encoded.bin *.o
