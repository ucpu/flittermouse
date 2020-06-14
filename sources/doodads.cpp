#include "common.h"

#include <cage-core/ini.h>
#include <cage-core/entities.h>
#include <cage-core/hashString.h>
#include <cage-engine/engine.h>

#include <cstring> // std::strlen

namespace
{

	constexpr const char *playerDoodadsPositionsIni = R"(
[cannon-bottom-back]
pos=3.4221814271262474e-10,-1.6008334159851074,-0.6838023662567139
rot=3.1415932178497314,-2.848595857351914e-14,-6.283185005187988

[cannon-bottom-front]
pos=-8.017686514705247e-10,1.5394352674484253,-0.5108082294464111
rot=3.1415934562683105,-2.848595857351914e-14,-9.42477798461914

[cannon-top-back]
pos=3.9110811811404744e-11,-1.7716792821884155,0.6530738472938538
rot=0.0,-0.0,-3.1415929794311523

[cannon-top-front]
pos=-3.32440408534751e-10,1.4147937297821045,0.8491280674934387
rot=0.0,0.0,0.0

[magnet.005]
pos=0.5396445989608765,1.5398247241973877,-0.3219475746154785
rot=-0.007565317675471306,2.5562453269958496,-0.01369260810315609

[magnet.006]
pos=0.5168066024780273,0.7993545532226562,-0.5981693267822266
rot=0.9823010563850403,2.0580179691314697,1.453059434890747

[magnet.007]
pos=0.52027428150177,-0.9763599038124084,-0.6573458909988403
rot=-0.8033921718597412,2.0514047145843506,-1.2221852540969849

[magnet.008]
pos=0.5094743967056274,-1.6541908979415894,-0.46314537525177
rot=-0.008029870688915253,2.1956429481506348,-0.01569186896085739

[magnet.009]
pos=0.5952869057655334,-4.598189353942871,0.22249627113342285
rot=2.0003061294555664,0.852513313293457,1.2314223051071167

[magnet.010]
pos=0.5952869057655334,-3.83650279045105,0.21225190162658691
rot=1.8802177906036377,0.4511089324951172,2.380744218826294

[magnet.011]
pos=0.6484470367431641,-4.2053914070129395,0.19666874408721924
rot=2.0101208686828613,0.6365816593170166,1.8432163000106812

[magnet.001]
pos=0.36667680740356445,-0.6259640455245972,0.8580915331840515
rot=-2.5464584827423096,2.1495521068573,-2.7193994522094727

[magnet.002]
pos=0.5269393920898438,0.1554029881954193,0.8651465177536011
rot=-3.103236675262451,2.864353656768799,-3.002293586730957

[magnet.003]
pos=0.3655846118927002,0.8823286294937134,0.8268902897834778
rot=-0.47081437706947327,1.2934224605560303,-0.160680890083313

[magnet.004]
pos=-0.5396450161933899,1.5398247241973877,-0.3219475746154785
rot=-0.007565317675471306,-2.556244134902954,0.013692613691091537

[magnet.012]
pos=-0.5168070197105408,0.7993545532226562,-0.5981693267822266
rot=0.9823010563850403,-2.0580224990844727,-1.4530599117279053

[magnet.013]
pos=-0.5202739834785461,-0.9763599038124084,-0.6573458909988403
rot=-0.8033921718597412,-2.0514075756073,1.2221859693527222

[magnet.014]
pos=-0.509473979473114,-1.6541908979415894,-0.46314537525177
rot=-0.008029870688915253,-2.1956417560577393,0.01569187082350254

[magnet.015]
pos=-0.595287024974823,-4.598189353942871,0.22249627113342285
rot=2.0003061294555664,-0.8525130748748779,-1.2314223051071167

[magnet.016]
pos=-0.595287024974823,-3.83650279045105,0.21225190162658691
rot=1.8802177906036377,-0.45110827684402466,-2.380751371383667

[magnet.017]
pos=-0.6484469771385193,-4.2053914070129395,0.19666874408721924
rot=2.0101208686828613,-0.6365809440612793,-1.8432247638702393

[magnet.018]
pos=-0.3666769862174988,-0.6259640455245972,0.8580915331840515
rot=-2.5464584827423096,-2.149547576904297,2.71939754486084

[magnet.019]
pos=-0.5269389748573303,0.1554029881954193,0.8651465177536011
rot=-3.103236675262451,-2.864346981048584,3.002297878265381

[magnet.020]
pos=-0.36558499932289124,0.8823286294937134,0.8268902897834778
rot=-0.47081437706947327,-1.2934216260910034,0.16068094968795776

[light.001]
pos=0.5731962323188782,1.9724761247634888,0.07613500207662582
rot=-1.5707963705062866,0.12163479626178741,-0.12163447588682175

[light.002]
pos=-0.5725168585777283,1.9721051454544067,0.07533806562423706
rot=-1.5707963705062866,0.12163479626178741,0.12163452059030533
)";

}

