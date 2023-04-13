#include "common.h"

#include <cage-engine/guiBuilder.h>
#include <cage-simple/engine.h>

namespace
{
	void engineUpdate()
	{
		EntityManager *ents = engineGuiEntities();
		ents->get(1)->value<GuiTextComponent>().value = Stringizer() + playerPosition;
		ents->get(2)->value<GuiTextComponent>().value = Stringizer() + terrainGenerationProgress * 100 + " %";
	}

	void engineInitialize()
	{
		Holder<GuiBuilder> g = newGuiBuilder(engineGuiEntities());
		auto _1 = g->alignment(Vec2(0));
		auto _2 = g->panel();
		auto _3 = g->verticalTable(2);
		g->label().text("Position: ");
		g->setNextName(1).label().text("");
		g->label().text("Loading: ");
		g->setNextName(2).label().text("");
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
