#include "common.h"

#include <cage-core/geometry.h>
#include <cage-core/collisionMesh.h>
#include <cage-core/collision.h>

#include <cage-engine/core.h>
#include <cage-engine/engine.h>

using namespace cage;

namespace
{
	holder<collisionData> collisionSearchData;
	holder<collisionQuery> collisionSearchQuery;

	void engineInitialize()
	{
		collisionSearchData = newCollisionData(collisionDataCreateConfig());
		collisionSearchQuery = newCollisionQuery(collisionSearchData.get());
	}

	void engineUpdate()
	{
		// nothing?
	}

	class callbacksInitClass
	{
		eventListener<void()> engineInitListener;
		eventListener<void()> engineUpdateListener;
	public:
		callbacksInitClass()
		{
			engineInitListener.attach(controlThread().initialize);
			engineInitListener.bind<&engineInitialize>();
			engineUpdateListener.attach(controlThread().update);
			engineUpdateListener.bind<&engineUpdate>();
		}
	} callbacksInitInstance;
}

vec3 terrainIntersection(const line &ln)
{
	collisionSearchQuery->query(ln);
	if (collisionSearchQuery->name() == 0)
		return vec3();
	CAGE_ASSERT(collisionSearchQuery->collisionPairs().size() == 1);
	const collisionMesh *c = nullptr;
	transform tr;
	collisionSearchQuery->collider(c, tr);
	triangle t = c->triangles()[collisionSearchQuery->collisionPairs()[0].b];
	t *= tr;
	vec3 r = intersection(ln, t);
	CAGE_ASSERT(r.valid());
	return r;
}

void terrainAddCollider(uint32 name, collisionMesh *c, const transform &tr)
{
	CAGE_ASSERT(tr.valid());
	CAGE_ASSERT(c);
	CAGE_ASSERT(c->box().valid());
	collisionSearchData->update(name, c, tr);
	collisionSearchData->rebuild();
}

void terrainRemoveCollider(uint32 name)
{
	collisionSearchData->remove(name);
	collisionSearchData->rebuild();
}
