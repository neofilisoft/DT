#pragma once

#include "core/platform/BuildConfig.h"
#include "core/platform/Types.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------
// Reflection.h
//
// Macro-based compile-time reflection, NOT an external codegen tool
// (no Unreal-Header-Tool-style separate build pass). Rationale, per the
// architecture discussion: a 1-3 person team cannot afford to author and
// maintain a custom C++ parser/codegen tool as a build dependency - every
// bug in that tool becomes a blocking build problem with no upstream to
// fix it. The REFLECT_BEGIN/REFLECT_FIELD/REFLECT_END macro sequence
// instead generates a static TypeInfo directly inside the class body at
// normal compile time, no extra build step, no generated files to keep in
// sync with the source.
//
// This single declaration is the source of truth consumed by three
// separate systems:
//   1. Serialization (core/serialization) walks TypeInfo::fields to write
//      both the binary format and the JSON diagnostic mirror, so the two
//      formats can never drift out of sync with each other or with the
//      actual struct layout.
//   2. Lua binding (via sol2, wired in the scripting layer above Core)
//      iterates the same fields to expose them to Lua interaction scripts
//      without hand-writing a second binding table per type.
//   3. The Editor's Inspector panel walks the same TypeInfo to
//      automatically generate property-edit widgets for any reflected
//      type, so adding a new field to e.g. NeedsComponent makes it appear
//      in the Inspector with zero editor-side code changes.
//
// FieldInfo stores a type-erased getter/setter pair (raw byte offset +
// FieldType tag) rather than templated accessor functions, specifically so
// TypeInfo itself is a plain, non-template runtime object that can be
// looked up by name/hash at runtime (needed for both the JSON
// deserializer, which sees field names as strings from a file, and the
// Editor, which needs to enumerate fields of a type not known until the
// user selects an object at runtime).
// ---------------------------------------------------------------------------

namespace dt
{
    enum class FieldType : u8
    {
        Bool,
        I8, I16, I32, I64,
        U8, U16, U32, U64,
        F32, F64,
        String,
        Vec2, Vec3, Vec4, Quat, Mat4,
        UUID,
        HandleGeneric,   // Handle<T> for some T; underlying storage is always {u32,u32}
        NestedStruct,    // Field is itself a reflected type; TypeInfo::nestedType points at its TypeInfo
        DynamicArray,    // std::vector<T>; TypeInfo::nestedType points at element TypeInfo (nullptr for primitive elements, use elementFieldType)
        Enum
    };

    struct TypeInfo;

    struct FieldInfo
    {
        std::string_view name;
        FieldType type;
        usize byteOffset;
        usize elementSize;             // sizeof(T) for the field's own type (or element type, for DynamicArray)
        const TypeInfo* nestedType;    // non-null only for NestedStruct and DynamicArray-of-struct
        FieldType elementFieldType;    // valid only for DynamicArray: primitive type of elements, if not NestedStruct

        // Type-erased std::vector<T> accessors, valid only when
        // type == FieldType::DynamicArray. These are plain (capture-less
        // lambda-derived) function pointers rather than std::function,
        // captured inside REFLECT_FIELD_ARRAY at the point where the
        // concrete element type T is still known to the compiler. This is
        // the mechanism that lets BinaryWriter/BinaryReader/JsonWriter
        // walk an arbitrary std::vector<T> field without themselves ever
        // knowing T - they call through these function pointers instead of
        // relying on any assumption about std::vector<T>'s memory layout
        // being independent of T (which is not something the C++ standard
        // guarantees, so reinterpret_cast-ing a vector<T> as a generic
        // byte-vector view would be undefined behavior, however commonly
        // it happens to work in practice on a given STL implementation).
        usize (*arraySize)(const void* fieldPtr) = nullptr;
        void (*arrayResizeForRead)(void* fieldPtr, usize newSize) = nullptr;
        void* (*arrayElementAt)(void* fieldPtr, usize index) = nullptr;
        const void* (*arrayElementAtConst)(const void* fieldPtr, usize index) = nullptr;
    };

    struct TypeInfo
    {
        std::string_view name;
        u64 nameHash;                  // FNV-1a of name; stable across process runs, used as the on-disk type tag in binary saves
        usize sizeBytes;
        std::vector<FieldInfo> fields;
        void (*constructInPlace)(void* dest);
        void (*destroyInPlace)(void* obj);
        void (*copyConstruct)(void* dest, const void* src);
    };

