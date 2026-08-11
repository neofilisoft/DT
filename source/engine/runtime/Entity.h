#pragma once

#include "core/handle/Handle.h"

// ---------------------------------------------------------------------------
// Entity.h
//
// The concrete Entity type is just Handle<EntityTag> - see the comment atop
// core/containers/ComponentArray.h for why Core only depends on the generic
// Handle<T> shape and not on this concrete alias. Runtime is where the tag
// gets attached and every simulation module (Time, Needs, Relationship, AI,
// ...) keys its ComponentArray<Entity, T> instances against this same type.
// ---------------------------------------------------------------------------

namespace dt
{
    struct EntityTag {};
    using Entity = Handle<EntityTag>;
}
