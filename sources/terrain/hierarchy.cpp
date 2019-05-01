#include "terrain.h"

#include <cage-core/log.h>
#include <cage-core/geometry.h>

std::set<tilePosStruct> findNeededTiles(const std::set<tilePosStruct> &tilesReady)
{
	std::set<tilePosStruct> neededTiles;
	tilePosStruct pt;
	pt.x = numeric_cast<sint32>(playerPosition[0] / 10) * 10;
	pt.y = numeric_cast<sint32>(playerPosition[1] / 10) * 10;
	pt.z = numeric_cast<sint32>(playerPosition[2] / 10) * 10;
	tilePosStruct r;
	r.radius = 5;
	for (r.z = pt.z - 100; r.z <= pt.z + 100; r.z += 10)
	{
		for (r.y = pt.y - 100; r.y <= pt.y + 100; r.y += 10)
		{
			for (r.x = pt.x - 100; r.x <= pt.x + 100; r.x += 10)
			{
				if (r.distanceToPlayer() < 50)
					neededTiles.insert(r);
			}
		}
	}
	return neededTiles;
}
