// Copyright Neofilisoft. All Rights Reserved.
#include "IsometricDepthSorter.h"

namespace lacrima::sim
{
    void IsometricDepthSorter::Sort(std::vector<SortedSpriteEntry>& entries)
    {
        std::sort(entries.begin(), entries.end(), [](const SortedSpriteEntry& a, const SortedSpriteEntry& b)
        {
            if (a.depthKey != b.depthKey)
            {
                return a.depthKey < b.depthKey;
            }
            return a.entity.index < b.entity.index;
        });
    }
}
