// Copyright Neofilisoft. All Rights Reserved.
#include "core/asset/SpriteAnimationAsset.h"

#include <zlib.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
#pragma pack(push, 1)
    struct AssetHeader
    {
        char magic[4];
        lacrima::u32 version;
        lacrima::u32 type;
        lacrima::u32 isCompressed;
        lacrima::u32 uncompressedSize;
    };

    struct SpritePayloadHeader
    {
        lacrima::u32 width;
        lacrima::u32 height;
        lacrima::f32 framesPerSecond;
        lacrima::u32 looping;
        lacrima::u32 pixelPerfect;
        lacrima::u32 frameCount;
        lacrima::u32 frameNameBytes;
    };
#pragma pack(pop)

    struct SpriteAtlasMetadata
    {
        lacrima::u32 atlasColumns;
        lacrima::u32 atlasRows;
    };

    constexpr lacrima::u32 kSpriteAssetType = 4;
}

namespace lacrima::asset
{
    bool SpriteAnimationAsset::LoadFromFile(const std::string& path)
    {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input)
            return false;

        const std::streamsize fileSize = input.tellg();
        if (fileSize < static_cast<std::streamsize>(sizeof(AssetHeader) + sizeof(SpritePayloadHeader)))
            return false;
        input.seekg(0, std::ios::beg);

        std::vector<u8> bytes(static_cast<size_t>(fileSize));
        if (!input.read(reinterpret_cast<char*>(bytes.data()), fileSize))
            return false;

        AssetHeader header{};
        std::memcpy(&header, bytes.data(), sizeof(header));
        if (std::memcmp(header.magic, "DTAS", 4) != 0 || header.type != kSpriteAssetType)
            return false;

        SpritePayloadHeader payloadHeader{};
        std::memcpy(&payloadHeader, bytes.data() + sizeof(header), sizeof(payloadHeader));
        const size_t payloadOffset = sizeof(header) + sizeof(payloadHeader);
        const size_t storedSize = bytes.size() - payloadOffset;
        if (payloadHeader.frameCount == 0 || payloadHeader.frameNameBytes == 0 || storedSize == 0)
            return false;

        std::vector<u8> names;
        if (header.isCompressed != 0)
        {
            names.resize(header.uncompressedSize);
            uLongf outputSize = static_cast<uLongf>(names.size());
            const int result = uncompress(names.data(), &outputSize,
                                           bytes.data() + payloadOffset,
                                           static_cast<uLong>(storedSize));
            if (result != Z_OK || outputSize < payloadHeader.frameNameBytes)
                return false;
            names.resize(static_cast<size_t>(outputSize));
        }
        else
        {
            if (storedSize < payloadHeader.frameNameBytes)
                return false;
            names.assign(bytes.begin() + static_cast<std::ptrdiff_t>(payloadOffset),
                         bytes.begin() + static_cast<std::ptrdiff_t>(payloadOffset + storedSize));
        }

        m_width = payloadHeader.width;
        m_height = payloadHeader.height;
        m_framesPerSecond = payloadHeader.framesPerSecond > 0.0f ? payloadHeader.framesPerSecond : 1.0f;
        m_looping = payloadHeader.looping != 0;
        m_pixelPerfect = payloadHeader.pixelPerfect != 0;
        m_atlasColumns = 1;
        m_atlasRows = 1;
        if (names.size() >= static_cast<size_t>(payloadHeader.frameNameBytes) + sizeof(SpriteAtlasMetadata))
        {
            SpriteAtlasMetadata metadata{};
            std::memcpy(&metadata, names.data() + payloadHeader.frameNameBytes, sizeof(metadata));
            m_atlasColumns = std::max(1u, metadata.atlasColumns);
            m_atlasRows = std::max(1u, metadata.atlasRows);
        }
        m_frameNames.clear();

        std::istringstream lines(std::string(reinterpret_cast<const char*>(names.data()), payloadHeader.frameNameBytes));
        std::string line;
        while (std::getline(lines, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty())
                m_frameNames.push_back(std::move(line));
        }

        return m_frameNames.size() == payloadHeader.frameCount;
    }
}