    constexpr u64 FnvHash(std::string_view str)
    {
        u64 hash = 14695981039346656037ULL;
        for (char c : str)
        {
            hash ^= static_cast<u64>(static_cast<unsigned char>(c));
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    // Registry of every reflected type, keyed by name hash. Populated by
    // static registration objects generated inside REFLECT_END (see below) -
    // each reflected type gets exactly one static TypeInfoRegistrar
    // instance whose constructor runs during static init and registers the
    // type's TypeInfo* into this global map, so any type can be looked up
    // by name at runtime (required for JSON deserialization, where the type
    // name is just a string read from a file).
    class TypeRegistry
    {
    public:
        static TypeRegistry& Get();

        void Register(const TypeInfo* info);
        const TypeInfo* Find(u64 nameHash) const;
        const TypeInfo* Find(std::string_view name) const;

    private:
        std::vector<const TypeInfo*> m_types;
    };

    template <typename T>
    struct TypeInfoRegistrar
    {
        TypeInfoRegistrar()
        {
            TypeRegistry::Get().Register(&T::StaticTypeInfo());
        }
    };
}

// ---------------------------------------------------------------------------
// Macros. Usage:
//
//   struct NeedsComponent
//   {
//       f32 hunger = 100.0f;
//       f32 bladder = 100.0f;
//       dt::Vec3 lastSatisfiedLocation;
//
//       REFLECT_BEGIN(NeedsComponent)
//           REFLECT_FIELD(hunger)
//           REFLECT_FIELD(bladder)
//           REFLECT_FIELD(lastSatisfiedLocation)
//       REFLECT_END()
//   };
//
// REFLECT_BEGIN declares a static StaticTypeInfo() accessor and opens a
// static-local TypeInfo being built up; each REFLECT_FIELD appends one
// FieldInfo computed via offsetof; REFLECT_END finalizes it and defines the
// static registrar instance that self-registers into TypeRegistry at
// static-init time.
// ---------------------------------------------------------------------------

#define REFLECT_BEGIN(ClassName)                                                                    \
    public:                                                                                          \
    using DT_ReflectedSelf = ClassName;                                                              \
    static const ::dt::TypeInfo& StaticTypeInfo()                                                    \
    {                                                                                                \
        static ::dt::TypeInfo info = []() {                                                          \
            ::dt::TypeInfo t;                                                                        \
            t.name = #ClassName;                                                                     \
            t.nameHash = ::dt::FnvHash(#ClassName);                                                   \
            t.sizeBytes = sizeof(ClassName);                                                         \
            t.constructInPlace = [](void* dest) { new (dest) ClassName(); };                         \
            t.destroyInPlace = [](void* obj) { static_cast<ClassName*>(obj)->~ClassName(); };        \
            t.copyConstruct = [](void* dest, const void* src) {                                      \
                new (dest) ClassName(*static_cast<const ClassName*>(src));                            \
            };

#define DT_REFLECT_FIELD_IMPL(FieldName, FieldTypeTag)                                               \
            t.fields.push_back(::dt::FieldInfo{                                                      \
                #FieldName,                                                                          \
                FieldTypeTag,                                                                        \
                offsetof(DT_ReflectedSelf, FieldName),                                                \
                sizeof(decltype(DT_ReflectedSelf::FieldName)),                                        \
                ::dt::DT_NestedTypeOf<decltype(DT_ReflectedSelf::FieldName)>::Get(),                   \
                ::dt::FieldType::Bool /* unused for non-array fields */                               \
            });

// REFLECT_FIELD infers FieldType from the field's declared C++ type via
// the DT_FIELD_TYPE_OF trait below, so call sites never manually spell out
// a FieldType enumerator (a common source of subtle mismatches if hand
// typed - e.g. declaring an f64 field but tagging it FieldType::F32 would
// silently corrupt binary serialization).
#define REFLECT_FIELD(FieldName) \
    DT_REFLECT_FIELD_IMPL(FieldName, (::dt::DT_FieldTypeOf<decltype(DT_ReflectedSelf::FieldName)>::value))

// REFLECT_FIELD_ARRAY is used for std::vector<T> fields specifically
// (rather than folding this into REFLECT_FIELD's inference) because a
// DynamicArray field needs a second piece of information beyond its own
// FieldType tag: the element type's FieldType, so the serializer knows
// how to read/write each element without needing T's full TypeInfo when T
// is a primitive. Keeping this as an explicit, separate macro means a
// call site declaring REFLECT_FIELD_ARRAY(items) is self-documenting about
// the field being a variable-length array, rather than that fact being
// silently inferred from the C++ type alone.
#define REFLECT_FIELD_ARRAY(FieldName)                                                                \
    do {                                                                                               \
        using VecT = decltype(DT_ReflectedSelf::FieldName);                                            \
        using ElemT = typename VecT::value_type;                                                       \
        ::dt::FieldInfo fi;                                                                            \
        fi.name = #FieldName;                                                                          \
        fi.type = ::dt::FieldType::DynamicArray;                                                       \
        fi.byteOffset = offsetof(DT_ReflectedSelf, FieldName);                                          \
        fi.elementSize = sizeof(ElemT);                                                                 \
        fi.nestedType = ::dt::DT_NestedTypeOf<ElemT>::Get();                                           \
        fi.elementFieldType = ::dt::DT_FieldTypeOf<ElemT>::value;                               \
        fi.arraySize = [](const void* p) -> ::dt::usize {                                              \
            return static_cast<const VecT*>(p)->size();                                                \
        };                                                                                              \
        fi.arrayResizeForRead = [](void* p, ::dt::usize n) {                                            \
            static_cast<VecT*>(p)->resize(n);                                                           \
        };                                                                                              \
        fi.arrayElementAt = [](void* p, ::dt::usize i) -> void* {                                      \
            return &(*static_cast<VecT*>(p))[i];                                                        \
        };                                                                                              \
        fi.arrayElementAtConst = [](const void* p, ::dt::usize i) -> const void* {                     \
            return &(*static_cast<const VecT*>(p))[i];                                                  \
        };                                                                                              \
        t.fields.push_back(fi);                                                                        \
    } while (0);

#define REFLECT_END()                                                                                \
            return t;                                                                                \
        }();                                                                                         \
        return info;                                                                                 \
    }                                                                                                 \
    static inline ::dt::TypeInfoRegistrar<DT_ReflectedSelf> DT_ReflectionRegistrar{};

// ---------------------------------------------------------------------------
// DT_FieldTypeOf: compile-time trait mapping a C++ type to its FieldType
// tag. Specialized for every primitive and math type; a reflected nested
// struct type is detected via the presence of ::DT_ReflectedSelf (any type
// that went through REFLECT_BEGIN defines this member typedef), so nested
// reflected structs do not need a manual specialization here - only the
// primitive/math leaf types do.
// ---------------------------------------------------------------------------

namespace dt
{
    template <typename T, typename = void>
    struct DT_FieldTypeOf
    {
        // Fallback: any type that defines DT_ReflectedSelf (i.e. went
        // through REFLECT_BEGIN itself) is treated as a nested struct.
        // A type matching neither a specialization below nor this
        // nested-reflected-type pattern is a compile error via
        // static_assert, since serializing an unrecognized type silently
        // as raw bytes would be a correctness hazard (endianness,
        // padding, pointer members).
        static_assert(sizeof(T) == 0, "DTEngine: type used in REFLECT_FIELD has no FieldType mapping. "
            "Add a DT_FieldTypeOf specialization, or ensure the nested type itself went through REFLECT_BEGIN/REFLECT_END.");
        static constexpr FieldType value = FieldType::NestedStruct;
    };

    template <typename T>
    struct DT_FieldTypeOf<T, std::void_t<typename T::DT_ReflectedSelf>>
    {
        static constexpr FieldType value = FieldType::NestedStruct;
    };

    template <typename T>
    struct DT_FieldTypeOf<T, std::enable_if_t<std::is_enum_v<T>>>
    {
        static constexpr FieldType value = FieldType::Enum;
    };

    // Guards against a field declared as std::vector<T> being run through
    // plain REFLECT_FIELD (which would otherwise fall through to the
    // "unmapped type" static_assert below with a confusing message) -
    // this specialization gives a directly actionable compile error
    // pointing the author at REFLECT_FIELD_ARRAY instead.
    template <typename T>
    struct DT_FieldTypeOf<std::vector<T>>
    {
        static_assert(sizeof(T) == 0,
            "DTEngine: std::vector<T> fields must use REFLECT_FIELD_ARRAY(field), not REFLECT_FIELD(field).");
        static constexpr FieldType value = FieldType::DynamicArray;
    };

    // DT_NestedTypeOf<T>::Get() returns T::StaticTypeInfo() address for any
    // reflected struct element type, or nullptr for primitive element
    // types. Used by REFLECT_FIELD_ARRAY to populate FieldInfo::nestedType
    // for arrays-of-structs (e.g. std::vector<GeneEntry> in the Genetics
    // module) without needing a separate macro variant per element
    // category.
    template <typename T, typename = void>
    struct DT_NestedTypeOf
    {
        static const TypeInfo* Get() { return nullptr; }
    };

    template <typename T>
    struct DT_NestedTypeOf<T, std::void_t<typename T::DT_ReflectedSelf>>
    {
        static const TypeInfo* Get() { return &T::StaticTypeInfo(); }
    };

    #define DT_DECLARE_FIELD_TYPE(CppType, Tag) \
        template <> struct DT_FieldTypeOf<CppType> { static constexpr FieldType value = FieldType::Tag; };

    DT_DECLARE_FIELD_TYPE(bool, Bool)
    DT_DECLARE_FIELD_TYPE(i8, I8)
    DT_DECLARE_FIELD_TYPE(i16, I16)
    DT_DECLARE_FIELD_TYPE(i32, I32)
    DT_DECLARE_FIELD_TYPE(i64, I64)
    DT_DECLARE_FIELD_TYPE(u8, U8)
    DT_DECLARE_FIELD_TYPE(u16, U16)
    DT_DECLARE_FIELD_TYPE(u32, U32)
    DT_DECLARE_FIELD_TYPE(u64, U64)
    DT_DECLARE_FIELD_TYPE(f32, F32)
    DT_DECLARE_FIELD_TYPE(f64, F64)
    DT_DECLARE_FIELD_TYPE(std::string, String)
}
