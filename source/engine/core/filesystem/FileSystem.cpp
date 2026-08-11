#include "core/filesystem/FileSystem.h"
#include "core/logging/Logger.h"
#include "core/platform/Assert.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace dt
{
    struct FileHandle::Impl
    {
        std::fstream stream;
        std::string path;
    };

    FileHandle::~FileHandle()
    {
        Close();
    }

    FileHandle::FileHandle(FileHandle&& other) noexcept
        : m_impl(other.m_impl)
    {
        other.m_impl = nullptr;
    }

    FileHandle& FileHandle::operator=(FileHandle&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            m_impl = other.m_impl;
            other.m_impl = nullptr;
        }
        return *this;
    }

    usize FileHandle::Read(void* dest, usize sizeBytes)
    {
        DT_ASSERT(IsOpen(), "FileHandle::Read called on unopened file");
        m_impl->stream.read(static_cast<char*>(dest), static_cast<std::streamsize>(sizeBytes));
        return static_cast<usize>(m_impl->stream.gcount());
    }

    usize FileHandle::Write(const void* data, usize sizeBytes)
    {
        DT_ASSERT(IsOpen(), "FileHandle::Write called on unopened file");
        const auto posBefore = m_impl->stream.tellp();
        m_impl->stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(sizeBytes));
        const auto posAfter = m_impl->stream.tellp();
        return static_cast<usize>(posAfter - posBefore);
    }

    void FileHandle::Seek(usize offset)
    {
        DT_ASSERT(IsOpen(), "FileHandle::Seek called on unopened file");
        m_impl->stream.seekg(static_cast<std::streamoff>(offset));
        m_impl->stream.seekp(static_cast<std::streamoff>(offset));
    }

    usize FileHandle::Tell() const
    {
        DT_ASSERT(IsOpen(), "FileHandle::Tell called on unopened file");
        return static_cast<usize>(m_impl->stream.tellg());
    }

    usize FileHandle::Size() const
    {
        DT_ASSERT(IsOpen(), "FileHandle::Size called on unopened file");
        std::error_code ec;
        const auto size = fs::file_size(m_impl->path, ec);
        return ec ? 0 : static_cast<usize>(size);
    }

    void FileHandle::Close()
    {
        if (m_impl != nullptr)
        {
            if (m_impl->stream.is_open())
            {
                m_impl->stream.close();
            }
            delete m_impl;
            m_impl = nullptr;
        }
    }

    // -------------------------------------------------------------------

    FileSystem& FileSystem::Get()
    {
        static FileSystem instance;
        return instance;
    }

    void FileSystem::SetContentRoot(const std::string& rootPath)
    {
        m_contentRoot = NormalizeSeparators(rootPath);
    }

    std::string FileSystem::NormalizeSeparators(const std::string& path)
    {
        std::string result = path;
        for (char& c : result)
        {
            if (c == '\\')
            {
                c = '/';
            }
        }
        return result;
    }

    std::string FileSystem::ResolvePath(const std::string& path) const
    {
        const std::string normalized = NormalizeSeparators(path);
        fs::path asPath(normalized);

        if (asPath.is_absolute())
        {
            return normalized;
        }

        if (m_contentRoot.empty())
        {
            return normalized;
        }

        fs::path resolved = fs::path(m_contentRoot) / asPath;
        return NormalizeSeparators(resolved.string());
    }

    std::optional<FileHandle> FileSystem::Open(const std::string& path, FileOpenMode mode)
    {
        const std::string resolved = ResolvePath(path);

        std::ios_base::openmode stdMode = std::ios_base::binary;
        switch (mode)
        {
            case FileOpenMode::Read:      stdMode |= std::ios_base::in; break;
            case FileOpenMode::Write:     stdMode |= std::ios_base::out | std::ios_base::trunc; break;
            case FileOpenMode::ReadWrite: stdMode |= std::ios_base::in | std::ios_base::out; break;
            case FileOpenMode::Append:    stdMode |= std::ios_base::out | std::ios_base::app; break;
        }

        auto* impl = new FileHandle::Impl();
        impl->path = resolved;
        impl->stream.open(resolved, stdMode);

        if (!impl->stream.is_open())
        {
            DT_LOG_WARN(LogCategory::FileSystem, "Failed to open file: {}", resolved);
            delete impl;
            return std::nullopt;
        }

        FileHandle handle;
        handle.m_impl = impl;
        return handle;
    }

    bool FileSystem::Exists(const std::string& path) const
    {
        std::error_code ec;
        return fs::exists(ResolvePath(path), ec);
    }

    FileStats FileSystem::Stat(const std::string& path) const
    {
        FileStats stats;
        std::error_code ec;
        const std::string resolved = ResolvePath(path);

        stats.exists = fs::exists(resolved, ec);
        if (!stats.exists || ec)
        {
            return stats;
        }

        stats.isDirectory = fs::is_directory(resolved, ec);

        if (!stats.isDirectory)
        {
            stats.sizeBytes = static_cast<usize>(fs::file_size(resolved, ec));
        }

        const auto ftime = fs::last_write_time(resolved, ec);
        if (!ec)
        {
            const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            stats.lastModifiedUnixMs = static_cast<u64>(
                std::chrono::duration_cast<std::chrono::milliseconds>(sctp.time_since_epoch()).count());
        }

        return stats;
    }

    bool FileSystem::CreateDirectoryRecursive(const std::string& path) const
    {
        std::error_code ec;
        fs::create_directories(ResolvePath(path), ec);
        if (ec)
        {
            DT_LOG_ERROR(LogCategory::FileSystem, "CreateDirectoryRecursive failed for '{}': {}", path, ec.message());
            return false;
        }
        return true;
    }

    std::vector<std::string> FileSystem::ListDirectory(const std::string& path, bool recursive) const
    {
        std::vector<std::string> results;
        std::error_code ec;
        const std::string resolved = ResolvePath(path);

        if (!fs::exists(resolved, ec) || !fs::is_directory(resolved, ec))
        {
            return results;
        }

        if (recursive)
        {
            for (const auto& entry : fs::recursive_directory_iterator(resolved, ec))
            {
                results.push_back(NormalizeSeparators(entry.path().string()));
            }
        }
        else
        {
            for (const auto& entry : fs::directory_iterator(resolved, ec))
            {
                results.push_back(NormalizeSeparators(entry.path().string()));
            }
        }

        return results;
    }

    std::optional<std::vector<u8>> FileSystem::ReadEntireFile(const std::string& path) const
    {
        const std::string resolved = ResolvePath(path);
        std::ifstream stream(resolved, std::ios::binary | std::ios::ate);

        if (!stream.is_open())
        {
            return std::nullopt;
        }

        const std::streamsize size = stream.tellg();
        stream.seekg(0, std::ios::beg);

        std::vector<u8> buffer(static_cast<usize>(size));
        if (size > 0 && !stream.read(reinterpret_cast<char*>(buffer.data()), size))
        {
            return std::nullopt;
        }

        return buffer;
    }

    bool FileSystem::WriteEntireFile(const std::string& path, const void* data, usize sizeBytes) const
    {
        const std::string resolved = ResolvePath(path);

        // Ensure parent directory exists - a common source of silent save
        // failures is a first-time save into a directory that hasn't been
        // created yet (e.g. a new profile's save folder).
        const fs::path parent = fs::path(resolved).parent_path();
        if (!parent.empty())
        {
            std::error_code ec;
            fs::create_directories(parent, ec);
        }

        std::ofstream stream(resolved, std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            DT_LOG_ERROR(LogCategory::FileSystem, "WriteEntireFile failed to open '{}'", resolved);
            return false;
        }

        stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(sizeBytes));
        return stream.good();
    }

    std::string FileSystem::GetExtension(const std::string& path)
    {
        return NormalizeSeparators(fs::path(path).extension().string());
    }

    std::string FileSystem::GetFileNameWithoutExtension(const std::string& path)
    {
        return fs::path(path).stem().string();
    }

    std::string FileSystem::GetParentDirectory(const std::string& path)
    {
        return NormalizeSeparators(fs::path(path).parent_path().string());
    }
}
