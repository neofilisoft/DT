#include "core/serialization/Serialization.h"
#include "core/handle/Handle.h"
#include "core/logging/Logger.h"
#include "core/math/Math.h"
#include "core/uuid/UUID.h"

#include <cstring>
#include <unordered_map>

namespace dt
{
    // -----------------------------------------------------------------------
    // Schema version table. A separate translation-unit-local map rather
    // than storing the version inside TypeInfo itself, because TypeInfo is
    // built once as a function-local static inside StaticTypeInfo() (see
    // Reflection.h) at first-use time, and schema bumps are a
    // serialization-system concern, not a reflection-system concern - a
    // type's shape (TypeInfo::fields) and its on-disk version number are
    // orthogonal, so they are tracked in orthogonal places.
    // -----------------------------------------------------------------------
    static std::unordered_map<u64, u32>& SchemaVersionTable()
    {
        static std::unordered_map<u64, u32> table;
        return table;
    }

    u32 BinaryWriter::GetSchemaVersion(u64 typeNameHash)
    {
        auto it = SchemaVersionTable().find(typeNameHash);
        return (it != SchemaVersionTable().end()) ? it->second : 1u;
    }

    void RegisterSchemaVersion(u64 typeNameHash, u32 version)
    {
        SchemaVersionTable()[typeNameHash] = version;
    }

    // -----------------------------------------------------------------------
    // BinaryWriter::WriteField
    // -----------------------------------------------------------------------
    void BinaryWriter::WriteField(const u8* fieldPtr, const FieldInfo& field)
    {
        switch (field.type)
        {
            case FieldType::Bool:   WritePrimitive<u8>(*reinterpret_cast<const bool*>(fieldPtr) ? 1 : 0); break;
            case FieldType::I8:     WritePrimitive<i8>(*reinterpret_cast<const i8*>(fieldPtr)); break;
            case FieldType::I16:    WritePrimitive<i16>(*reinterpret_cast<const i16*>(fieldPtr)); break;
            case FieldType::I32:    WritePrimitive<i32>(*reinterpret_cast<const i32*>(fieldPtr)); break;
            case FieldType::I64:    WritePrimitive<i64>(*reinterpret_cast<const i64*>(fieldPtr)); break;
            case FieldType::U8:     WritePrimitive<u8>(*reinterpret_cast<const u8*>(fieldPtr)); break;
            case FieldType::U16:    WritePrimitive<u16>(*reinterpret_cast<const u16*>(fieldPtr)); break;
            case FieldType::U32:    WritePrimitive<u32>(*reinterpret_cast<const u32*>(fieldPtr)); break;
            case FieldType::U64:    WritePrimitive<u64>(*reinterpret_cast<const u64*>(fieldPtr)); break;
            case FieldType::F32:    WritePrimitive<f32>(*reinterpret_cast<const f32*>(fieldPtr)); break;
            case FieldType::F64:    WritePrimitive<f64>(*reinterpret_cast<const f64*>(fieldPtr)); break;
            case FieldType::String: WriteString(*reinterpret_cast<const std::string*>(fieldPtr)); break;

            case FieldType::Vec2:
            {
                const Vec2& v = *reinterpret_cast<const Vec2*>(fieldPtr);
                WritePrimitive<f32>(v.x); WritePrimitive<f32>(v.y);
                break;
            }
            case FieldType::Vec3:
            {
                const Vec3& v = *reinterpret_cast<const Vec3*>(fieldPtr);
                WritePrimitive<f32>(v.x); WritePrimitive<f32>(v.y); WritePrimitive<f32>(v.z);
                break;
            }
            case FieldType::Vec4:
            {
                const Vec4& v = *reinterpret_cast<const Vec4*>(fieldPtr);
                WritePrimitive<f32>(v.x); WritePrimitive<f32>(v.y); WritePrimitive<f32>(v.z); WritePrimitive<f32>(v.w);
                break;
            }
            case FieldType::Quat:
            {
                const Quat& q = *reinterpret_cast<const Quat*>(fieldPtr);
                WritePrimitive<f32>(q.x); WritePrimitive<f32>(q.y); WritePrimitive<f32>(q.z); WritePrimitive<f32>(q.w);
                break;
            }
            case FieldType::Mat4:
            {
                const Mat4& m = *reinterpret_cast<const Mat4*>(fieldPtr);
                for (int i = 0; i < 16; ++i) WritePrimitive<f32>(m.m[i]);
                break;
            }
            case FieldType::UUID:
            {
                const UUID& id = *reinterpret_cast<const UUID*>(fieldPtr);
                WritePrimitive<u64>(id.High());
                WritePrimitive<u64>(id.Low());
                break;
            }
            case FieldType::HandleGeneric:
            {
                // Handle<T> layout is always {u32 index, u32 generation}
                // regardless of T (see Handle.h) - safe to reinterpret as a
                // fixed 8-byte struct without knowing T at this call site.
                struct RawHandle { u32 index; u32 generation; };
                const RawHandle& h = *reinterpret_cast<const RawHandle*>(fieldPtr);
                WritePrimitive<u32>(h.index);
                WritePrimitive<u32>(h.generation);
                break;
            }
            case FieldType::NestedStruct:
            {
                DT_ASSERT(field.nestedType != nullptr, "NestedStruct field missing TypeInfo pointer");
                WriteObject(fieldPtr, *field.nestedType);
                break;
            }
            case FieldType::DynamicArray:
            {
                DT_ASSERT(field.arraySize != nullptr, "DynamicArray field missing arraySize accessor (was REFLECT_FIELD_ARRAY used?)");

                const usize count = field.arraySize(fieldPtr);
                WritePrimitive<u32>(static_cast<u32>(count));

                for (usize i = 0; i < count; ++i)
                {
                    const void* elemPtr = field.arrayElementAtConst(fieldPtr, i);
                    WriteArrayElement(static_cast<const u8*>(elemPtr), field);
                }
                break;
            }
            case FieldType::Enum:
                DT_ASSERT(false, "Enum field serialization requires explicit per-enum support (not yet wired for this field)");
                break;
        }
    }

