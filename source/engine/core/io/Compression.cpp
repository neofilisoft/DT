// Copyright Neofilisoft. All Rights Reserved.
#include "core/io/Compression.h"
#include "core/logging/Logger.h"
#include <zstd.h>

namespace lacrima::io
{
    std::vector<uint8_t> Compression::Compress(const void* data, size_t size, int compressionLevel)
    {
        if (!data || size == 0) return {};

        size_t bound = ZSTD_compressBound(size);
        std::vector<uint8_t> compressed(bound);

        size_t cSize = ZSTD_compress(compressed.data(), bound, data, size, compressionLevel);
        
        if (ZSTD_isError(cSize))
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "ZSTD Compression failed: %s", ZSTD_getErrorName(cSize));
            return {};
        }

        compressed.resize(cSize);
        return compressed;
    }

    std::vector<uint8_t> Compression::Decompress(const void* compressedData, size_t compressedSize)
    {
        if (!compressedData || compressedSize == 0) return {};

        unsigned long long const rSize = ZSTD_getFrameContentSize(compressedData, compressedSize);
        if (rSize == ZSTD_CONTENTSIZE_ERROR)
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "ZSTD Decompression: Not compressed by zstd!");
            return {};
        }
        if (rSize == ZSTD_CONTENTSIZE_UNKNOWN)
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "ZSTD Decompression: Original size unknown. Streaming not supported yet.");
            return {};
        }

        std::vector<uint8_t> decompressed(rSize);
        size_t dSize = ZSTD_decompress(decompressed.data(), rSize, compressedData, compressedSize);

        if (ZSTD_isError(dSize))
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "ZSTD Decompression failed: %s", ZSTD_getErrorName(dSize));
            return {};
        }

        return decompressed;
    }
} // namespace lacrima::io


