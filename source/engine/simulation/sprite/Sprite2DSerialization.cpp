// Copyright Neofilisoft. All Rights Reserved.
#include "simulation/sprite/Sprite2DComponent.h"
#include "core/serialization/Serialization.h"

namespace lacrima::sim
{
    namespace
    {
        struct Sprite2DSerializationRegistration
        {
            Sprite2DSerializationRegistration()
            {
                const u64 typeHash = Sprite2DComponent::StaticTypeInfo().nameHash;
                RegisterSchemaVersion(typeHash, 2);
                RegisterMigration(typeHash, [](BinaryReader& reader, u32 storedVersion, void* object)
                {
                    if (storedVersion != 1 || object == nullptr)
                        return false;

                    auto& sprite = *static_cast<Sprite2DComponent*>(object);
                    sprite = Sprite2DComponent{};

                    // Version 1 contained exactly the original reflected fields;
                    // fields added later were not present in the byte stream.
                    sprite.textureAsset = StringID(reader.ReadPrimitive<u64>());
                    sprite.width = reader.ReadPrimitive<f32>();
                    sprite.height = reader.ReadPrimitive<f32>();
                    sprite.pivotX = reader.ReadPrimitive<f32>();
                    sprite.pivotY = reader.ReadPrimitive<f32>();
                    sprite.tintR = reader.ReadPrimitive<f32>();
                    sprite.tintG = reader.ReadPrimitive<f32>();
                    sprite.tintB = reader.ReadPrimitive<f32>();
                    sprite.tintA = reader.ReadPrimitive<f32>();
                    sprite.renderLayer = reader.ReadPrimitive<i32>();
                    return true;
                });
            }
        };

        const Sprite2DSerializationRegistration g_sprite2DSerializationRegistration{};
    }
}