    void BinaryWriter::WriteArrayElement(const u8* elemPtr, const FieldInfo& arrayField)
    {
        // Builds a synthetic FieldInfo describing a single element so the
        // existing WriteField dispatch (primitives, math types, nested
        // structs) can be reused verbatim rather than duplicating the
        // entire switch statement for "array element" vs "plain field".
        FieldInfo elemField;
        elemField.type = (arrayField.elementFieldType == FieldType::NestedStruct)
            ? FieldType::NestedStruct
            : arrayField.elementFieldType;
        elemField.nestedType = arrayField.nestedType;
        WriteField(elemPtr, elemField);
    }

    // -----------------------------------------------------------------------
    // BinaryReader::ReadObject / ReadField
    // -----------------------------------------------------------------------
    bool BinaryReader::ReadObject(void* object, const TypeInfo& typeInfo)
    {
        const u64 storedHash = ReadPrimitive<u64>();
        const u32 storedVersion = ReadPrimitive<u32>();

        if (storedHash != typeInfo.nameHash)
        {
            DT_LOG_ERROR(LogCategory::Serialization,
                "BinaryReader::ReadObject type mismatch: expected '{}' (hash {}), stream has hash {}",
                std::string(typeInfo.name), typeInfo.nameHash, storedHash);
            return false;
        }

        for (const FieldInfo& field : typeInfo.fields)
        {
            u8* fieldPtr = static_cast<u8*>(object) + field.byteOffset;
            ReadField(fieldPtr, field, storedVersion);
        }
        return true;
    }

