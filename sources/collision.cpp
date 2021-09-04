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
		collisionSearchQuery = newCollisionQuery(collisionSearchData.share());
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

void renderDebugRay(const Line &ln, const Vec3 &color, uint32 duration)
{
	CAGE_ASSERT(ln.normalized());
	Entity *e = engineEntities()->createAnonymous();
	GAME_COMPONENT(Timeout, to, e);
	to.ttl = duration;
	RenderComponent &r = e->value<RenderComponent>();
	r.object = HashString("flittermouse/laser/laser.obj");
	r.color = color;
	TransformComponent &t = e->value<TransformComponent>();
	t.position = ln.origin;
	t.orientation = Quat(ln.direction, Vec3(0, 0, 1));
	t.scale = ln.maximum;
}

Vec3 terrainIntersection(const Line &ln)
{
	CAGE_ASSERT(ln.isSegment());
	if (!collisionSearchQuery->query(ln))
		return Vec3::Nan();
	CAGE_ASSERT(collisionSearchQuery->collisionPairs().size() >= 1);
	Holder<const Collider> c;
	Transform tr;
	collisionSearchQuery->collider(c, tr);
	Triangle t = c->triangles()[collisionSearchQuery->collisionPairs()[0].b];
	t *= tr;
	Vec3 r = intersection(ln, t);
	CAGE_ASSERT(r.valid());
	//renderDebugRay(makeSegment(ln.origin, r));
	return r;
}

void terrainAddCollider(uint32 name, Holder<Collider> c, const Transform &tr)
{
	CAGE_ASSERT(tr.valid());
	CAGE_ASSERT(c);
	CAGE_ASSERT(c->box().valid());
	collisionSearchData->update(name, std::move(c), tr);
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
	collisionSearchData->rebuild();
}
