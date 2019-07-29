#include "terrain.h"

namespace
{
	std::array<tilePosStruct, 8> children(const tilePosStruct &pos)
	{
		std::array<tilePosStruct, 8> res;
		for (uint32 i = 0; i < 8; i++)
		{
			res[i] = pos;
			res[i].radius /= 2;
			res[i].x += (((i / 1) % 2) == 0 ? -1 : 1) * res[i].radius;
			res[i].y += (((i / 2) % 2) == 0 ? -1 : 1) * res[i].radius;
			res[i].z += (((i / 2) / 2) == 0 ? -1 : 1) * res[i].radius;
		}
		return res;
	}

	bool coarsenessTest(const tilePosStruct &pos)
	{
		real d = pos.distanceToPlayer();
		return d > pos.radius * 1.5;
	}

	void traverse(tilePosStruct pos, std::set<tilePosStruct> &tilesRequests, const std::set<tilePosStruct> &tilesReady)
	{
		if (pos.radius <= 4 || coarsenessTest(pos))
		{
			pos.visible = true;
			tilesRequests.insert(pos);
			return;
		}

		auto cs = children(pos);
		bool ok = true;
		for (const auto &p : cs)
			ok = ok && tilesReady.count(p) > 0;
		if (ok)
		{
			for (const auto &p : cs)
				traverse(p, tilesRequests, tilesReady);
		}
		else
		{
			for (auto &p : cs)
			{
				p.visible = false;
				tilesRequests.insert(p);
			}
		}
		pos.visible = !ok;
		tilesRequests.insert(pos);
	}
}

std::set<tilePosStruct> findNeededTiles(const std::set<tilePosStruct> &tilesReady)
{
	OPTICK_EVENT("findNeededTiles");
	std::set<tilePosStruct> tilesRequests;
	tilePosStruct pt;
	pt.x = numeric_cast<sint32>(playerPosition[0] / 64) * 64;
	pt.y = numeric_cast<sint32>(playerPosition[1] / 64) * 64;
	pt.z = numeric_cast<sint32>(playerPosition[2] / 64) * 64;
	for (sint32 z = -1; z < 2; z += 1)
	{
		for (sint32 y = -1; y < 2; y += 1)
		{
			for (sint32 x = -1; x < 2; x += 1)
			{
				tilePosStruct r(pt);
				r.radius = 32;
				r.x += 64 * x;
				r.y += 64 * y;
				r.z += 64 * z;
				traverse(r, tilesRequests, tilesReady);
			}
		}
	}
	//CAGE_LOG(severityEnum::Info, "terrain", string() + "ready: " + tilesReady.size() + ", requested: " + tilesRequests.size());
	return tilesRequests;
}