    void BinaryReader::ReadField(u8* fieldPtr, const FieldInfo& field, u32 storedSchemaVersion)
    {
        // storedSchemaVersion is threaded through to ReadArrayElement (for
        // recursive per-element migration logic) and is available here for
        // future direct per-field migration branches, though no type has
        // needed one yet.
        switch (field.type)
        {
            case FieldType::Bool:   *reinterpret_cast<bool*>(fieldPtr) = ReadPrimitive<u8>() != 0; break;
            case FieldType::I8:     *reinterpret_cast<i8*>(fieldPtr) = ReadPrimitive<i8>(); break;
            case FieldType::I16:    *reinterpret_cast<i16*>(fieldPtr) = ReadPrimitive<i16>(); break;
            case FieldType::I32:    *reinterpret_cast<i32*>(fieldPtr) = ReadPrimitive<i32>(); break;
            case FieldType::I64:    *reinterpret_cast<i64*>(fieldPtr) = ReadPrimitive<i64>(); break;
            case FieldType::U8:     *reinterpret_cast<u8*>(fieldPtr) = ReadPrimitive<u8>(); break;
            case FieldType::U16:    *reinterpret_cast<u16*>(fieldPtr) = ReadPrimitive<u16>(); break;
            case FieldType::U32:    *reinterpret_cast<u32*>(fieldPtr) = ReadPrimitive<u32>(); break;
            case FieldType::U64:    *reinterpret_cast<u64*>(fieldPtr) = ReadPrimitive<u64>(); break;
            case FieldType::F32:    *reinterpret_cast<f32*>(fieldPtr) = ReadPrimitive<f32>(); break;
            case FieldType::F64:    *reinterpret_cast<f64*>(fieldPtr) = ReadPrimitive<f64>(); break;
            case FieldType::String: *reinterpret_cast<std::string*>(fieldPtr) = ReadString(); break;

            case FieldType::Vec2:
            {
                Vec2& v = *reinterpret_cast<Vec2*>(fieldPtr);
                v.x = ReadPrimitive<f32>(); v.y = ReadPrimitive<f32>();
                break;
            }
            case FieldType::Vec3:
            {
                Vec3& v = *reinterpret_cast<Vec3*>(fieldPtr);
                v.x = ReadPrimitive<f32>(); v.y = ReadPrimitive<f32>(); v.z = ReadPrimitive<f32>();
                break;
            }
            case FieldType::Vec4:
            {
                Vec4& v = *reinterpret_cast<Vec4*>(fieldPtr);
                v.x = ReadPrimitive<f32>(); v.y = ReadPrimitive<f32>(); v.z = ReadPrimitive<f32>(); v.w = ReadPrimitive<f32>();
                break;
            }
            case FieldType::Quat:
            {
                Quat& q = *reinterpret_cast<Quat*>(fieldPtr);
                q.x = ReadPrimitive<f32>(); q.y = ReadPrimitive<f32>(); q.z = ReadPrimitive<f32>(); q.w = ReadPrimitive<f32>();
                break;
            }
            case FieldType::Mat4:
            {
                Mat4& m = *reinterpret_cast<Mat4*>(fieldPtr);
                for (int i = 0; i < 16; ++i) m.m[i] = ReadPrimitive<f32>();
                break;
            }
            case FieldType::UUID:
            {
                const u64 high = ReadPrimitive<u64>();
                const u64 low = ReadPrimitive<u64>();
                // UUID has no public mutable-field constructor by design
                // (identity should not be casually reassignable) - reader
                // reconstructs via placement using the private layout
                // through a trivial byte copy, since UUID is itself
                // trivially copyable (two u64 members, no virtuals).
                struct RawUUID { u64 high; u64 low; };
                RawUUID raw{ high, low };
                std::memcpy(fieldPtr, &raw, sizeof(RawUUID));
                break;
            }
            case FieldType::HandleGeneric:
            {
                struct RawHandle { u32 index; u32 generation; };
                RawHandle h;
                h.index = ReadPrimitive<u32>();
                h.generation = ReadPrimitive<u32>();
                std::memcpy(fieldPtr, &h, sizeof(RawHandle));
                break;
            }
            case FieldType::NestedStruct:
            {
                DT_ASSERT(field.nestedType != nullptr, "NestedStruct field missing TypeInfo pointer");
                ReadObject(fieldPtr, *field.nestedType);
                break;
            }
            case FieldType::DynamicArray:
            {
                DT_ASSERT(field.arrayResizeForRead != nullptr, "DynamicArray field missing arrayResizeForRead accessor (was REFLECT_FIELD_ARRAY used?)");

                const u32 count = ReadPrimitive<u32>();
                field.arrayResizeForRead(fieldPtr, count);

                for (u32 i = 0; i < count; ++i)
                {
                    void* elemPtr = field.arrayElementAt(fieldPtr, i);
                    ReadArrayElement(static_cast<u8*>(elemPtr), field, storedSchemaVersion);
                }
                break;
            }
            case FieldType::Enum:
                DT_ASSERT(false, "Enum field deserialization requires explicit per-enum support (not yet wired for this field)");
                break;
        }
    }

    void BinaryReader::ReadArrayElement(u8* elemPtr, const FieldInfo& arrayField, u32 storedSchemaVersion)
    {
        FieldInfo elemField;
        elemField.type = (arrayField.elementFieldType == FieldType::NestedStruct)
            ? FieldType::NestedStruct
            : arrayField.elementFieldType;
        elemField.nestedType = arrayField.nestedType;
        ReadField(elemPtr, elemField, storedSchemaVersion);
    }

    // -----------------------------------------------------------------------
    // JsonWriter
    // -----------------------------------------------------------------------
    void JsonWriter::Indent(std::ostringstream& out, int level)
    {
        for (int i = 0; i < level; ++i)
        {
            out << "  ";
        }
    }

