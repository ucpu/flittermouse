#include "common.h"

#include <cage-core/geometry.h>
#include <cage-core/collider.h>
#include <cage-core/collisionStructure.h>

#include <cage-engine/core.h>
#include <cage-engine/engine.h>

using namespace cage;

namespace
{
	Holder<CollisionStructure> collisionSearchData;
	Holder<CollisionQuery> collisionSearchQuery;
	bool collisionSearchNeedsRebuild;

	void engineInitialize()
	{
		collisionSearchData = newCollisionStructure({});
		collisionSearchQuery = newCollisionQuery(collisionSearchData.get());
	}

	void engineUpdate()
	{
		// nothing?
	}

	class Callbacks
	{
		EventListener<void()> engineInitListener;
		EventListener<void()> engineUpdateListener;
	public:
		Callbacks()
		{
			engineInitListener.attach(controlThread().initialize);
			engineInitListener.bind<&engineInitialize>();
			engineUpdateListener.attach(controlThread().update);
			engineUpdateListener.bind<&engineUpdate>();
		}
	} callbacksInstance;
}

vec3 terrainIntersection(const line &ln)
{
	collisionSearchQuery->query(ln);
	if (collisionSearchQuery->name() == 0)
		return vec3::Nan();
	CAGE_ASSERT(collisionSearchQuery->collisionPairs().size() == 1);
	const Collider *c = nullptr;
	transform tr;
	collisionSearchQuery->collider(c, tr);
	triangle t = c->triangles()[collisionSearchQuery->collisionPairs()[0].b];
	t *= tr;
	vec3 r = intersection(ln, t);
	CAGE_ASSERT(r.valid());
	return r;
}

void terrainAddCollider(uint32 name, Collider *c, const transform &tr)
{
	CAGE_ASSERT(tr.valid());
	CAGE_ASSERT(c);
	CAGE_ASSERT(c->box().valid());
	collisionSearchData->update(name, c, tr);
	collisionSearchNeedsRebuild = true;
}

void terrainRemoveCollider(uint32 name)
{
	collisionSearchData->remove(name);
	collisionSearchNeedsRebuild = true;
}

void terrainRebuildColliders()
{
	if (!collisionSearchNeedsRebuild)
		return;
	collisionSearchNeedsRebuild = false;
	OPTICK_EVENT("terrainRebuildColliders");
	collisionSearchData->rebuild();
}
