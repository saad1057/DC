CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = bzip2sim
SRCS = main.c block.c rle1.c bwt.c config.c

.PHONY: all clean encode decode roundtrip

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

encode: $(TARGET)
	./$(TARGET) encode sample_input.txt

decode: $(TARGET)
	./$(TARGET) decode output.bin

roundtrip: $(TARGET)
	./$(TARGET) encode sample_input.txt
	./$(TARGET) decode output.bin
	cmp -s sample_input.txt decoded.bin && echo "MATCH: decoded output equals input" || echo "MISMATCH: decoded output differs"

clean:
	rm -f $(TARGET) output.bin decoded.bin encoded.bin
