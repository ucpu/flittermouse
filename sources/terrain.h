#ifndef baseTile_h_dsfg7d8f5
#define baseTile_h_dsfg7d8f5

#include <set>
#include <vector>

#include "common.h"

struct vertexStruct
{
	vec3 position;
	vec3 normal;
	//vec3 tangent;
	//vec3 bitangent;
	vec2 uv;
};

void terrainGenerate(std::vector<vertexStruct> &meshVertices, std::vector<uint32> &meshIndices, holder<pngImageClass> &albedo, holder<pngImageClass> &special);

#endif // !baseTile_h_dsfg7d8f5
