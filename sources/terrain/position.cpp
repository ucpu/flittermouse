#include "terrain.h"

#include <cage-core/geometry.h>

TilePos::TilePos() : x(0), y(0), z(0), radius(0), visible(true)
{}

aabb TilePos::getBox() const
{
	return aabb(vec3(-1), vec3(1)) * getTransform();
}

transform TilePos::getTransform() const
{
	return transform(vec3(x, y, z), quat(), radius);
}

real TilePos::distanceToPlayer() const
{
	return distance(getBox(), playerPosition);
}

bool TilePos::operator < (const TilePos &other) const
{
	if (z == other.z)
	{
		if (y == other.y)
		{
			if (x == other.x)
				return radius < other.radius;
			return x < other.x;
		}
		return y < other.y;
	}
	return z < other.z;
}
