import configparser
import csv
import os
import shutil
import subprocess
import sys
import time

# Weights for scoring formula: Score = w1*(C_ref/C) + w2*(S/S_ref)
W1 = 0.5
W2 = 0.5

# Output CSV path required by the spec (Section 7.3).
SPEC_CSV_PATH = os.path.join("results", "results.csv")
# Extra/full metrics (used internally for plotting & grading visibility).
FULL_CSV_PATH = os.path.join("results", "results_full.csv")
TIME_BIN = "/usr/bin/time"


def read_block_size(config_path="config.ini", default=500000):
    """Read block_size from config.ini so the CSV reflects the active setting."""
    parser = configparser.ConfigParser(inline_comment_prefixes=("#", ";"))
    try:
        parser.read(config_path)
        raw = parser.get("General", "block_size", fallback=str(default))
        return int(raw.split()[0])
    except Exception:
        return default


def run_with_metrics(cmd):
    """Run cmd; return (elapsed_seconds, max_rss_kb_or_None)."""
    if shutil.which(TIME_BIN):
        wrapped = [TIME_BIN, "-v", *cmd]
        start = time.time()
        proc = subprocess.run(wrapped, capture_output=True, text=True)
        elapsed = time.time() - start
        rss_kb = None
        for line in proc.stderr.splitlines():
            if "Maximum resident set size" in line:
                try:
                    rss_kb = int(line.split(":")[-1].strip())
                except ValueError:
                    rss_kb = None
                break
        return elapsed, rss_kb

    start = time.time()
    subprocess.run(cmd, capture_output=True)
    return time.time() - start, None

def get_bzip2_reference(input_path):
    """Run real bzip2 and return (compression_ratio_%, speed_MB/s)."""
    orig_size = os.path.getsize(input_path)
    bz2_path = input_path + ".bz2"

    if os.path.exists(bz2_path):
        os.remove(bz2_path)

    start = time.time()
    subprocess.run(["bzip2", "-k", "-9", input_path], capture_output=True)
    elapsed = time.time() - start

    if not os.path.exists(bz2_path):
        return None, None

    comp_size = os.path.getsize(bz2_path)
    os.remove(bz2_path)

    ratio = (1 - comp_size / orig_size) * 100
    speed = (orig_size / 1_000_000) / elapsed if elapsed > 0 else 0
    return round(ratio, 2), round(speed, 3)

