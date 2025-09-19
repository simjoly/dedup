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

Here is an example of usage with the barcodes in the names of the samples and using a Bloom filter:

```bash
./dedup --read1 R1.fastq.gz --read2 R2.fastq.gz --barcode-in-name --use-bloom
```

Output files will be written as:

- `nodup_<read1file>.fastq.gz`
- `nodup_<read2file>.fastq.gz`


## Random barcode index

The program can take into account the use of a random nucleotide barcodes incorporated at the PCR step of the library to identify PCR duplicates, as in the 3RAD protocol. There are two options to feed this information to the program.

### Separate index file

If the random index is in a separate fastq file (same order as in the files that contain the reads), you can run the program using this command:

```bash
./dedup --read1 R1.fastq.gz --read2 R2.fastq.gz --index <indexfile>
```

### Index in the name of the sample

The random index can be placed in the name of the sample. For instance, in the fastq file:

```
@A01114:199:HGJMGDSXF:2:1101:1949:1016:GTGGGGGG 1:N:0:TGAGGTGT
ANCGTTGGCTAGACTGAAATAACTAGACGTCTAAGTCTAGGTCTTCTCTAGGTCGTCTTCAGGTGAACAACGAGGTCCTACAGAAGATGTTGAGATAAGAGAGGTATAAAACCGAAATAATGATTTAGAACCCGCAAAAGTTTTTGAAATA
+
F#:F:,F:FFF,F,F,FFF,FF:F,:FFF:F:F:FFFFFFFFF:FFFFF:FFFF,FFF,FFF,,FFF:FFF:F:FFFFFF:FFFF,FF,F,FFFF:FFFFFF,:FFFFFFFF,FFFF:FFFFF:FFF:FF:F::FFFFF,F::FFFF,FFF
``` 

The index is before the space in the sequence name (line begining with @). In this example, the index is GTGGGGGG. To analyze this, you can run the program like this:

```bash
./dedup --read1 R1.fastq.gz --read2 R2.fastq.gz --barcode-in-name
```


## Deduplication method

Three options are possible for the deduplication method

### Virtual memory

With this option, the reads and indexes are stored in the virtual memory and this information is read to see if a new sequence is a duplicate (i.e., is already in memory). This approach is fast, exact (makes no errors), but uses ~50–100 bytes per read stored. Should be used with datasets up to ~ 50M reads. Usage is:

```bash
./dedup --read1 R1.fastq.gz --read2 R2.fastq.gz --barcode-in-name --use-memory
```

### Bloom filter (default option)

The Bloom filter is an approach often used in bioinformatics. It is a probabilistic method that trades exactness for probabilistic membership (false positive are allowed, no false negatives). This allow to save memory space, but a few unique flags may be wrongly flagged as duplicates. The false positive rate is fixed to 0.1% in the program, but this can be adjusted in the code. Usage is:

```bash
./dedup --read1 R1.fastq.gz --read2 R2.fastq.gz --barcode-in-name --use-bloom
```

### SQlite database

If memory is really a problem, then it is possible to store the reads in a database on disk. This requires very little virtual memory, but it makes the program run very slowly.

```bash
./dedup --read1 R1.fastq.gz --read2 R2.fastq.gz --barcode-in-name --use-sqlite
```


## Performance Notes

- **Memory mode**: Exact and fastest, but uses ~50–100 bytes per read stored. Works up to ~ 50 M reads
- **Bloom filter**: Memory efficient, but trades exactness for probabilistic membership (false positive allowed, no false negatives). This allow to save a lot of memory space, and a few unique flags may be wrongly flagged as duplicates. The false positive rate is adjustable (default 0.1%). 
- **SQLite**: Saves the reads in a SQlite database. This is safe for very large datasets, but slower due to disk I/O.


## Authors

Simon Joly, 2025, for the main program. Developped with support from chatGPT, but the whole script was validated by the author.

Arash Partow, 2000, for the Open Bloom Filter (bloom_filter.hpp)



## License

Distributed under the MIT License. Feel free to use, modify, and distribute.
