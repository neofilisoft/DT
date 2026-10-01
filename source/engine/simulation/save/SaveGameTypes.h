// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include <cstring>
#include <string>

namespace lacrima::save
{
    static constexpr u32 kSaveMagic = 0x5352434C; // 'LCRS' (Lacrima Save)
    static constexpr u32 kCurrentSaveVersion = 1;

    #pragma pack(push, 1)
    struct SaveGameHeader
    {
        u32  magic = kSaveMagic;
        u32  version = kCurrentSaveVersion;
        u64  timestamp = 0;              // Unix epoch seconds
        u64  totalPlayTimeSeconds = 0;   // In-game play duration
        char saveName[64] = {};         // Display name of slot
        char gameVersion[16] = "0.1.0"; // Game version string
        u32  uncompressedSize = 0;       // Raw decompressed payload size
        u32  compressedSize = 0;         // Compressed payload size on disk
        u32  entityCount = 0;            // Total entities in saved world
    };
    #pragma pack(pop)

    struct GameSavePayload
    {
        i64         simoleons = 0;       // Household funds
        f32         timeOfDay = 12.0f;   // 0.0 to 24.0 hours
        u32         dayIndex = 1;        // Day count
        f32         cameraPosX = 0.0f;
        f32         cameraPosY = 0.0f;
        f32         cameraZoom = 1.0f;
        std::string customJsonData;      // Extensible game-level payload
    };
}
