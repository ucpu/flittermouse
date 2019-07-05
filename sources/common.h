#ifndef flittermouse_common_h_sdg456ds4hg6
#define flittermouse_common_h_sdg456ds4hg6

#include <cage-core/core.h>
#include <cage-core/math.h>
#include <cage-core/log.h>

using namespace cage;

vec3 terrainIntersection(const line &ln);
void terrainAddCollider(uint32 name, collisionMesh *c, const transform &tr);
void terrainRemoveCollider(uint32 name);

struct timeoutComponent
{
	static entityComponent *component;
	uint32 ttl;
	timeoutComponent();
};

#define GAME_GET_COMPONENT(T,C,E) ::CAGE_JOIN(T, Component) &C = (E)->value<::CAGE_JOIN(T, Component)>(::CAGE_JOIN(T, Component)::component);

extern entityGroup *entitiesToDestroy;
extern vec3 playerPosition;

#endif
