// dedup.cpp

// removes PCR duplicates from paired-ends fastq files

// Simon Joly, 2025



#include <iostream>
#include <iomanip>  // <<<< this is required for std::setprecision
#include <fstream>
#include <string>
#include <unordered_set>
#include <filesystem>
#include <zlib.h>
#include <openssl/sha.h>
#include <sqlite3.h>
#include <getopt.h>
#include "bloom_filter.hpp"

// --------------------------------------------------
// SHA-256 hashing (hex string)
// --------------------------------------------------
std::string sha256(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)data.c_str(), data.size(), hash);
    std::string hex;
    char buf[3];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(buf, "%02x", hash[i]);
        hex.append(buf);
    }
    return hex;
}

// --------------------------------------------------
// FASTQ record structure
// --------------------------------------------------
struct FastqRecord {
    std::string id, seq, plus, qual;
};

// --------------------------------------------------
// Read next FASTQ record
// --------------------------------------------------
bool read_fastq_record(gzFile file, FastqRecord &rec) {
    char buf[10000];
    if (!gzgets(file, buf, sizeof(buf))) return false;
    rec.id = buf;
    if (!gzgets(file, buf, sizeof(buf))) return false;
    rec.seq = buf;
    if (!gzgets(file, buf, sizeof(buf))) return false;
    rec.plus = buf;
    if (!gzgets(file, buf, sizeof(buf))) return false;
    rec.qual = buf;
    return true;
}

// --------------------------------------------------
// Write FASTQ record
// --------------------------------------------------
void write_fastq_record(gzFile file, const FastqRecord &rec) {
    gzputs(file, rec.id.c_str());
    gzputs(file, rec.seq.c_str());
    gzputs(file, rec.plus.c_str());
    gzputs(file, rec.qual.c_str());
}

// --------------------------------------------------
// Count number of fastq records
// --------------------------------------------------
size_t count_fastq_records(const std::string& filename) {
    gzFile f = gzopen(filename.c_str(), "rb");
    if (!f) throw std::runtime_error("Cannot open file: " + filename);
    char buf[8192];
    size_t lines = 0;
    while (gzgets(f, buf, sizeof(buf))) ++lines;
    gzclose(f);
    return lines / 4;
}

// --------------------------------------------------
// Extract barcode from FASTQ header
// --------------------------------------------------
std::string extract_barcode_from_name(const std::string& header) {
    size_t space_pos = header.find(' ');
    std::string main_part = (space_pos == std::string::npos) ? header : header.substr(0, space_pos);
    size_t last_colon = main_part.rfind(':');
    if (last_colon == std::string::npos) return "";
    return main_part.substr(last_colon + 1);
}

// --------------------------------------------------
// SQLite backend
// --------------------------------------------------
class SQLiteStore {
    sqlite3* db;
public:
    SQLiteStore(const std::string& filename) {
        if (sqlite3_open(filename.c_str(), &db))
            throw std::runtime_error("Cannot open SQLite DB");
        const char* sql = "CREATE TABLE IF NOT EXISTS hashes (hash TEXT PRIMARY KEY);";
        sqlite3_exec(db, sql, 0, 0, 0);
    }
    ~SQLiteStore() { sqlite3_close(db); }
    bool is_unique(const std::string& key) {
        sqlite3_stmt* stmt;
        const char* insert = "INSERT INTO hashes (hash) VALUES (?);";
        if (sqlite3_prepare_v2(db, insert, -1, &stmt, 0) != SQLITE_OK)
            throw std::runtime_error("SQLite prepare failed");
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_STATIC);
        bool unique = true;
        if (sqlite3_step(stmt) != SQLITE_DONE) unique = false;
        sqlite3_finalize(stmt);
        return unique;
    }
};