def run_benchmark(input_dir):
    files = sorted([
        f for f in os.listdir(input_dir)
        if os.path.isfile(os.path.join(input_dir, f))
    ])
    results = []
    block_size = read_block_size()

    print(f"{'File':<25} {'Orig MB':>8} {'C%':>7} {'Cref%':>7} {'S MB/s':>8} {'Sref MB/s':>10} {'Mem KB':>8} {'Score':>7}")
    print("-" * 90)

    for file_name in files:
        input_path = os.path.join(input_dir, file_name)
        orig_size = os.path.getsize(input_path)
        orig_mb = orig_size / 1_000_000

        if os.path.exists("output.bin"):
            os.remove("output.bin")

        elapsed, mem_kb = run_with_metrics(["./bzip2sim", "encode", input_path])

        if not os.path.exists("output.bin"):
            print(f"{file_name:<25} FAILED (no output.bin)")
            continue

        comp_size = os.path.getsize("output.bin")
        ratio     = (1 - comp_size / orig_size) * 100
        speed     = orig_mb / elapsed if elapsed > 0 else 0

        c_ref, s_ref = get_bzip2_reference(input_path)

        if c_ref and c_ref > 0 and ratio > 0 and s_ref and s_ref > 0:
            score = W1 * (c_ref / ratio) + W2 * (speed / s_ref)
        else:
            score = None

        score_str = f"{score:.4f}" if score is not None else "N/A"
        mem_str   = f"{mem_kb}" if mem_kb is not None else "N/A"
        print(f"{file_name:<25} {orig_mb:>8.2f} {ratio:>7.1f} {c_ref or 0:>7.1f} "
              f"{speed:>8.2f} {s_ref or 0:>10.2f} {mem_str:>8} {score_str:>7}")

        results.append({
            "File":             file_name,
            "Size":             orig_size,
            "BlockSize":        block_size,
            "CompressionRatio": round(ratio, 2),
            "Time":             round(elapsed, 3),
            "Memory":           mem_kb if mem_kb is not None else "",
            # Extra fields kept for plotting / our own analysis (full CSV only).
            "_compressed_size": comp_size,
            "_ref_ratio":       c_ref,
            "_speed_mb_s":      round(speed, 3),
            "_ref_speed_mb_s":  s_ref,
            "_score":           round(score, 4) if score is not None else "",
        })

    if not results:
        print("No results.")
        return []

    os.makedirs(os.path.dirname(SPEC_CSV_PATH), exist_ok=True)

    spec_columns = ["File", "Size", "BlockSize", "CompressionRatio", "Time", "Memory"]
    with open(SPEC_CSV_PATH, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=spec_columns, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(results)
    print(f"\nSpec results written to {SPEC_CSV_PATH}")

    full_columns = [
        "File", "Size", "BlockSize", "CompressionRatio", "Time", "Memory",
        "_compressed_size", "_ref_ratio", "_speed_mb_s", "_ref_speed_mb_s", "_score",
    ]
    rename = {
        "_compressed_size": "CompressedSize",
        "_ref_ratio":       "RefCompressionRatio",
        "_speed_mb_s":      "Speed_MBps",
        "_ref_speed_mb_s":  "RefSpeed_MBps",
        "_score":           "Score",
    }
    pretty = []
    for r in results:
        pretty.append({rename.get(k, k): v for k, v in r.items() if k in full_columns})
    pretty_columns = [rename.get(k, k) for k in full_columns]
    with open(FULL_CSV_PATH, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=pretty_columns)
        writer.writeheader()
        writer.writerows(pretty)
    print(f"Full metrics written to {FULL_CSV_PATH}")

    return results


def _import_pyplot():
    """Load matplotlib using system packages only.

    A pip-installed NumPy 2.x under ~/.local is loaded before Ubuntu's NumPy 1.x and
    breaks apt's matplotlib (binary ABI mismatch: AttributeError _ARRAY_API).
    Temporarily dropping user site-packages from sys.path avoids that.
    """
    home_lib = os.path.expanduser("~/.local/lib/")
    saved = sys.path[:]
    try:
        sys.path[:] = [p for p in sys.path if not p.startswith(home_lib)]
        import matplotlib.pyplot as plt  # noqa: PLC0415

        return plt
    finally:
        sys.path[:] = saved


def plot_results(csv_file):
    try:
        plt = _import_pyplot()
    except ImportError:
        print(
            "matplotlib is not installed (or disk too full to install it); skipping graphs. "
            "CSV results were written successfully."
        )
        return
    except AttributeError as exc:
        # Still happens if system numpy/matplotlib mismatch remains.
        if "_ARRAY_API" in str(exc):
            print(
                "matplotlib cannot load (NumPy ABI mismatch with system matplotlib). "
                "Try: python3 -m pip uninstall -y numpy  "
                "(keep Ubuntu's python3-numpy), or run: PYTHONNOUSERSITE=1 python3 benchmark.py"
            )
            return
        raise

    def cell_float(row, key, default=0.0):
        v = row.get(key)
        if v is None or v == "" or v == "N/A":
            return default
        try:
            return float(v)
        except ValueError:
            return default

    def score_float(row):
        v = row.get("Score")
        if v is None or v == "" or v == "N/A":
            return float("nan")
        try:
            return float(v)
        except ValueError:
            return float("nan")

    with open(csv_file, newline="") as f:
        rows = list(csv.DictReader(f))

    if not rows:
        print("No rows to plot.")
        return

    x = range(len(rows))
    width = 0.35
    files = [r["File"] for r in rows]

    fig, axes = plt.subplots(1, 3, figsize=(18, 6))

    axes[0].bar(
        [i - width / 2 for i in x],
        [cell_float(r, "CompressionRatio") for r in rows],
        width,
        label="bzip2sim",
        color="skyblue",
    )
    axes[0].bar(
        [i + width / 2 for i in x],
        [cell_float(r, "RefCompressionRatio") for r in rows],
        width,
        label="bzip2 ref",
        color="steelblue",
    )
    axes[0].set_xticks(list(x))
    axes[0].set_xticklabels(files, rotation=45, ha="right")
    axes[0].set_ylabel("Compression Ratio (%)")
    axes[0].set_title("Compression Ratio vs bzip2 Reference")
    axes[0].legend()

    axes[1].bar(
        [i - width / 2 for i in x],
        [cell_float(r, "Speed_MBps") for r in rows],
        width,
        label="bzip2sim",
        color="salmon",
    )
    axes[1].bar(
        [i + width / 2 for i in x],
        [cell_float(r, "RefSpeed_MBps") for r in rows],
        width,
        label="bzip2 ref",
        color="firebrick",
    )
    axes[1].set_xticks(list(x))
    axes[1].set_xticklabels(files, rotation=45, ha="right")
    axes[1].set_ylabel("Speed (MB/s)")
    axes[1].set_title("Speed vs bzip2 Reference")
    axes[1].legend()

    scores = [score_float(r) for r in rows]
    axes[2].bar(files, scores, color="mediumseagreen")
    axes[2].axhline(y=1.0, color="red", linestyle="--", label="Score = 1.0 (matches bzip2)")
    axes[2].set_xticks(range(len(rows)))
    axes[2].set_xticklabels(files, rotation=45, ha="right")
    axes[2].set_ylabel("Score")
    axes[2].set_title(f"Performance Score (w1={W1}, w2={W2})")
    axes[2].legend()

    plt.tight_layout()
    out_png = os.path.join("results", "compression_results.png")
    plt.savefig(out_png, dpi=150)
    plt.show()
    print(f"Graph saved to {out_png}")

if __name__ == "__main__":
    bench_dir = "./benchmarks"
    if not os.path.exists(bench_dir):
        os.makedirs(bench_dir)
        print(f"Created {bench_dir}/. Put test files in there and run again.")
    else:
        results = run_benchmark(bench_dir)
        if results:
            plot_results(FULL_CSV_PATH)
