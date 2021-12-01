#include "common.h"

#include <cage-core/entities.h>
#include <cage-core/hashString.h>
#include <cage-core/color.h>
#include <cage-engine/scene.h>
#include <cage-simple/engine.h>

EntityGroup *entitiesToDestroy;

namespace
{
	void engineUpdate()
	{
		for (Entity *e : engineEntities()->component<TimeoutComponent>()->entities())
		{
			TimeoutComponent &t = e->value<TimeoutComponent>();
			if (t.ttl-- == 0)
				e->add(entitiesToDestroy);
		}
		entitiesToDestroy->destroy();
	}

	void engineInitialize()
	{
		entitiesToDestroy = engineEntities()->defineGroup();
		engineEntities()->defineComponent(TimeoutComponent());
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
