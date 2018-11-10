#ifndef flittermouse_common_h_sdg456ds4hg6
#define flittermouse_common_h_sdg456ds4hg6

#include <cage-core/core.h>
#include <cage-core/math.h>

using namespace cage;

real terrainDensity(const vec3 &pos);
vec3 terrainIntersection(const line &ln);
void terrainAddCollider(uint32 name, colliderClass *c, const transform &tr);
void terrainRemoveCollider(uint32 name);

vec3 colorDeviation(const vec3 &color, real deviation);

struct timeoutComponent
{
	static componentClass *component;
	uint32 ttl;
	timeoutComponent();
};

#define GAME_GET_COMPONENT(T,C,E) ::CAGE_JOIN(T, Component) &C = (E)->value<::CAGE_JOIN(T, Component)>(::CAGE_JOIN(T, Component)::component);

extern groupClass *entitiesToDestroy;
extern vec3 playerPosition;

#endif
