
#include "terrain.h"

#include <cage-core/log.h>
#include <cage-core/geometry.h>
#include <cage-core/noise.h>
#include <cage-core/color.h>
#include <cage-core/random.h>
#include <cage-core/png.h>

namespace
{
	const uint32 globalSeed = (uint32)currentRandomGenerator().next();
}

vec3 colorDeviation(const vec3 &color, real deviation)
{
	vec3 hsv = convertRgbToHsv(color) + (randomChance3() - 0.5) * deviation;
	hsv[0] = (hsv[0] + 1) % 1;
	return convertHsvToRgb(clamp(hsv, vec3(), vec3(1, 1, 1)));
}

void terrainGenerate(std::vector<vertexStruct> &meshVertices, std::vector<uint32> &meshIndices, holder<pngImageClass> &albedo, holder<pngImageClass> &special)
{
	vertexStruct v;
	meshVertices.push_back(v);
	meshIndices.push_back(0);
	meshIndices.push_back(0);
	meshIndices.push_back(0);
	albedo = newPngImage();
	albedo->empty(1, 1, 3);
	special = newPngImage();
	special->empty(1, 1, 2);
	// todo
}
