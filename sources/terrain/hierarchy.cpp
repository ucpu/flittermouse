#include "terrain.h"

#include <cage-core/log.h>
#include <cage-core/geometry.h>

std::set<tilePosStruct> findNeededTiles()
{
	std::set<tilePosStruct> neededTiles;
	tilePosStruct pt;
	pt.x = numeric_cast<sint32>(playerPosition[0] / tileLength);
	pt.y = numeric_cast<sint32>(playerPosition[1] / tileLength);
	pt.z = numeric_cast<sint32>(playerPosition[2] / tileLength);
	tilePosStruct r;
	for (r.z = pt.z - 10; r.z <= pt.z + 10; r.z++)
	{
		for (r.y = pt.y - 10; r.y <= pt.y + 10; r.y++)
		{
			for (r.x = pt.x - 10; r.x <= pt.x + 10; r.x++)
			{
				if (r.distanceToPlayer() < distanceToLoadTile)
					neededTiles.insert(r);
			}
		}
	}
	return neededTiles;
}
