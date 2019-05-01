#include "terrain.h"

#include <cage-core/geometry.h>

tilePosStruct::tilePosStruct() : x(0), y(0), z(0), radius(0), visible(true)
{}

aabb tilePosStruct::getBox() const
{
	return aabb(vec3(-1), vec3(1)) * getTransform();
}

transform tilePosStruct::getTransform() const
{
	return transform(vec3(x, y, z), quat(), radius);
}

real tilePosStruct::distanceToPlayer() const
{
	return distance(getBox(), playerPosition);
}

bool tilePosStruct::operator < (const tilePosStruct &other) const
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

tilePosStruct::operator string () const
{
	return string() + x + " " + y + " " + z + " " + radius;
}
