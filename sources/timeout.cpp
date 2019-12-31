#include "common.h"

#include <cage-core/entities.h>
#include <cage-core/hashString.h>
#include <cage-core/color.h>

#include <cage-engine/core.h>
#include <cage-engine/engine.h>

EntityGroup *entitiesToDestroy;
EntityComponent *TimeoutComponent::component;

TimeoutComponent::TimeoutComponent() : ttl(0)
{}

namespace
{
	void engineUpdate()
	{
		OPTICK_EVENT("timeout & entities destroy");
		for (Entity *e : TimeoutComponent::component->entities())
		{
			GAME_COMPONENT(Timeout, t, e);
			if (t.ttl-- == 0)
				e->add(entitiesToDestroy);
		}
		entitiesToDestroy->destroy();
	}

	void engineInitialize()
	{
		entitiesToDestroy = entities()->defineGroup();
		TimeoutComponent::component = entities()->defineComponent(TimeoutComponent(), true);
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
