#include "terrain.h"

#include <array>

namespace
{
	std::array<TilePos, 8> children(const TilePos &pos)
	{
		std::array<TilePos, 8> res;
		for (uint32 i = 0; i < 8; i++)
		{
			res[i] = pos;
			res[i].radius /= 2;
			res[i].pos[0] += (((i / 1) % 2) == 0 ? -1 : 1) * res[i].radius;
			res[i].pos[1] += (((i / 2) % 2) == 0 ? -1 : 1) * res[i].radius;
			res[i].pos[2] += (((i / 2) / 2) == 0 ? -1 : 1) * res[i].radius;
		}
		return res;
	}

	bool coarsenessTest(const TilePos &pos)
	{
		real d = pos.distanceToPlayer();
		return d > pos.radius * 4;
	}

	void traverse(TilePos pos, std::set<TilePos> &tilesRequests, const std::set<TilePos> &tilesReady)
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

std::set<TilePos> findNeededTiles(const std::set<TilePos> &tilesReady)
{
	OPTICK_EVENT("findNeededTiles");
	std::set<TilePos> tilesRequests;
	TilePos pt;
	constexpr sint32 TileSize = 32;
	pt.pos[0] = numeric_cast<sint32>(playerPosition[0] / TileSize) * TileSize;
	pt.pos[1] = numeric_cast<sint32>(playerPosition[1] / TileSize) * TileSize;
	pt.pos[2] = numeric_cast<sint32>(playerPosition[2] / TileSize) * TileSize;
	constexpr sint32 Range = 2;
	for (sint32 z = -Range; z <= Range; z += 1)
	{
		for (sint32 y = -Range; y <= Range; y += 1)
		{
			for (sint32 x = -Range; x <= Range; x += 1)
			{
				TilePos r(pt);
				r.radius = TileSize / 2;
				r.pos[0] += TileSize * x;
				r.pos[1] += TileSize * y;
				r.pos[2] += TileSize * z;
				traverse(r, tilesRequests, tilesReady);
			}
		}
	}
	CAGE_LOG_DEBUG(SeverityEnum::Info, "terrain", stringizer() + "ready: " + tilesReady.size() + ", requested: " + tilesRequests.size());
	return tilesRequests;
}
