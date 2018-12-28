
#include "common.h"

#include <cage-core/log.h>
#include <cage-core/entities.h>
#include <cage-core/hashString.h>
#include <cage-core/color.h>

#include <cage-client/core.h>
#include <cage-client/engine.h>

groupClass *entitiesToDestroy;
componentClass *timeoutComponent::component;

timeoutComponent::timeoutComponent() : ttl(0)
{}

namespace
{
	void engineUpdate()
	{
		for (entityClass *e : timeoutComponent::component->entities())
		{
			GAME_GET_COMPONENT(timeout, t, e);
			if (t.ttl-- == 0)
				e->add(entitiesToDestroy);
		}
		entitiesToDestroy->destroy();
	}

	void engineInitialize()
	{
		entitiesToDestroy = entities()->defineGroup();
		timeoutComponent::component = entities()->defineComponent(timeoutComponent(), true);
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