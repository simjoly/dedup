# dedup : remove duplicates in fastq files

This tool removes duplicate read pairs from paired-end FASTQ files. It can handle a random index incorporated in the PCR product during the library construction.

It supports multiple backends for duplicate detection: in-memory, Bloom filter, and SQLite database.


## Features

- Handles **paired-end FASTQ** files (gzipped).
- Supports duplicate detection using:
  - **In-memory hash set** (fastest, but high RAM usage).
  - **Bloom filter** (low memory, allows false positives).
  - **SQLite** (disk-based, low memory).
- Barcode handling:
  - From a separate index FASTQ file (`--index`).
  - From the read name itself (`--barcode-in-name`).
  - Pasted to one of the reads.


## Compilation

### Dependencies
You need the following libraries:

- `zlib` (for `.gz` FASTQ support)
- `sqlite3`
- `openssl` (for SHA-256 hashing)
- C++17 compiler (`g++` or `clang++`)

### Linux (Debian/Ubuntu)
```bash
sudo apt-get update
sudo apt-get install g++ make zlib1g-dev libsqlite3-dev libssl-dev
make
```

### macOS (Homebrew)
```bash
brew install zlib sqlite openssl
make
```


## Usage

```bash
./dedup --read1 R1.fastq.gz --read2 R2.fastq.gz [options]
```

### Options

- `--read1 <read1file>` : Input FASTQ (read 1, gzipped).
- `--read2 <read2file>` : Input FASTQ (read 2, gzipped).
- `--index <indexfile>` : Optional index FASTQ file.
- `--barcode-in-name` : Extract barcode from sequence name in read1.
- `--use-memory` : Use in-memory hash set (fast, high RAM).
- `--use-bloom` : Use Bloom filter (low RAM, some false positives).
- `--use-sqlite` : Use SQLite database (default, low RAM, disk usage).

### Example

```bash
./dedup --read1 R1.fastq.gz --read2 R2.fastq.gz --barcode-in-name --use-memory
```

Output files will be written as:

- `nodup_<read1file>.fastq.gz`
- `nodup_<read2file>.fastq.gz`



## Performance Notes

- **Memory mode**: Exact and fastest, but uses ~50–100 bytes per read stored. Works up to ~ 50 M reads
- **Bloom filter**: Memory efficient, but trades exactness for probabilistic membership (false positive allowed, no false negatives). This allow to save a lot of memory space, and a few unique flags may be wrongly flagged as duplicates. The false positive rate is adjustable (default 0.1%). 
- **SQLite**: Saves the reads in a SQlite database. This is safe for very large datasets, but slower due to disk I/O.


## Authors

Simon Joly, 2025, for the main program. Developped with support from chatGPT, but the whole script was validated by the author.

Arash Partow, 2000, for the Open Bloom Filter (bloom_filter.hpp)



## License

Distributed under the MIT License. Feel free to use, modify, and distribute.
