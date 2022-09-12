#ifndef flittermouse_common_h_sdg456ds4hg6
#define flittermouse_common_h_sdg456ds4hg6

#include <cage-core/math.h>

namespace cage
{
	class Collider;
	class EntityGroup;
}

using namespace cage;

void renderDebugRay(const Line &ln, const Vec3 &color = Vec3(), uint32 duration = 1);

Vec3 terrainIntersection(const Line &ln);
void terrainAddCollider(uint32 name, Holder<Collider> c, const Transform &tr);
void terrainRemoveCollider(uint32 name);
void terrainRebuildColliders();

struct TimeoutComponent
{
	uint32 ttl = 10;
};

extern EntityGroup *entitiesToDestroy;
extern Vec3 playerPosition;
extern Real terrainGenerationProgress;

#endif
