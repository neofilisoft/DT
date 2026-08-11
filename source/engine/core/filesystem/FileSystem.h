#pragma once

#include "core/platform/Types.h"

#include <optional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// FileSystem.h
//
// Platform-abstracted file I/O. This is the ONLY place in the engine that
// is allowed to call platform-native file APIs directly (std::filesystem
// underneath, wrapped rather than used directly at every call site,
// specifically so error handling and path-separator normalization has one
// enforcement point rather than being re-solved at every call site across
// Asset, Serialization/save-system, and Config).
//
// Path normalization: all paths accepted and returned by this API use
// forward slashes ('/') internally, converted to the native separator only
// at the actual OS syscall boundary inside the .cpp. This matters because
// path strings get embedded in save-file references (e.g. an .asset path
// stored in a save) and in Lua interaction scripts (a script referencing
// "objects/fridge/fridge_open.asset") - if those strings used
// platform-native separators, a save file or script authored on Windows
// would fail to resolve its asset paths when loaded on Linux.
// ---------------------------------------------------------------------------

namespace dt
{
    enum class FileOpenMode : u8
    {
        Read,
        Write,
        ReadWrite,
        Append
    };

    struct FileStats
    {
        usize sizeBytes = 0;
        u64 lastModifiedUnixMs = 0;
        bool exists = false;
        bool isDirectory = false;
    };

    class FileHandle
    {
    public:
        FileHandle() = default;
        ~FileHandle();

        FileHandle(const FileHandle&) = delete;
        FileHandle& operator=(const FileHandle&) = delete;
        FileHandle(FileHandle&& other) noexcept;
        FileHandle& operator=(FileHandle&& other) noexcept;

        bool IsOpen() const { return m_impl != nullptr; }

        usize Read(void* dest, usize sizeBytes);
        usize Write(const void* data, usize sizeBytes);

        void Seek(usize offset);
        usize Tell() const;
        usize Size() const;

        void Close();

    private:
        friend class FileSystem;
        struct Impl;
        Impl* m_impl = nullptr;
    };

    class FileSystem
    {
    public:
        static FileSystem& Get();

        // All paths are treated relative to the engine's "content root"
        // (set via SetContentRoot, typically the directory containing the
        // cooked .asset database) unless the path is already absolute.
        void SetContentRoot(const std::string& rootPath);
        const std::string& GetContentRoot() const { return m_contentRoot; }

        std::optional<FileHandle> Open(const std::string& path, FileOpenMode mode);

        bool Exists(const std::string& path) const;
        FileStats Stat(const std::string& path) const;

        bool CreateDirectoryRecursive(const std::string& path) const;
        std::vector<std::string> ListDirectory(const std::string& path, bool recursive) const;

        // Reads an entire file into memory in one call - used by the asset
        // Importer/Cooker pipeline and by save/load, where the whole file
        // is needed in memory anyway (not a case for streamed reads).
        std::optional<std::vector<u8>> ReadEntireFile(const std::string& path) const;
        bool WriteEntireFile(const std::string& path, const void* data, usize sizeBytes) const;

        std::string ResolvePath(const std::string& path) const;

        static std::string GetExtension(const std::string& path);
        static std::string GetFileNameWithoutExtension(const std::string& path);
        static std::string GetParentDirectory(const std::string& path);
        static std::string NormalizeSeparators(const std::string& path);

    private:
        std::string m_contentRoot;
    };
}
