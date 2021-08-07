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
			GuiTextComponent &t = ents->get(playerPositionLabel)->value<GuiTextComponent>();
			t.value = stringizer() + playerPosition;
		}

		{ // terrain generation progress
			GuiTextComponent &t = ents->get(terrainGenerationProgressLabel)->value<GuiTextComponent>();
			t.value = stringizer() + terrainGenerationProgress * 100 + " %";
		}
	}

	void engineInitialize()
	{
		EntityManager *ents = engineGui()->entities();

		Entity *panel = nullptr;
		{ // panel
			Entity *wrapper = ents->createUnique();
			GuiScrollbarsComponent &sc = wrapper->value<GuiScrollbarsComponent>();
			panel = ents->createUnique();
			GuiParentComponent &parent = panel->value<GuiParentComponent>();
			parent.parent = wrapper->name();
			GuiPanelComponent &g = panel->value<GuiPanelComponent>();
			GuiLayoutTableComponent &lt = panel->value<GuiLayoutTableComponent>();
		}

		{ // player position
			{ // label
				Entity *e = ents->createUnique();
				GuiParentComponent &p = e->value<GuiParentComponent>();
				p.parent = panel->name();
				p.order = 1;
				GuiLabelComponent &l = e->value<GuiLabelComponent>();
				GuiTextComponent &t = e->value<GuiTextComponent>();
				t.value = "Position: ";
			}
			{ // value
				Entity *e = ents->createUnique();
				GuiParentComponent &p = e->value<GuiParentComponent>();
				p.parent = panel->name();
				p.order = 2;
				GuiLabelComponent &l = e->value<GuiLabelComponent>();
				GuiTextComponent &t = e->value<GuiTextComponent>();
				playerPositionLabel = e->name();
			}
		}

		{ // terrain generation progress
			{ // label
				Entity *e = ents->createUnique();
				GuiParentComponent &p = e->value<GuiParentComponent>();
				p.parent = panel->name();
				p.order = 3;
				GuiLabelComponent &l = e->value<GuiLabelComponent>();
				GuiTextComponent &t = e->value<GuiTextComponent>();
				t.value = "Loading: ";
			}
			{ // value
				Entity *e = ents->createUnique();
				GuiParentComponent &p = e->value<GuiParentComponent>();
				p.parent = panel->name();
				p.order = 4;
				GuiLabelComponent &l = e->value<GuiLabelComponent>();
				GuiTextComponent &t = e->value<GuiTextComponent>();
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
