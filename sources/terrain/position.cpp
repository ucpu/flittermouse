#include "terrain.h"

tilePosStruct::tilePosStruct() : x(0), y(0), z(0)
{}

bool tilePosStruct::operator < (const tilePosStruct &other) const
{
	if (z == other.z)
	{
		if (y == other.y)
			return x < other.x;
		return y < other.y;
	}
	return z < other.z;
}

real tilePosStruct::distanceToPlayer() const
{
	return distance(vec3(x, y, z) * tileLength, playerPosition);
}

transform tilePosStruct::getTransform() const
{
	return transform(vec3(x, y, z) * tileLength);
}

tilePosStruct::operator string () const
{
	return string() + x + " " + y + " " + z;
}
