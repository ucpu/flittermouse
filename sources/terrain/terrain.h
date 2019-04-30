#ifndef baseTile_h_dsfg7d8f5
#define baseTile_h_dsfg7d8f5

#include <set>
#include <vector>

#include "../common.h"

const real tileLength = 10; // real world size of a tile (in 1 dimension)
const real distanceToLoadTile = tileLength * 5;
const real distanceToUnloadTile = tileLength * 6;

struct tilePosStruct
{
	sint32 x, y, z;

	tilePosStruct();
	bool operator < (const tilePosStruct &other) const;
	real distanceToPlayer() const;
	transform getTransform() const;
	operator string () const;
};

struct vertexStruct
{
	vec3 position;
	vec3 normal;
	//vec3 tangent;
	//vec3 bitangent;
	vec2 uv;
};

std::set<tilePosStruct> findNeededTiles();
void terrainGenerate(const tilePosStruct &tilePos, std::vector<vertexStruct> &meshVertices, std::vector<uint32> &meshIndices, holder<pngImageClass> &albedo, holder<pngImageClass> &special);

#endif // !baseTile_h_dsfg7d8f5
