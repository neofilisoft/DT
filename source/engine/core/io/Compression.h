// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include <vector>
#include <cstdint>

namespace lacrima::io
{
    /**
     * @brief High-performance compression utilities wrapping Zstandard (zstd).
     */
    class Compression
    {
    public:
        /**
         * @brief Compresses a data buffer.
         * @param data Pointer to the input data.
         * @param size Size of the input data in bytes.
         * @param compressionLevel 1 (fastest) to 22 (smallest). Default is usually 3.
         * @return Compressed byte array. Empty if compression failed.
         */
        static std::vector<uint8_t> Compress(const void* data, size_t size, int compressionLevel = 3);

        /**
         * @brief Decompresses a data buffer.
         * @param compressedData Pointer to the compressed data.
         * @param compressedSize Size of the compressed data in bytes.
         * @return Decompressed byte array. Empty if decompression failed.
         */
        static std::vector<uint8_t> Decompress(const void* compressedData, size_t compressedSize);
    };
} // namespace lacrima::io