namespace
{
	struct DoodadComponent : public transform
	{
		static EntityComponent *component;
	};

	EntityComponent *DoodadComponent::component;

	void createGun(const vec3 &position, const quat &orientation)
	{
		{
			Entity *e = engineEntities()->createAnonymous();
			GAME_COMPONENT(Doodad, t, e);
			t.position = position;
			t.orientation = orientation;
			CAGE_COMPONENT_ENGINE(Render, r, e);
			r.object = HashString("flittermouse/player/tower.object");
		}
		{
			Entity *e = engineEntities()->createAnonymous();
			GAME_COMPONENT(Doodad, t, e);
			t.position = position;
			t.orientation = orientation;
			CAGE_COMPONENT_ENGINE(Render, r, e);
			r.object = HashString("flittermouse/player/muzzle.object");
		}
	}

	void createMagnet(const vec3 &position, const quat &orientation)
	{
		Entity *e = engineEntities()->createAnonymous();
		GAME_COMPONENT(Doodad, t, e);
		t.position = position;
		t.orientation = orientation;
		CAGE_COMPONENT_ENGINE(Render, r, e);
		r.object = HashString("flittermouse/player/magnet.object");
	}

	void createLight(const vec3 &position, const quat &orientation)
	{
		Entity *e = engineEntities()->createAnonymous();
		GAME_COMPONENT(Doodad, t, e);
		t.position = position;
		t.orientation = orientation * quat(degs(90), degs(), degs());
		CAGE_COMPONENT_ENGINE(Light, l, e);
		l.lightType = LightTypeEnum::Spot;
		l.color = vec3(1);
		l.intensity = 10;
		l.attenuation = vec3(0, 0, 0.05);
		CAGE_COMPONENT_ENGINE(Shadowmap, s, e);
		s.resolution = 2048;
		s.worldSize = vec3(0.1, 100, 0);
	}

	void engineUpdate()
	{
		OPTICK_EVENT("player doodads");
		if (!engineEntities()->has(10))
			return;
		CAGE_COMPONENT_ENGINE(Transform, p, engineEntities()->get(10));
		for (Entity *e : DoodadComponent::component->entities())
		{
			GAME_COMPONENT(Doodad, d, e);
			CAGE_COMPONENT_ENGINE(Transform, t, e);
			t = p * d;
		}
	}

	vec3 convPos(const vec3 &v)
	{
		return vec3(v[0], v[2], -v[1]) * 0.02;
	}

	quat convRot(const vec3 &v)
	{
		return quat(rads(), rads(v[2]), rads()) * quat(rads(), rads(), rads(-v[1])) * quat(rads(v[0]), rads(), rads());
	}

	void engineInitialize()
	{
		DoodadComponent::component = engineEntities()->defineComponent(DoodadComponent(), true);
		Holder<Ini> ini = newIni();
		ini->importBuffer({ playerDoodadsPositionsIni, playerDoodadsPositionsIni + std::strlen(playerDoodadsPositionsIni) });
		for (const string &s : ini->sections())
		{
			const vec3 p = convPos(vec3::parse(ini->getString(s, "pos")));
			const quat r = convRot(vec3::parse(ini->getString(s, "rot")));
			if (s.isPattern("", "magnet", ""))
				createMagnet(p, r);
			else if (s.isPattern("", "cannon", ""))
				createGun(p, r);
			else if (s.isPattern("", "light", ""))
				createLight(p, r);
			else
				CAGE_THROW_ERROR(Exception, "unknown player doodad");
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
			engineUpdateListener.attach(controlThread().update, 100);
			engineUpdateListener.bind<&engineUpdate>();
		}
	} callbacksInstance;
}
