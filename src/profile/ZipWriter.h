#pragma once
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <zlib.h>

// Minimal ZIP file writer using zlib (DEFLATE compression).
// Produces standard ZIP files readable by any ZIP tool.
class ZipWriter {
public:
    explicit ZipWriter(const std::string& path) : m_path(path), m_ok(true) {}

    // Add a file with the given name (may contain slashes for directory structure)
    // and contents as a byte vector.
    bool add(const std::string& entryName, const std::vector<uint8_t>& contents) {
        if (!m_ok) return false;
        addEntry({entryName, contents});
        return m_ok;
    }

    // Add a file with string contents (UTF-8 text).
    bool add(const std::string& entryName, const std::string& contents) {
        std::vector<uint8_t> data(contents.begin(), contents.end());
        return add(entryName, data);
    }

    // Finalize and write the ZIP file to disk.
    // Returns true on success.
    bool finalize() {
        if (!m_ok) return false;

        std::ofstream out(m_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;

        uint32_t centralDirOffset = static_cast<uint32_t>(out.tellp());

        // Write local file headers + compressed data for each entry
        for (const auto& e : m_entries) {
            writeLocalHeader(out, e);
        }

        uint32_t centralDirSize = static_cast<uint32_t>(out.tellp()) - centralDirOffset;

        // Write central directory
        uint32_t centralDirStart = centralDirOffset;
        for (const auto& e : m_entries) {
            writeCentralHeader(out, e);
        }

        // Write end-of-central-directory record
        writeEocd(out, centralDirStart, centralDirSize);

        out.close();
        return out.good();
    }

    bool isOk() const { return m_ok; }

private:
    struct Entry {
        std::string name;
        std::vector<uint8_t> data;
        uint32_t crc32;
        uint16_t compressedSize;
        uint16_t compressedSizeExt; // 32-bit stored as 16+16 split
        uint32_t compressedSizeFull;
    };

    std::string m_path;
    std::vector<Entry> m_entries;
    bool m_ok;

    void addEntry(Entry&& e) {
        e.crc32 = static_cast<uint32_t>(crc32(0L, nullptr, 0));
        e.crc32 = static_cast<uint32_t>(
            crc32(e.crc32, e.data.data(), static_cast<uInt>(e.data.size())));

        // Compress with DEFLATE
        z_stream zs;
        std::memset(&zs, 0, sizeof(zs));
        if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                         -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            m_ok = false;
            return;
        }

        std::vector<uint8_t> compressed;
        compressed.reserve(e.data.size() / 2 + 64);

        zs.next_in = e.data.data();
        zs.avail_in = static_cast<uInt>(e.data.size());

        uint8_t outBuf[4096];
        do {
            zs.next_out = outBuf;
            zs.avail_out = sizeof(outBuf);
            int r = deflate(&zs, Z_FINISH);
            if (r != Z_OK && r != Z_STREAM_END && r != Z_BUF_ERROR) {
                deflateEnd(&zs);
                m_ok = false;
                return;
            }
            size_t produced = sizeof(outBuf) - zs.avail_out;
            compressed.insert(compressed.end(), outBuf, outBuf + produced);
            if (r == Z_STREAM_END) break;
        } while (zs.avail_out == 0);

        deflateEnd(&zs);

        e.compressedSizeFull = static_cast<uint32_t>(compressed.size());
        // Store size in two 16-bit parts for compatibility
        e.compressedSize = static_cast<uint16_t>(compressed.size() & 0xFFFF);
        e.compressedSizeExt = static_cast<uint16_t>(compressed.size() >> 16);

        e.data = std::move(compressed);
        m_entries.push_back(std::move(e));
    }

    static void writeLE16(std::ofstream& out, uint16_t v) {
        out.put(static_cast<char>(v & 0xFF));
        out.put(static_cast<char>((v >> 8) & 0xFF));
    }

    static void writeLE32(std::ofstream& out, uint32_t v) {
        out.put(static_cast<char>(v & 0xFF));
        out.put(static_cast<char>((v >> 8) & 0xFF));
        out.put(static_cast<char>((v >> 16) & 0xFF));
        out.put(static_cast<char>((v >> 24) & 0xFF));
    }

    void writeLocalHeader(std::ofstream& out, const Entry& e) const {
        writeLE32(out, 0x04034b50);           // local file header signature
        writeLE16(out, 20);                    // version needed to extract
        writeLE16(out, 0);                     // general purpose bit flag
        writeLE16(out, 8);                     // compression method (DEFLATE)
        writeLE16(out, 0);                     // last mod file time
        writeLE16(out, 0);                     // last mod file date
        writeLE32(out, e.crc32);              // crc-32
        writeLE32(out, e.compressedSizeFull); // compressed size
        writeLE32(out, static_cast<uint32_t>(e.name.size())); // name length
        writeLE16(out, 0);                     // extra field length
        out.write(e.name.data(), static_cast<std::streamsize>(e.name.size()));
        out.write(reinterpret_cast<const char*>(e.data.data()),
                  static_cast<std::streamsize>(e.data.size()));
    }

    void writeCentralHeader(std::ofstream& out, const Entry& e) const {
        writeLE32(out, 0x02014b50);            // central file header signature
        writeLE16(out, 20);                    // version made by
        writeLE16(out, 20);                    // version needed to extract
        writeLE16(out, 0);                     // general purpose bit flag
        writeLE16(out, 8);                     // compression method (DEFLATE)
        writeLE16(out, 0);                     // last mod file time
        writeLE16(out, 0);                     // last mod file date
        writeLE32(out, e.crc32);              // crc-32
        writeLE32(out, e.compressedSizeFull); // compressed size
        writeLE32(out, static_cast<uint32_t>(e.name.size())); // name length
        writeLE16(out, 0);                     // extra field length
        writeLE16(out, 0);                     // file comment length
        writeLE16(out, 0);                     // disk number start
        writeLE16(out, 0);                     // internal file attributes
        writeLE16(out, 0);                     // external file attributes
        writeLE32(out, 0);                     // relative offset of local header
        out.write(e.name.data(), static_cast<std::streamsize>(e.name.size()));
    }

    void writeEocd(std::ofstream& out, uint32_t cdOffset, uint32_t cdSize) const {
        uint32_t numEntries = static_cast<uint32_t>(m_entries.size());
        writeLE32(out, 0x06054b50);        // end of central dir signature
        writeLE16(out, 0);                  // number of this disk
        writeLE16(out, 0);                  // disk where central directory starts
        writeLE16(out, numEntries & 0xFFFF); // entries on this disk (16-bit)
        writeLE16(out, numEntries & 0xFFFF); // total entries (16-bit)
        writeLE32(out, cdSize);             // size of central directory
        writeLE32(out, cdOffset);           // offset of start of central directory
        writeLE16(out, 0);                  // ZIP file comment length
    }
};
