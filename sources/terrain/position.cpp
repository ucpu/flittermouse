#include "terrain.h"

#include <cage-core/geometry.h>

Aabb TilePos::getBox() const
{
	return Aabb(Vec3(-1), Vec3(1)) * getTransform();
}

Transform TilePos::getTransform() const
{
	return Transform(Vec3(pos[0], pos[1], pos[2]), Quat(), radius);
}

Real TilePos::distanceToPlayer() const
{
	return distance(getBox(), playerPosition);
}

bool TilePos::operator < (const TilePos &other) const
{
	if (pos == other.pos)
		return radius < other.radius;
	return detail::memcmp(&pos, &other.pos, sizeof(pos)) < 0;
}
