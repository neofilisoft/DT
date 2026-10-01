// Copyright Neofilisoft. All Rights Reserved.
#include "SaveGameManager.h"
#include "core/io/Compression.h"
#include "core/logging/Logger.h"
#include "core/serialization/Serialization.h"

#include <filesystem>
#include <fstream>
#include <chrono>

namespace lacrima::save
{
    namespace fs = std::filesystem;

    std::string SaveGameManager::s_saveDirectory = "Saved/SaveGames";

    void SaveGameManager::SetSaveDirectory(const std::string& directoryPath)
    {
        s_saveDirectory = directoryPath;
    }

    const std::string& SaveGameManager::GetSaveDirectory()
    {
        return s_saveDirectory;
    }

    std::string SaveGameManager::GetSlotFilePath(const std::string& slotName)
    {
        fs::path dir(s_saveDirectory);
        std::string filename = slotName;
        if (!filename.ends_with(".sav"))
        {
            filename += ".sav";
        }
        return (dir / filename).string();
    }

    bool SaveGameManager::SaveGameToSlot(
        const std::string& slotName,
        const sim::SimulationWorld& world,
        const GameSavePayload& gameData,
        u64 playTimeSeconds)
    {
        fs::path dir(s_saveDirectory);
        std::error_code ec;
        fs::create_directories(dir, ec);

        // 1. Serialize uncompressed payload into memory
        lacrima::BinaryWriter writer;
        world.SaveState(writer);

        // Append GameSavePayload
        writer.WritePrimitive<i64>(gameData.simoleons);
        writer.WritePrimitive<f32>(gameData.timeOfDay);
        writer.WritePrimitive<u32>(gameData.dayIndex);
        writer.WritePrimitive<f32>(gameData.cameraPosX);
        writer.WritePrimitive<f32>(gameData.cameraPosY);
        writer.WritePrimitive<f32>(gameData.cameraZoom);
        writer.WriteString(gameData.customJsonData);

        const auto& rawBuffer = writer.Data();
        if (rawBuffer.empty())
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "SaveGameManager: empty payload during save slot '{}'", slotName);
            return false;
        }

        // 2. Compress payload via ZSTD
        std::vector<u8> compressed = io::Compression::Compress(rawBuffer.data(), rawBuffer.size(), 3);
        if (compressed.empty())
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "SaveGameManager: ZSTD compression failed for slot '{}'", slotName);
            return false;
        }

        // 3. Construct header
        SaveGameHeader header{};
        header.magic = kSaveMagic;
        header.version = kCurrentSaveVersion;
        header.timestamp = static_cast<u64>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        header.totalPlayTimeSeconds = playTimeSeconds;
        std::strncpy(header.saveName, slotName.c_str(), sizeof(header.saveName) - 1);
        header.uncompressedSize = static_cast<u32>(rawBuffer.size());
        header.compressedSize = static_cast<u32>(compressed.size());
        header.entityCount = static_cast<u32>(world.LiveEntityCount());

        // 4. Atomic File Write (write to temp file then rename)
        std::string finalPath = GetSlotFilePath(slotName);
        std::string tmpPath = finalPath + ".tmp";

        {
            std::ofstream out(tmpPath, std::ios::binary);
            if (!out)
            {
                LACRIMA_LOG_ERROR(LogCategory::Core, "SaveGameManager: could not open temp file '{}' for writing", tmpPath);
                return false;
            }

            out.write(reinterpret_cast<const char*>(&header), sizeof(SaveGameHeader));
            out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
            out.flush();
            if (!out.good())
            {
                LACRIMA_LOG_ERROR(LogCategory::Core, "SaveGameManager: failed to write all bytes to '{}'", tmpPath);
                return false;
            }
        }

        // Atomic swap
        fs::rename(tmpPath, finalPath, ec);
        if (ec)
        {
            // Fallback for filesystem quirks
            fs::copy_file(tmpPath, finalPath, fs::copy_options::overwrite_existing, ec);
            fs::remove(tmpPath, ec);
        }

        LACRIMA_LOG_INFO(LogCategory::Core, "SaveGameManager: Successfully saved slot '{}' (raw: {} B, zstd: {} B)",
            slotName, header.uncompressedSize, header.compressedSize);
        return true;
    }

    bool SaveGameManager::LoadGameFromSlot(
        const std::string& slotName,
        sim::SimulationWorld& world,
        GameSavePayload& outGameData)
    {
        std::string filePath = GetSlotFilePath(slotName);
        std::ifstream in(filePath, std::ios::binary);
        if (!in)
        {
            LACRIMA_LOG_WARN(LogCategory::Core, "SaveGameManager: save file not found '{}'", filePath);
            return false;
        }

        // 1. Read and validate header
        SaveGameHeader header{};
        in.read(reinterpret_cast<char*>(&header), sizeof(SaveGameHeader));
        if (!in || header.magic != kSaveMagic)
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "SaveGameManager: corrupted header / invalid magic in '{}'", filePath);
            return false;
        }

        if (header.version > kCurrentSaveVersion)
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "SaveGameManager: save version {} exceeds current supported version {}",
                header.version, kCurrentSaveVersion);
            return false;
        }

        // 2. Read compressed payload
        std::vector<u8> compressed(header.compressedSize);
        in.read(reinterpret_cast<char*>(compressed.data()), header.compressedSize);
        if (!in)
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "SaveGameManager: failed reading compressed payload in '{}'", filePath);
            return false;
        }

        // 3. Decompress payload
        std::vector<u8> decompressed = io::Compression::Decompress(compressed.data(), compressed.size());
        if (decompressed.size() != header.uncompressedSize)
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "SaveGameManager: ZSTD decompression size mismatch (expected {}, got {})",
                header.uncompressedSize, decompressed.size());
            return false;
        }

        // 4. Deserialize world and game data
        lacrima::BinaryReader reader(decompressed);
        if (!world.LoadState(reader))
        {
            LACRIMA_LOG_ERROR(LogCategory::Core, "SaveGameManager: SimulationWorld::LoadState failed for '{}'", filePath);
            return false;
        }

        if (!reader.AtEnd())
        {
            outGameData.simoleons = reader.ReadPrimitive<i64>();
            outGameData.timeOfDay = reader.ReadPrimitive<f32>();
            outGameData.dayIndex = reader.ReadPrimitive<u32>();
            outGameData.cameraPosX = reader.ReadPrimitive<f32>();
            outGameData.cameraPosY = reader.ReadPrimitive<f32>();
            outGameData.cameraZoom = reader.ReadPrimitive<f32>();
            outGameData.customJsonData = reader.ReadString();
        }

        LACRIMA_LOG_INFO(LogCategory::Core, "SaveGameManager: Successfully loaded slot '{}'", slotName);
        return true;
    }

    bool SaveGameManager::DoesSaveExist(const std::string& slotName)
    {
        return fs::exists(GetSlotFilePath(slotName));
    }

    std::optional<SaveGameHeader> SaveGameManager::ReadSlotHeader(const std::string& slotName)
    {
        std::string filePath = GetSlotFilePath(slotName);
        std::ifstream in(filePath, std::ios::binary);
        if (!in) return std::nullopt;

        SaveGameHeader header{};
        in.read(reinterpret_cast<char*>(&header), sizeof(SaveGameHeader));
        if (!in || header.magic != kSaveMagic) return std::nullopt;

        return header;
    }

    std::vector<SaveGameHeader> SaveGameManager::ListAllSaveSlots()
    {
        std::vector<SaveGameHeader> results;
        fs::path dir(s_saveDirectory);
        if (!fs::exists(dir)) return results;

        for (const auto& entry : fs::directory_iterator(dir))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".sav")
            {
                std::ifstream in(entry.path(), std::ios::binary);
                if (in)
                {
                    SaveGameHeader header{};
                    in.read(reinterpret_cast<char*>(&header), sizeof(SaveGameHeader));
                    if (in && header.magic == kSaveMagic)
                    {
                        results.push_back(header);
                    }
                }
            }
        }
        return results;
    }

    bool SaveGameManager::DeleteSaveSlot(const std::string& slotName)
    {
        std::string path = GetSlotFilePath(slotName);
        std::error_code ec;
        return fs::remove(path, ec);
    }
}
