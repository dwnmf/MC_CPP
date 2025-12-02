#pragma once
#include <vector>
#include <string>
#include <cstdint>

namespace NBT {
    // Чтение NBT (как было)
    bool read_blocks_from_gzip(const std::string& path, uint8_t* dest_buffer, size_t expected_size);

    // Запись массива блоков в сжатый NBT файл
    // Структура: Root -> Level -> Blocks (ByteArray)
    bool write_blocks_to_gzip(const std::string& path, const uint8_t* src_buffer, int width, int height, int length);
}
