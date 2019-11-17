#ifndef baseTile_h_dsfg7d8f5
#define baseTile_h_dsfg7d8f5

#include <set>
#include <vector>
#include <array>

#include "../common.h"

struct tilePosStruct
{
	sint32 x, y, z; // center at the finest level
	sint32 radius;
	bool visible;

	tilePosStruct();
	aabb getBox() const; // aabb in world space
	transform getTransform() const;
	real distanceToPlayer() const;
	bool operator < (const tilePosStruct &other) const;
};

inline stringizer &operator + (stringizer &s, const tilePosStruct &p)
{
	return s + p.x + " " + p.y + " " + p.z + " " + p.radius;
}

struct vertexStruct
{
	vec3 position;
	vec3 normal;
	//vec3 tangent;
	//vec3 bitangent;
	vec2 uv;
};

std::set<tilePosStruct> findNeededTiles(const std::set<tilePosStruct> &tilesReady);
void terrainGenerate(const tilePosStruct &tilePos, std::vector<vertexStruct> &meshVertices, std::vector<uint32> &meshIndices, holder<image> &albedo, holder<image> &special);
void inpaint(image *img, uint32 radius);

#endif // !baseTile_h_dsfg7d8f5
