#ifndef baseTile_h_dsfg7d8f5
#define baseTile_h_dsfg7d8f5

#include "../common.h"

#include <set>

struct TilePos
{
	Vec3i pos; 
	sint32 radius = 0;
	bool visible = true;

	Aabb getBox() const; // aabb in world space
	Transform getTransform() const;
	Real distanceToPlayer() const;
	bool operator < (const TilePos &other) const;
};

inline Stringizer &operator + (Stringizer &s, const TilePos &p)
{
	return s + p.radius + "__" + p.pos[0] + "_" + p.pos[1] + "_" + p.pos[2];
}

std::set<TilePos> findNeededTiles(const std::set<TilePos> &tilesReady);
void terrainGenerate(const TilePos &tilePos, Holder<Mesh> &mesh, Holder<Collider> &collider, Holder<Image> &albedo, Holder<Image> &special);

#endif // !baseTile_h_dsfg7d8f5
