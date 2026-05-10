# bzip2sim — A BZip2-style Compressor

This is our implementation of a simplified **BZip2** compression pipeline,
written in C for the Data Compression course. It does not aim to be a
drop-in replacement for `bzip2`, but it follows the same overall idea:
break the file into blocks, run a chain of well-known transforms, and
finish with an entropy coder. Decompression undoes every stage in
reverse and the original file comes back byte-for-byte.

The project also ships with a small Python script that benchmarks the
compressor against the real `bzip2`, dumps a CSV, and (if matplotlib is
available) draws a few comparison graphs.

---

## Pipeline at a glance

```
encode:  file ──► blocks ──► RLE-1 ──► BWT ──► MTF ──► RLE-2 ──► Entropy ──► output.bin
decode:                               (everything above, inverted)
```

For the entropy stage we support both **canonical Huffman** (the default
expected by the spec) and **ANS** (Asymmetric Numeral Systems) as an
extra-credit option. BWT can be done either with a classic
**rotation matrix** approach or a **suffix-array** based one. All of
this is configurable from `config.ini`, no recompilation needed.

---

## Project layout

```
project-bzip2/
├── src/                # all .c source files
│   ├── main.c          # CLI driver + per-block pipeline
│   ├── block.c         # block division / reassembly
│   ├── rle1.c          # first run-length encoder (raw bytes)
│   ├── bwt.c           # Burrows–Wheeler transform (matrix + suffix array)
│   ├── mtf.c           # move-to-front transform
│   ├── rle2.c          # second RLE specialised for MTF output
│   ├── huffman.c       # canonical Huffman encoder/decoder
│   ├── ans.c           # ANS entropy coder (extra credit)
│   └── config.c        # config.ini parser
├── include/            # all public headers, mirrored from src/
├── benchmarks/         # Canterbury / Calgary / Silesia + custom test files
├── results/            # results.csv, results_full.csv, graphs (generated)
├── Makefile            # Linux + Windows cross-compile targets
├── config.ini          # runtime configuration
├── benchmark.py        # benchmarking + plotting script
├── sample_input.txt    # tiny file used by `make roundtrip`
└── README.md
```

---

## Building

### Linux

```bash
make            # builds ./bzip2sim
make -B         # force a clean rebuild
```

### Windows (cross-compile from Linux)

You need the MinGW-w64 toolchain installed (`x86_64-w64-mingw32-gcc`).

```bash
make windows    # produces bzip2sim.exe
```

### Cleaning up

```bash
make clean      # removes the binary and any generated *.bin files
```

The build is C11, with `-Wall -Wextra -O3 -march=native`. Two harmless
warnings still show up (an `fread` whose error path is intentionally
ignored, and a `strncpy` truncation warning in the config parser). Both
are non-fatal.

---

## Running

### Encode / decode a single file

```bash
./bzip2sim encode <input-file>     # writes output.bin
./bzip2sim decode output.bin       # writes decoded.bin
```

The driver prints a per-stage size trace for every block, which is very
useful when you want to *see* what each transform does:

```
[Block 0] Input          in=47 bytes,  out=47 bytes
[Block 0] RLE-1          in=47 bytes,  out=47 bytes
[Block 0] BWT            in=47 bytes,  out=47 bytes
[Block 0] MTF            in=47 bytes,  out=47 bytes
[Block 0] RLE-2          in=47 bytes,  out=41 bytes
[Block 0] Entropy        in=41 bytes,  out=540 bytes
```

(For very small inputs the entropy stage can grow the data because of
the model headers — that is expected; on real benchmark files we get
proper compression.)

### Quick correctness check

```bash
make roundtrip
```

This encodes `sample_input.txt`, decodes the result, and runs `cmp` to
make sure the decoded file matches the original byte-for-byte.

---

## Configuration

Everything tunable lives in `config.ini`:

```ini
[General]
block_size      = 900000        # 100 KB – 900 KB
rle1_enabled    = true
bwt_type        = suffix_array  # matrix | suffix_array
mtf_enabled     = true
rle2_enabled    = true
entropy_type    = ans           # huffman | ans
huffman_enabled = false

[Performance]
benchmark_mode  = false
output_metrics  = true

[Paths]
input_directory  = ./benchmarks/
output_directory = ./results/
```

Toggle a stage off (e.g. `mtf_enabled = false`) and the pipeline will
skip it on both encode and decode. This is handy for ablation studies
and for debugging individual transforms in isolation.

---

## Benchmarking

```bash
python3 benchmark.py
```

The script:

1. Walks `./benchmarks/` and runs `./bzip2sim encode` on every file.
2. Runs the system `bzip2 -k -9` on the same file as a reference.
3. Measures wall-clock time and (on Linux, via `/usr/bin/time -v`)
   the peak resident memory of `bzip2sim`.
4. Computes the spec score:

   `Score = w1 · (C_ref / C) + w2 · (S / S_ref)`,  with `w1 = w2 = 0.5`.

5. Writes two files:
   - `results/results.csv` — exactly the columns required by the brief:
     `File, Size, BlockSize, CompressionRatio, Time, Memory`.
   - `results/results_full.csv` — the same rows plus reference numbers
     and the score, used by the plotter.

If `matplotlib` is installed, it also produces
`results/compression_results.png` with three bar charts:
compression ratio vs `bzip2`, speed vs `bzip2`, and the per-file score
with a dashed line at `score = 1.0` (which marks parity with `bzip2`).

If matplotlib is not installed (or its NumPy version clashes with the
system one) the script still finishes successfully and writes the CSVs
— it just skips the plotting step and prints a hint.

---

## Datasets used

We tested against the standard public corpora plus a couple of larger
custom files:

- **Canterbury Corpus** (`alice29.txt`, `cp.html`, `fields.c`, …)
- **Calgary Corpus** (`bib`, `book1`, `paper1`, `progc`, …)
- **Silesia Corpus** (`dickens`, `mozilla`, `nci`, `webster`, …)
- **Large text and binary files** (10 MB – ~50 MB)

All of them are included under `benchmarks/`.

---

## What is implemented

| Stage  | Spec component      | Status                                             |
| ------ | ------------------- | -------------------------------------------------- |
| 1      | Block division      | Yes — configurable size, large files supported     |
| 1      | RLE-1               | Yes — encode + decode                              |
| 1      | BWT                 | Yes — both matrix and suffix-array variants        |
| 2      | MTF                 | Yes — encode + decode                              |
| 2      | RLE-2               | Yes — tuned for MTF output                         |
| 3      | Canonical Huffman   | Yes — header stores code lengths only              |
| Extra  | Suffix-array BWT    | Yes (Section 8.2)                                  |
| Extra  | ANS entropy coding  | Yes (Section 8.3)                                  |
| Build  | Linux + Windows     | Yes — `make` and `make windows`                    |
| Bench  | results.csv + plots | Yes — `benchmark.py` + matplotlib                  |
