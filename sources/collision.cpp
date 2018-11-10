
#include "common.h"

#include <cage-core/log.h>
#include <cage-core/geometry.h>
#include <cage-core/collider.h>
#include <cage-core/collision.h>

#include <cage-client/core.h>
#include <cage-client/engine.h>

using namespace cage;

namespace
{
	holder<collisionDataClass> collisionData;
	holder<collisionQueryClass> collisionQuery;

	void engineInitialize()
	{
		collisionData = newCollisionData(collisionDataCreateConfig());
		collisionQuery = newCollisionQuery(collisionData.get());
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
	collisionQuery->query(ln);
	if (collisionQuery->name() == 0)
		return vec3();
	CAGE_ASSERT_RUNTIME(collisionQuery->collisionPairsCount() == 1);
	const colliderClass *c = nullptr;
	transform tr;
	collisionQuery->collider(c, tr);
	triangle t = c->triangleData(collisionQuery->collisionPairsData()->b);
	t *= tr;
	vec3 r = intersection(ln, t);
	CAGE_ASSERT_RUNTIME(r.valid());
	return r;
}

void terrainAddCollider(uint32 name, colliderClass *c, const transform &tr)
{
	CAGE_ASSERT_RUNTIME(tr.valid());
	CAGE_ASSERT_RUNTIME(c);
	CAGE_ASSERT_RUNTIME(c->box().valid());
	collisionData->update(name, c, tr);
	collisionData->rebuild();
}

void terrainRemoveCollider(uint32 name)
{
	collisionData->remove(name);
	collisionData->rebuild();
}
