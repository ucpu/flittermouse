#include "common.h"

#include <cage-core/entities.h>

#include <cage-engine/core.h>
#include <cage-engine/engine.h>
#include <cage-engine/gui.h>

namespace
{
	uint32 playerPositionLabel;
	uint32 terrainGenerationProgressLabel;

	void engineUpdate()
	{
		EntityManager *ents = engineGui()->entities();

		{ // player position
			CAGE_COMPONENT_GUI(Text, t, ents->get(playerPositionLabel));
			t.value = stringizer() + playerPosition;
		}

		{ // terrain generation progress
			CAGE_COMPONENT_GUI(Text, t, ents->get(terrainGenerationProgressLabel));
			t.value = stringizer() + terrainGenerationProgress * 100 + " %";
		}
	}

	void engineInitialize()
	{
		EntityManager *ents = engineGui()->entities();

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

		{ // terrain generation progress
			{ // label
				Entity *e = ents->createUnique();
				CAGE_COMPONENT_GUI(Parent, p, e);
				p.parent = panel->name();
				p.order = 3;
				CAGE_COMPONENT_GUI(Label, l, e);
				CAGE_COMPONENT_GUI(Text, t, e);
				t.value = "Loading: ";
			}
			{ // value
				Entity *e = ents->createUnique();
				CAGE_COMPONENT_GUI(Parent, p, e);
				p.parent = panel->name();
				p.order = 4;
				CAGE_COMPONENT_GUI(Label, l, e);
				CAGE_COMPONENT_GUI(Text, t, e);
				terrainGenerationProgressLabel = e->name();
			}
		}
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
