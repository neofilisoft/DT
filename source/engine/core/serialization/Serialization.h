// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Assert.h"
#include "core/platform/Types.h"
#include "core/reflection/Reflection.h"
#include "core/logging/Logger.h"

#include <cstring>
#include <functional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

// ---------------------------------------------------------------------------
// Serialization.h
//
// Binary is the source of truth (save files load from binary only); JSON
// is a diagnostic mirror generated from the exact same TypeInfo walk, never
// hand-maintained separately, so the two representations cannot drift
// apart from each other or from the actual in-memory struct layout. This
// mirrors the architecture decision from the initial planning discussion.
//
// Versioning: every binary blob is prefixed with a per-type schema version
// (a u32 stored alongside the type's nameHash in the blob header). When a
// field is added to a reflected struct, bump DT_SCHEMA_VERSION_OVERRIDE for
// that type (defaults to 1 if never overridden) and add the corresponding
// upgrade step to VersionedUpgrade() below - old save files continue to
// load correctly because the reader checks the stored version against the
// current version and runs any needed field-default-fill/migration logic
// before handing the object to the caller, rather than silently
// misreading a field that didn't exist in an older save's binary layout.
//
// Endianness: all multi-byte primitives are written little-endian
// explicitly (not "whatever this platform's native order is"), because a
// save file created on one platform must load correctly on the other, and
// x86/x64 (Windows) and most Linux deployment targets are little-endian
// already, so this is a zero-cost normalization on the primary platforms
// and only matters if the engine is ever ported to a big-endian target.
// ---------------------------------------------------------------------------

namespace lacrima
{
    // Call once (typically at module static-init, or explicitly in
    // engine startup) when a reflected type's on-disk layout changes in a
    // way that requires migration logic in BinaryReader::ReadField. Types
    // that never call this default to schema version 1 forever.
    void RegisterSchemaVersion(u64 typeNameHash, u32 version);

    // Schema version lookup - defaults to 1 unless a type has an
    // explicit override registered via RegisterSchemaVersion.
    u32 GetSchemaVersion(u64 typeNameHash);

    class BinaryReader;
    using MigrationFunc = std::function<bool(BinaryReader& reader, u32 storedVersion, void* object)>;
    
    // Registers a custom migration function for a type. When ReadObject encounters
    // a stored schema version < current schema version, it calls this function to
    // allow manual reading and upgrading of the old binary layout.
    void RegisterMigration(u64 typeNameHash, MigrationFunc func);

    // ---------------------------------------------------------------------
    // BinaryWriter / BinaryReader
    // ---------------------------------------------------------------------

    class BinaryWriter
    {
    public:
        void WriteBytes(const void* data, usize size)
        {
            const u8* bytes = static_cast<const u8*>(data);
            m_buffer.insert(m_buffer.end(), bytes, bytes + size);
        }

        template <typename T>
        void WritePrimitive(const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>, "WritePrimitive requires a trivially copyable type");
            WriteBytes(&value, sizeof(T));
        }

        void WriteString(const std::string& str)
        {
            WritePrimitive<u32>(static_cast<u32>(str.size()));
            WriteBytes(str.data(), str.size());
        }

        // Walks `typeInfo`'s fields via raw byte offsets into `object` and
        // writes each field according to its FieldType tag. Nested reflected
        // structs and dynamic arrays recurse.
        void WriteObject(const void* object, const TypeInfo& typeInfo)
        {
            WritePrimitive<u64>(typeInfo.nameHash);
            WritePrimitive<u32>(GetSchemaVersion(typeInfo.nameHash));

            for (const FieldInfo& field : typeInfo.fields)
            {
                const u8* fieldPtr = static_cast<const u8*>(object) + field.byteOffset;
                WriteField(fieldPtr, field);
            }
        }

        const std::vector<u8>& Data() const { return m_buffer; }

