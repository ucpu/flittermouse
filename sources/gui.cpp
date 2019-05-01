#include "common.h"

#include <cage-core/entities.h>

#include <cage-client/core.h>
#include <cage-client/engine.h>
#include <cage-client/gui.h>

namespace
{
	uint32 playerPositionLabel;

	bool engineUpdate()
	{
		entityManagerClass *ents = gui()->entities();

		{ // player position
			GUI_GET_COMPONENT(text, t, ents->get(playerPositionLabel));
			t.value = playerPosition;
		}

		return false;
	}

	bool engineInitialize()
	{
		entityManagerClass *ents = gui()->entities();

		entityClass *panel = nullptr;
		{ // panel
			entityClass *wrapper = ents->createUnique();
			GUI_GET_COMPONENT(scrollbars, sc, wrapper);
			panel = ents->createUnique();
			GUI_GET_COMPONENT(parent, parent, panel);
			parent.parent = wrapper->name();
			GUI_GET_COMPONENT(panel, g, panel);
			GUI_GET_COMPONENT(layoutTable, lt, panel);
		}

		{ // player position
			{ // label
				entityClass *e = ents->createUnique();
				GUI_GET_COMPONENT(parent, p, e);
				p.parent = panel->name();
				p.order = 1;
				GUI_GET_COMPONENT(label, l, e);
				GUI_GET_COMPONENT(text, t, e);
				t.value = "Position: ";
			}
			{ // value
				entityClass *e = ents->createUnique();
				GUI_GET_COMPONENT(parent, p, e);
				p.parent = panel->name();
				p.order = 2;
				GUI_GET_COMPONENT(label, l, e);
				GUI_GET_COMPONENT(text, t, e);
				playerPositionLabel = e->name();
			}
		}

		return false;
	}

	class callbacksInitClass
	{
		eventListener<bool()> engineInitListener;
		eventListener<bool()> engineUpdateListener;
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
