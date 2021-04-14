#include "common.h"

#include <cage-core/entities.h>
#include <cage-core/hashString.h>
#include <cage-core/geometry.h>
#include <cage-core/collider.h>
#include <cage-core/collisionStructure.h>

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

void renderDebugRay(const Line &ln, const vec3 &color, uint32 duration)
{
	CAGE_ASSERT(ln.normalized());
	Entity *e = engineEntities()->createAnonymous();
	GAME_COMPONENT(Timeout, to, e);
	to.ttl = duration;
	CAGE_COMPONENT_ENGINE(Render, r, e);
	r.object = HashString("flittermouse/laser/laser.obj");
	r.color = color;
	CAGE_COMPONENT_ENGINE(Transform, t, e);
	t.position = ln.origin;
	t.orientation = quat(ln.direction, vec3(0, 0, 1));
	t.scale = ln.maximum;
}

vec3 terrainIntersection(const Line &ln)
{
	CAGE_ASSERT(ln.isSegment());
	if (!collisionSearchQuery->query(ln))
		return vec3::Nan();
	CAGE_ASSERT(collisionSearchQuery->collisionPairs().size() >= 1);
	const Collider *c = nullptr;
	transform tr;
	collisionSearchQuery->collider(c, tr);
	Triangle t = c->triangles()[collisionSearchQuery->collisionPairs()[0].b];
	t *= tr;
	vec3 r = intersection(ln, t);
	CAGE_ASSERT(r.valid());
	//renderDebugRay(makeSegment(ln.origin, r));
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
