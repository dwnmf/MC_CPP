#include "nbt_utils.h"
#include <zlib.h>
#include <iostream>
#include <vector>
#include <cstring>

// Helper functions for Big Endian
int16_t read_short_be(const uint8_t* data, size_t& offset) {
    uint16_t val = ((uint16_t)data[offset] << 8) | (uint16_t)data[offset + 1];
    offset += 2;
    return (int16_t)val;
}

int32_t read_int_be(const uint8_t* data, size_t& offset) {
    uint32_t val = ((uint32_t)data[offset] << 24) |
    ((uint32_t)data[offset + 1] << 16) |
    ((uint32_t)data[offset + 2] << 8) |
    (uint32_t)data[offset + 3];
    offset += 4;
    return (int32_t)val;
}

bool NBT::read_blocks_from_gzip(const std::string& path, uint8_t* dest_buffer, size_t expected_size) {
    gzFile file = gzopen(path.c_str(), "rb");
    if (!file) return false;

    // 512KB buffer
    std::vector<uint8_t> buffer(512 * 1024);
    int bytes_read = gzread(file, buffer.data(), buffer.size());
    gzclose(file);

    if (bytes_read <= 0) return false;

    size_t cursor = 0;

    auto safe_check = [&](size_t needed) {
        return (cursor + needed <= (size_t)bytes_read);
    };

    // Skip root Compound tag (0x0A) if present
    if (safe_check(1) && buffer[cursor] == 0x0A) {
        cursor++;
        if (!safe_check(2)) return false;
        int16_t name_len = read_short_be(buffer.data(), cursor);
        if (name_len < 0 || !safe_check(name_len)) return false;
        cursor += name_len;
    }

    // Iterate through tags to find "Blocks"
    while (safe_check(1)) {
        uint8_t tagType = buffer[cursor++];
        if (tagType == 0) break; // End Tag

        if (!safe_check(2)) return false;
        int16_t name_len = read_short_be(buffer.data(), cursor);
        if (name_len < 0 || !safe_check(name_len)) return false;

        std::string tagName((char*)&buffer[cursor], name_len);
        cursor += name_len;

        // Found "Blocks" (ByteArray = 7)
        if (tagType == 7 && tagName == "Blocks") {
            if (!safe_check(4)) return false;
            int32_t array_len = read_int_be(buffer.data(), cursor);

            if (array_len < 0) return false;

            // Check if size matches (32768 bytes for 16x16x128)
            if (array_len == (int32_t)expected_size && safe_check(array_len)) {

                // === FIX STARTS HERE ===
                // Instead of memcpy, we map the indices manually.

                // Cast the flat dest_buffer back to the 3D array structure used in Chunk
                // blocks[x][y][z] -> [16][128][16]
                using BlockArray = uint8_t[16][128][16];
                BlockArray& blocks = *reinterpret_cast<BlockArray*>(dest_buffer);

                const uint8_t* src = &buffer[cursor];

                // Minecraft Standard Format (Pre-1.13/Anvil):
                // Index = y + (z * Height) + (x * Height * Width)
                // Y is the fastest changing index.

                for (int x = 0; x < 16; x++) {
                    for (int z = 0; z < 16; z++) {
                        for (int y = 0; y < 128; y++) {
                            // Calculate index in NBT file
                            int nbt_index = y + (z * 128) + (x * 128 * 16);

                            // Bounds check (just in case)
                            if (nbt_index < array_len) {
                                blocks[x][y][z] = src[nbt_index];
                            }
                        }
                    }
                }
                // === FIX ENDS HERE ===

                return true;
            }
            return false;
        }
        // Handle "Level" compound to recurse slightly without full parser
        else if (tagType == 10 && tagName == "Level") {
            continue;
        }
        else {
            // Basic skip logic for unknown tags could be added here,
            // but for now, rely on fallback if structure is complex.
            // (Simple skip logic removed for brevity, assuming standard format)
            // If we hit a complex tag we can't skip, we fall through to "Fallback search"
            break;
        }
    }

    // Fallback: Brute-force search for "Blocks" signature
    const uint8_t header[] = {0x07, 0x00, 0x06, 'B', 'l', 'o', 'c', 'k', 's'};
    for (size_t i = 0; i < (size_t)bytes_read - sizeof(header) - 4; ++i) {
        if (std::memcmp(&buffer[i], header, sizeof(header)) == 0) {
            size_t data_offset = i + sizeof(header);
            int32_t len = read_int_be(buffer.data(), data_offset);

            if (len == (int32_t)expected_size && (data_offset + len <= (size_t)bytes_read)) {

                // Apply the same fix in fallback
                using BlockArray = uint8_t[16][128][16];
                BlockArray& blocks = *reinterpret_cast<BlockArray*>(dest_buffer);
                const uint8_t* src = &buffer[data_offset];

                for (int x = 0; x < 16; x++) {
                    for (int z = 0; z < 16; z++) {
                        for (int y = 0; y < 128; y++) {
                            int nbt_index = y + (z * 128) + (x * 128 * 16);
                            blocks[x][y][z] = src[nbt_index];
                        }
                    }
                }
                return true;
            }
        }
    }

    return false;
}

// --- ЗАПИСЬ (НОВОЕ) ---

void write_short_be(std::vector<uint8_t>& buf, int16_t val) {
    buf.push_back((val >> 8) & 0xFF);
    buf.push_back(val & 0xFF);
}

void write_int_be(std::vector<uint8_t>& buf, int32_t val) {
    buf.push_back((val >> 24) & 0xFF);
    buf.push_back((val >> 16) & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
    buf.push_back(val & 0xFF);
}

void write_string(std::vector<uint8_t>& buf, const std::string& str) {
    write_short_be(buf, (int16_t)str.size());
    for(char c : str) buf.push_back(c);
}

bool NBT::write_blocks_to_gzip(const std::string& path, const uint8_t* src_buffer, int width, int height, int length) {
    std::vector<uint8_t> nbt;

    // 1. Root Compound (Tag 10) - Name ""
    nbt.push_back(0x0A);
    write_string(nbt, "");

    // 2. "Level" Compound (Tag 10)
    nbt.push_back(0x0A);
    write_string(nbt, "Level");

    // 3. "Blocks" ByteArray (Tag 7)
    nbt.push_back(0x07);
    write_string(nbt, "Blocks");

    // Array Length (16 * 128 * 16 = 32768)
    int32_t size = width * height * length;
    write_int_be(nbt, size);

    // Data: Переупорядочиваем из формата чанка (X, Y, Z) в формат NBT (X, Z, Y)
    // где Y меняется быстрее всего.
    // blocks[x][y][z] -> [16][128][16]
    using BlockArray = uint8_t[16][128][16];
    const BlockArray& blocks = *reinterpret_cast<const BlockArray*>(src_buffer);

    // NBT Order: index = y + (z * Height) + (x * Height * Width)
    // Значит внешний цикл X, средний Z, внутренний Y
    for (int x = 0; x < 16; x++) {
        for (int z = 0; z < 16; z++) {
            for (int y = 0; y < 128; y++) {
                nbt.push_back(blocks[x][y][z]);
            }
        }
    }

    // 4. End Tags
    nbt.push_back(0x00); // End "Level"
    nbt.push_back(0x00); // End Root

    // Запись в GZIP
    gzFile file = gzopen(path.c_str(), "wb");
    if (!file) return false;

    int written = gzwrite(file, nbt.data(), nbt.size());
    gzclose(file);

    return (written > 0);
}
