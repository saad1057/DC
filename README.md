# BZip2 Data Compression - Phase 1

## Overview

This repository contains a **Phase 1** implementation of a BZip2-style compression pipeline in C.
It demonstrates block-based processing, RLE-1, Burrows-Wheeler Transform (BWT), and reverse decoding
to verify round-trip correctness (`input == decoded output`).

## Phase 1 Features

1. **Block Division**
   - Splits input files into blocks using a configurable `block_size`.
   - Handles file reassembly after processing.

2. **RLE-1 (Run-Length Encoding)**
   - Encodes repeated bytes into `[byte, count]` pairs.
   - Decodes encoded pairs back to original bytes.

3. **Burrows-Wheeler Transform (BWT)**
   - Applies BWT during encoding.
   - Stores `primary_index` per block for correct inverse BWT during decoding.

4. **Custom Encoded Container**
   - Writes a compact format with:
     - magic header (`BZS1`)
     - block count
     - per-block metadata (`orig_size`, `stage_size`, `primary_index`, `final_size`)
     - encoded payload bytes

5. **Config Loader**
   - Loads defaults from `config.c`.
   - Optionally overrides settings using `config.ini` if present.

6. **Terminal Block Previews**
   - Prints per-block input, encoded payload, and decoded previews in terminal.
   - Helps verify behavior without opening binary files repeatedly.

## Directory Structure

```text
DC/
├── Makefile          # Build and run automation
├── README.md         # Project documentation
├── main.c            # Pipeline controller (encode/decode + container I/O)
├── block.h           # Block data structures and APIs
├── block.c           # File-to-block splitting and reassembly
├── rle1.h            # RLE-1 function declarations
├── rle1.c            # RLE-1 encode/decode implementation
├── bwt.h             # BWT declarations and rotation struct
├── bwt.c             # BWT encode/decode implementation
├── config.h          # Config struct and loader declaration
├── config.c          # Config defaults + config.ini parser
├── sample_input.txt  # Sample input for quick testing
├── output.bin        # Encoded output (generated)
└── decoded.bin       # Decoded output (generated)
```

## Build and Run

### 1) Build

```bash
make
```

### 2) Encode sample input

```bash
make encode
```

### 3) Decode encoded output

```bash
make decode
```

### 4) Full round-trip test (recommended)

```bash
make roundtrip
```

This command:
- encodes `sample_input.txt`
- decodes `output.bin` into `decoded.bin`
- verifies both files are identical with `cmp`

### 5) Clean generated files

```bash
make clean
```

## Example Terminal Output

```text
./bzip2sim encode sample_input.txt
Block 0 encoded: orig=17 stage=34 primary=25
Input block 0 (17 bytes): banana and mango\n
Encoded payload block 0 (34 bytes): oadnnb m\nnn aaaag.................
Done. Output written to output.bin

./bzip2sim decode output.bin
Block 0 decoded: stage=34 final=17
Decoded block 0 (17 bytes): banana and mango\n
Done. Output written to decoded.bin

MATCH: decoded output equals input
```

## Configuration Notes

- If `config.ini` is missing, the program uses defaults from `config.c`.
- Default `block_size` is `500000`.
- Default pipeline enables `rle1_enabled = true`.

## Current Scope

This phase focuses on correctness and demonstrability of the core pipeline:
`Block Split -> RLE-1 -> BWT -> (store metadata) -> inverse BWT -> inverse RLE-1 -> Reassemble`.

Future phases can add MTF, RLE-2, Huffman coding, performance metrics, and benchmark automation.