    void JsonWriter::WriteFieldValue(std::ostringstream& out, const u8* fieldPtr, const FieldInfo& field, int indent)
    {
        switch (field.type)
        {
            case FieldType::Bool:   out << (*reinterpret_cast<const bool*>(fieldPtr) ? "true" : "false"); break;
            case FieldType::I8:     out << static_cast<int>(*reinterpret_cast<const i8*>(fieldPtr)); break;
            case FieldType::I16:    out << *reinterpret_cast<const i16*>(fieldPtr); break;
            case FieldType::I32:    out << *reinterpret_cast<const i32*>(fieldPtr); break;
            case FieldType::I64:    out << *reinterpret_cast<const i64*>(fieldPtr); break;
            case FieldType::U8:     out << static_cast<unsigned int>(*reinterpret_cast<const u8*>(fieldPtr)); break;
            case FieldType::U16:    out << *reinterpret_cast<const u16*>(fieldPtr); break;
            case FieldType::U32:    out << *reinterpret_cast<const u32*>(fieldPtr); break;
            case FieldType::U64:    out << *reinterpret_cast<const u64*>(fieldPtr); break;
            case FieldType::F32:    out << *reinterpret_cast<const f32*>(fieldPtr); break;
            case FieldType::F64:    out << *reinterpret_cast<const f64*>(fieldPtr); break;
            case FieldType::String: out << '"' << *reinterpret_cast<const std::string*>(fieldPtr) << '"'; break;

            case FieldType::Vec2:
            {
                const Vec2& v = *reinterpret_cast<const Vec2*>(fieldPtr);
                out << "[" << v.x << ", " << v.y << "]";
                break;
            }
            case FieldType::Vec3:
            {
                const Vec3& v = *reinterpret_cast<const Vec3*>(fieldPtr);
                out << "[" << v.x << ", " << v.y << ", " << v.z << "]";
                break;
            }
            case FieldType::Vec4:
            {
                const Vec4& v = *reinterpret_cast<const Vec4*>(fieldPtr);
                out << "[" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << "]";
                break;
            }
            case FieldType::Quat:
            {
                const Quat& q = *reinterpret_cast<const Quat*>(fieldPtr);
                out << "[" << q.x << ", " << q.y << ", " << q.z << ", " << q.w << "]";
                break;
            }
            case FieldType::Mat4:
            {
                const Mat4& m = *reinterpret_cast<const Mat4*>(fieldPtr);
                out << "[";
                for (int i = 0; i < 16; ++i)
                {
                    out << m.m[i];
                    if (i != 15) out << ", ";
                }
                out << "]";
                break;
            }
            case FieldType::UUID:
            {
                const UUID& id = *reinterpret_cast<const UUID*>(fieldPtr);
                out << '"' << id.ToString() << '"';
                break;
            }
            case FieldType::HandleGeneric:
            {
                struct RawHandle { u32 index; u32 generation; };
                const RawHandle& h = *reinterpret_cast<const RawHandle*>(fieldPtr);
                out << "{ \"index\": " << h.index << ", \"generation\": " << h.generation << " }";
                break;
            }
            case FieldType::NestedStruct:
            {
                DT_ASSERT(field.nestedType != nullptr, "NestedStruct field missing TypeInfo pointer");
                out << "\n";
                WriteObjectRecursive(out, fieldPtr, *field.nestedType, indent + 1);
                break;
            }
            case FieldType::DynamicArray:
            {
                DT_ASSERT(field.arraySize != nullptr, "DynamicArray field missing arraySize accessor (was REFLECT_FIELD_ARRAY used?)");

                const usize count = field.arraySize(fieldPtr);
                if (count == 0)
                {
                    out << "[]";
                    break;
                }

                out << "[\n";
                FieldInfo elemField;
                elemField.type = (field.elementFieldType == FieldType::NestedStruct)
                    ? FieldType::NestedStruct
                    : field.elementFieldType;
                elemField.nestedType = field.nestedType;

                for (usize i = 0; i < count; ++i)
                {
                    const void* elemPtr = field.arrayElementAtConst(fieldPtr, i);
                    Indent(out, indent + 1);
                    WriteFieldValue(out, static_cast<const u8*>(elemPtr), elemField, indent + 1);
                    if (i != count - 1) out << ",";
                    out << "\n";
                }

                Indent(out, indent);
                out << "]";
                break;
            }
            case FieldType::Enum:
                out << "null";
                break;
        }
    }

    void JsonWriter::WriteObjectRecursive(std::ostringstream& out, const void* object, const TypeInfo& typeInfo, int indent)
    {
        Indent(out, indent);
        out << "{\n";

        for (usize i = 0; i < typeInfo.fields.size(); ++i)
        {
            const FieldInfo& field = typeInfo.fields[i];
            const u8* fieldPtr = static_cast<const u8*>(object) + field.byteOffset;

            Indent(out, indent + 1);
            out << '"' << field.name << "\": ";
            WriteFieldValue(out, fieldPtr, field, indent + 1);

            if (i != typeInfo.fields.size() - 1)
            {
                out << ",";
            }
            out << "\n";
        }

        Indent(out, indent);
        out << "}";
    }
}