// --------------------------------------------------
// Main
// --------------------------------------------------
int main(int argc, char* argv[]) {
    std::string read1_file, read2_file, index_file;
    bool barcode_in_name = false;
    std::string backend = "bloom"; // default

    static struct option long_options[] = {
        {"read1", required_argument, 0, 'a'},
        {"read2", required_argument, 0, 'b'},
        {"index", required_argument, 0, 'i'},
        {"barcode-in-name", no_argument, 0, 'c'},
        {"use-memory", no_argument, 0, 'm'},
        {"use-bloom", no_argument, 0, 'l'},
        {"use-sqlite", no_argument, 0, 's'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "a:b:i:cmls", long_options, NULL)) != -1) {
        switch (opt) {
            case 'a': read1_file = optarg; break;
            case 'b': read2_file = optarg; break;
            case 'i': index_file = optarg; break;
            case 'c': barcode_in_name = true; break;
            case 'm': backend = "memory"; break;
            case 'l': backend = "bloom"; break;
            case 's': backend = "sqlite"; break;
            default:
                std::cerr << "Usage: dedup --read1 R1.fq.gz --read2 R2.fq.gz "
                          << "[--index I.fq.gz] [--barcode-in-name] "
                          << "[--use-memory | --use-bloom | --use-sqlite]\n";
                return 1;
        }
    }

    if (read1_file.empty() || read2_file.empty()) {
        std::cerr << "Error: must provide --read1 and --read2\n";
        return 1;
    }

    // Count reads
    std::cerr << "Counting reads in " << read1_file << "...\n";
    size_t total_reads = count_fastq_records(read1_file);
    std::cerr << "Total reads: " << total_reads << "\n";

    // Open input and output files
    gzFile f1 = gzopen(read1_file.c_str(), "rb");
    gzFile f2 = gzopen(read2_file.c_str(), "rb");
    if (!f1 || !f2) {
        std::cerr << "Error opening input files.\n";
        return 1;
    }
    gzFile f3 = nullptr;
    if (!index_file.empty()) {
        f3 = gzopen(index_file.c_str(), "rb");
        if (!f3) {
            std::cerr << "Error opening index file.\n";
            return 1;
        }
    }

    // Extract base filenames (no directories)
    std::string read1_base_filename = std::filesystem::path(read1_file).filename().string();
    std::string read2_base_filename = std::filesystem::path(read2_file).filename().string();

    std::string out1_name = "nodup_" + read1_base_filename;
    std::string out2_name = "nodup_" + read2_base_filename;
    gzFile out1 = gzopen(out1_name.c_str(), "wb");
    gzFile out2 = gzopen(out2_name.c_str(), "wb");

    // Backend init
    SQLiteStore* sqlite_store = nullptr;
    bloom_filter* bloom = nullptr;
    std::unordered_set<std::string> seen;

    if (backend == "sqlite") {
        sqlite_store = new SQLiteStore("dedup.sqlite");
    } else if (backend == "bloom") {
        bloom_parameters params;
        params.projected_element_count = static_cast<uint64_t>(total_reads);
        params.false_positive_probability = 0.001;
        params.compute_optimal_parameters();
        bloom = new bloom_filter(params);
    }

    // Process FASTQ pairs
    FastqRecord r1, r2, r3;
    size_t processed = 0, dup = 0, written = 0;

    while (read_fastq_record(f1, r1) && read_fastq_record(f2, r2)) {
        std::string key;
        if (barcode_in_name) {
            std::string barcode = extract_barcode_from_name(r1.id);
            key = sha256(barcode + r1.seq + r2.seq);
        } if (!index_file.empty()) {
            read_fastq_record(f3, r3);
            key = sha256(r3.seq + r1.seq + r2.seq);
        }
        else {
            key = sha256(r1.seq + r2.seq);
        }

        bool unique = true;
        if (backend == "memory") {
            if (seen.count(key)) unique = false;
            else seen.insert(key);
        } else if (backend == "sqlite") {
            unique = sqlite_store->is_unique(key);
        } else if (backend == "bloom") {
            if (bloom->contains(key)) unique = false;
            else bloom->insert(key);
        }

        if (unique) {
            write_fastq_record(out1, r1);
            write_fastq_record(out2, r2);
            written++;
        } else {
            dup++;
        }

        processed++;
        if (processed % 100000 == 0) {
            double pct_processed = (100.0 * processed) / total_reads;
            double pct_dup = (100.0 * dup) / processed;
            std::cerr << "\rProcessed: " << processed << " / " << total_reads << " (" 
                << std::fixed << std::setprecision(1) << pct_processed << "%) | " 
                << dup << " (" << std::fixed << std::setprecision(1) << pct_dup << "%) duplicates" << std::flush;
        }
    }

    // Cleanup
    gzclose(f1); gzclose(f2);
    if (f3) gzclose(f3);
    gzclose(out1); gzclose(out2);
    delete sqlite_store;
    delete bloom;

    std::cerr << "\nDone.\n";
    std::cerr << "Processed: " << processed << " read pairs\n";
    std::cerr << "Written:   " << written << " unique read pairs\n";
    std::cerr << "Duplicates: " << dup << " (" << (100.0 * dup / processed) << "%)\n";

    return 0;
}
