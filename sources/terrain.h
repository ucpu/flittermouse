#ifndef baseTile_h_dsfg7d8f5
#define baseTile_h_dsfg7d8f5

#include <set>
#include <vector>

#include "common.h"

const real tileLength = 10; // real world size of a tile (in 1 dimension)

struct tilePosStruct
{
	sint32 x, y, z;

	tilePosStruct() : x(0), y(0), z(0)
	{}

	bool operator < (const tilePosStruct &other) const
	{
		if (z == other.z)
		{
			if (y == other.y)
				return x < other.x;
			return y < other.y;
		}
		return z < other.z;
	}

	real distanceToPlayer(real tileLength) const
	{
		return distance(vec3(x, y, z) * tileLength, playerPosition);
	}

	operator string () const { return string() + x + " " + y + " " + z; }
};

struct vertexStruct
{
	vec3 position;
	vec3 normal;
	//vec3 tangent;
	//vec3 bitangent;
	vec2 uv;
};

void terrainGenerate(const tilePosStruct &tilePos, std::vector<vertexStruct> &meshVertices, std::vector<uint32> &meshIndices, holder<pngImageClass> &albedo, holder<pngImageClass> &special);

#endif // !baseTile_h_dsfg7d8f5