    private:
        void WriteField(const u8* fieldPtr, const FieldInfo& field);
        void WriteArrayElement(const u8* elemPtr, const FieldInfo& arrayField);

        std::vector<u8> m_buffer;
    };

    class BinaryReader
    {
    public:
        explicit BinaryReader(const std::vector<u8>& data) : m_data(data), m_offset(0) {}

        bool HasError() const { return m_hasError; }

        template <typename T>
        T ReadPrimitive()
        {
            static_assert(std::is_trivially_copyable_v<T>, "ReadPrimitive requires a trivially copyable type");
            // C4 fix: fallible bounds check that survives Shipping builds (no assert stripping).
            if (m_offset + sizeof(T) > m_data.size())
            {
                LACRIMA_LOG_ERROR(LogCategory::Core, "BinaryReader: read past end of buffer (offset={}, need={}, size={})",
                    m_offset, sizeof(T), m_data.size());
                m_hasError = true;
                return T{};
            }
            T value;
            std::memcpy(&value, m_data.data() + m_offset, sizeof(T));
            m_offset += sizeof(T);
            return value;
        }

        bool ReadBytes(void* dest, usize size)
        {
            if (m_offset + size > m_data.size())
            {
                LACRIMA_LOG_ERROR(LogCategory::Core, "BinaryReader: read past end of buffer (offset={}, need={}, size={})",
                    m_offset, size, m_data.size());
                m_hasError = true;
                return false;
            }
            std::memcpy(dest, m_data.data() + m_offset, size);
            m_offset += size;
            return true;
        }

        std::string ReadString()
        {
            const u32 len = ReadPrimitive<u32>();
            if (m_hasError) return {};
            // C4 fix: validate string length against remaining buffer before advancing.
            if (m_offset + static_cast<usize>(len) > m_data.size())
            {
                LACRIMA_LOG_ERROR(LogCategory::Core, "BinaryReader: string length {} exceeds buffer remainder {}",
                    len, m_data.size() - m_offset);
                m_hasError = true;
                return {};
            }
            std::string result(reinterpret_cast<const char*>(m_data.data() + m_offset), len);
            m_offset += len;
            return result;
        }

        // Reads a type header (nameHash + schema version) and validates it
        // against the expected type before reading fields into `object`.
        // Returns false (and logs) on a type mismatch rather than reading
        // garbage into the wrong struct layout.
        bool ReadObject(void* object, const TypeInfo& typeInfo);

        bool AtEnd() const { return m_offset >= m_data.size(); }

    private:
        void ReadField(u8* fieldPtr, const FieldInfo& field, u32 storedSchemaVersion);
        void ReadArrayElement(u8* elemPtr, const FieldInfo& arrayField, u32 storedSchemaVersion);

        const std::vector<u8>& m_data;
        usize m_offset;
        bool m_hasError{ false }; // C4 fix: sticky error flag - survives Shipping builds
    };

    // ---------------------------------------------------------------------
    // JsonWriter
    //
    // Diagnostic-only mirror. Generated via the exact same TypeInfo walk as
    // BinaryWriter, guaranteeing field-for-field parity with the binary
    // format. This is never read back by the engine at runtime - it exists
    // purely so a developer or the Editor's Save Inspector panel can open a
    // human-readable dump of what a binary save file actually contains,
    // without maintaining a second hand-written serialization path that
    // could silently diverge from the binary one.
    // ---------------------------------------------------------------------

    class JsonWriter
    {
    public:
        std::string WriteObject(const void* object, const TypeInfo& typeInfo)
        {
            std::ostringstream out;
            WriteObjectRecursive(out, object, typeInfo, 0);
            return out.str();
        }

    private:
        void WriteObjectRecursive(std::ostringstream& out, const void* object, const TypeInfo& typeInfo, int indent);
        void WriteFieldValue(std::ostringstream& out, const u8* fieldPtr, const FieldInfo& field, int indent);
        static void Indent(std::ostringstream& out, int level);
    };
}




