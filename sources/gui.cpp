#include "common.h"

#include <cage-core/entities.h>

#include <cage-engine/core.h>
#include <cage-engine/engine.h>
#include <cage-engine/gui.h>

namespace
{
	uint32 playerPositionLabel;

	bool engineUpdate()
	{
		entityManager *ents = gui()->entities();

		{ // player position
			CAGE_COMPONENT_GUI(text, t, ents->get(playerPositionLabel));
			t.value = playerPosition;
		}

		return false;
	}

	bool engineInitialize()
	{
		entityManager *ents = gui()->entities();

		entity *panel = nullptr;
		{ // panel
			entity *wrapper = ents->createUnique();
			CAGE_COMPONENT_GUI(scrollbars, sc, wrapper);
			panel = ents->createUnique();
			CAGE_COMPONENT_GUI(parent, parent, panel);
			parent.parent = wrapper->name();
			CAGE_COMPONENT_GUI(panel, g, panel);
			CAGE_COMPONENT_GUI(layoutTable, lt, panel);
		}

		{ // player position
			{ // label
				entity *e = ents->createUnique();
				CAGE_COMPONENT_GUI(parent, p, e);
				p.parent = panel->name();
				p.order = 1;
				CAGE_COMPONENT_GUI(label, l, e);
				CAGE_COMPONENT_GUI(text, t, e);
				t.value = "Position: ";
			}
			{ // value
				entity *e = ents->createUnique();
				CAGE_COMPONENT_GUI(parent, p, e);
				p.parent = panel->name();
				p.order = 2;
				CAGE_COMPONENT_GUI(label, l, e);
				CAGE_COMPONENT_GUI(text, t, e);
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
