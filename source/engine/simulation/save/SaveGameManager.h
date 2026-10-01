// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "SaveGameTypes.h"
#include "simulation/world/SimulationWorld.h"
#include <string>
#include <vector>
#include <optional>

namespace lacrima::save
{
    class SaveGameManager
    {
    public:
        // Sets the directory where save files are stored (default: "Saved/SaveGames")
        static void SetSaveDirectory(const std::string& directoryPath);
        static const std::string& GetSaveDirectory();

        // High-level slot API with atomic writing and ZSTD compression
        static bool SaveGameToSlot(
            const std::string& slotName,
            const sim::SimulationWorld& world,
            const GameSavePayload& gameData = {},
            u64 playTimeSeconds = 0);

        static bool LoadGameFromSlot(
            const std::string& slotName,
            sim::SimulationWorld& world,
            GameSavePayload& outGameData);

        // Slot queries & file management
        static bool DoesSaveExist(const std::string& slotName);
        static std::optional<SaveGameHeader> ReadSlotHeader(const std::string& slotName);
        static std::vector<SaveGameHeader> ListAllSaveSlots();
        static bool DeleteSaveSlot(const std::string& slotName);

    private:
        static std::string GetSlotFilePath(const std::string& slotName);
        static std::string s_saveDirectory;
    };
}
