#ifndef baseTile_h_dsfg7d8f5
#define baseTile_h_dsfg7d8f5

#include <set>
#include <vector>
#include <array>

#include "../common.h"

struct TilePos
{
	sint32 x, y, z; // center at the finest level
	sint32 radius;
	bool visible;

	TilePos();
	aabb getBox() const; // aabb in world space
	transform getTransform() const;
	real distanceToPlayer() const;
	bool operator < (const TilePos &other) const;
};

inline stringizer &operator + (stringizer &s, const TilePos &p)
{
	return s + p.x + " " + p.y + " " + p.z + " " + p.radius;
}

struct Vertex
{
	vec3 position;
	vec3 normal;
	//vec3 tangent;
	//vec3 bitangent;
	vec2 uv;
};

std::set<TilePos> findNeededTiles(const std::set<TilePos> &tilesReady);
void terrainGenerate(const TilePos &tilePos, std::vector<Vertex> &meshVertices, std::vector<uint32> &meshIndices, Holder<Image> &albedo, Holder<Image> &special);
void textureData(Holder<Image> &albedo, Holder<Image> &special, std::vector<vec3> &positions, std::vector<vec3> &normals, std::vector<uint32> &xs, std::vector<uint32> &ys);
void imageInpaint(Image *img, uint32 radius);
Holder<NoiseFunction> newClouds(uint32 seed, uint32 octaves);
uint32 globalSeed();

#endif // !baseTile_h_dsfg7d8f5
