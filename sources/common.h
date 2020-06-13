#ifndef flittermouse_common_h_sdg456ds4hg6
#define flittermouse_common_h_sdg456ds4hg6

#include <cage-core/math.h>

#include <optick.h>

using namespace cage;

vec3 terrainIntersection(const line &ln);
void terrainAddCollider(uint32 name, Collider *c, const transform &tr);
void terrainRemoveCollider(uint32 name);
void terrainRebuildColliders();

struct TimeoutComponent
{
	static EntityComponent *component;
	uint32 ttl;
	TimeoutComponent();
};

#define GAME_COMPONENT(T, C, E) ::T##Component &C = E->value<::T##Component>(::T##Component::component);

extern EntityGroup *entitiesToDestroy;
extern vec3 playerPosition;
extern real terrainGenerationProgress;

#endif
