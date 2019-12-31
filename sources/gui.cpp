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
		EntityManager *ents = gui()->entities();

		{ // player position
			CAGE_COMPONENT_GUI(Text, t, ents->get(playerPositionLabel));
			t.value = stringizer() + playerPosition;
		}

		return false;
	}

	bool engineInitialize()
	{
		EntityManager *ents = gui()->entities();

		Entity *panel = nullptr;
		{ // panel
			Entity *wrapper = ents->createUnique();
			CAGE_COMPONENT_GUI(Scrollbars, sc, wrapper);
			panel = ents->createUnique();
			CAGE_COMPONENT_GUI(Parent, parent, panel);
			parent.parent = wrapper->name();
			CAGE_COMPONENT_GUI(Panel, g, panel);
			CAGE_COMPONENT_GUI(LayoutTable, lt, panel);
		}

		{ // player position
			{ // label
				Entity *e = ents->createUnique();
				CAGE_COMPONENT_GUI(Parent, p, e);
				p.parent = panel->name();
				p.order = 1;
				CAGE_COMPONENT_GUI(Label, l, e);
				CAGE_COMPONENT_GUI(Text, t, e);
				t.value = "Position: ";
			}
			{ // value
				Entity *e = ents->createUnique();
				CAGE_COMPONENT_GUI(Parent, p, e);
				p.parent = panel->name();
				p.order = 2;
				CAGE_COMPONENT_GUI(Label, l, e);
				CAGE_COMPONENT_GUI(Text, t, e);
				playerPositionLabel = e->name();
			}
		}

		return false;
	}

	class Callbacks
	{
		EventListener<bool()> engineInitListener;
		EventListener<bool()> engineUpdateListener;
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
