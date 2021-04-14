#include "terrain.h"

#include <cage-core/geometry.h>

Aabb TilePos::getBox() const
{
	return Aabb(vec3(-1), vec3(1)) * getTransform();
}

transform TilePos::getTransform() const
{
	return transform(vec3(pos[0], pos[1], pos[2]), quat(), radius);
}

real TilePos::distanceToPlayer() const
{
	return distance(getBox(), playerPosition);
}

bool TilePos::operator < (const TilePos &other) const
{
	if (pos == other.pos)
		return radius < other.radius;
	return detail::memcmp(&pos, &other.pos, sizeof(pos)) < 0;
}
