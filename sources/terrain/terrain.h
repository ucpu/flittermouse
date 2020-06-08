#ifndef baseTile_h_dsfg7d8f5
#define baseTile_h_dsfg7d8f5

#include "../common.h"

#include <set>

struct TilePos
{
	ivec3 pos; // center at the finest level
	sint32 radius = 0;
	bool visible = true;

	aabb getBox() const; // aabb in world space
	transform getTransform() const;
	real distanceToPlayer() const;
	bool operator < (const TilePos &other) const;
};

inline stringizer &operator + (stringizer &s, const TilePos &p)
{
	return s + p.radius + "__" + p.pos[0] + "_" + p.pos[1] + "_" + p.pos[2];
}

std::set<TilePos> findNeededTiles(const std::set<TilePos> &tilesReady);
void terrainGenerate(const TilePos &tilePos, Holder<Polyhedron> &mesh, Holder<Collider> &collider, Holder<Image> &albedo, Holder<Image> &special);

#endif // !baseTile_h_dsfg7d8f5
