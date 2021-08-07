#include <cage-core/core.h>
#include <cage-core/logger.h>
#include <cage-core/math.h>
#include <cage-core/config.h>
#include <cage-core/assetManager.h>
#include <cage-core/hashString.h>

#include <cage-engine/core.h>
#include <cage-engine/window.h>
#include <cage-engine/engine.h>
#include <cage-engine/engineStatistics.h>
#include <cage-engine/fullscreenSwitcher.h>
#include <cage-engine/highPerformanceGpuHint.h>

#include <exception>

using namespace cage;

namespace
{
	bool windowClose()
	{
		engineStop();
		return true;
	}
}

int main(int argc, const char *args[])
{
	try
	{
		configSetBool("cage/config/autoSave", true);
		Holder<Logger> log1 = newLogger();
		log1->format.bind<logFormatConsole>();
		log1->output.bind<logOutputStdOut>();

		engineInitialize(EngineCreateConfig());
		controlThread().updatePeriod(1000000 / 30);
		engineAssets()->add(HashString("flittermouse/flittermouse.pack"));

		EventListener<bool()> windowCloseListener;
		windowCloseListener.bind<&windowClose>();
		engineWindow()->events.windowClose.attach(windowCloseListener);
		engineWindow()->title("flittermouse");

		{
			Holder<FullscreenSwitcher> fullscreen = newFullscreenSwitcher({});
			Holder<EngineStatistics> EngineStatistics = newEngineStatistics();
			EngineStatistics->statisticsScope = EngineStatisticsScopeEnum::None;

			engineStart();
		}

		engineAssets()->remove(HashString("flittermouse/flittermouse.pack"));
		engineFinalize();
		return 0;
	}
	catch (...)
	{
		detail::logCurrentCaughtException();
	}
	return 1;
}
