#pragma once
#include "core/platform/Types.h"
#include <string>
#include <vector>
namespace lacrima::sim {
enum class RaycastSide : u8 { Vertical = 0, Horizontal = 1 };
struct Raycast2DHit { bool hit=false; f32 distance=0,worldX=0,worldY=0,wallU=0; i32 cellX=-1,cellY=-1; u16 textureIndex=0; RaycastSide side=RaycastSide::Vertical; };
class GridRaycastScene {
public:
 void Resize(u32 width,u32 height,bool solidBorder=false);
 void Clear();
 // Loads a deterministic ASCII grid: . or space is empty, 1-9 are wall texture indices.
 bool LoadAsciiFile(const std::string& path,bool solidBorder=false);
 u32 Width() const { return m_width; } u32 Height() const { return m_height; }
 void SetCell(i32 x,i32 y,u16 textureIndex); bool IsSolid(i32 x,i32 y) const; u16 TextureAt(i32 x,i32 y) const;
 Raycast2DHit Cast(f32 originX,f32 originY,f32 directionX,f32 directionY,f32 maxDistance) const;
 std::vector<Raycast2DHit> CastColumns(f32 originX,f32 originY,f32 directionX,f32 directionY,f32 planeX,f32 planeY,u32 columnCount,f32 maxDistance) const;
private:
 usize Index(u32 x,u32 y) const { return static_cast<usize>(y)*m_width+x; }
 u32 m_width=0,m_height=0; std::vector<u16> m_cells;
};
}
