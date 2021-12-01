#include "common.h"

#include <cage-core/ini.h>
#include <cage-core/entities.h>
#include <cage-core/geometry.h>
#include <cage-core/hashString.h>
#include <cage-core/string.h>
#include <cage-engine/scene.h>
#include <cage-simple/engine.h>

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
	struct MagnetComponent
	{
		Transform model;
		Vec3 target;
	};

	struct LightComponent
	{
		Transform model;
		Vec3 target;
	};

	struct GunMuzzleComponent
	{
		Transform model;
	};

	struct GunTowerComponent
	{
		Entity *muzzle = nullptr;
	};

	void createMagnet(const Vec3 &position, const Quat &orientation)
	{
		Entity *e = engineEntities()->createAnonymous();
		MagnetComponent &t = e->value<MagnetComponent>();
		t.model.position = position;
		t.model.orientation = orientation;
		t.target = t.model.position + t.model.orientation * Vec3(0, 0, -1);
		RenderComponent &r = e->value<RenderComponent>();
		r.object = HashString("flittermouse/player/magnet.object");
	}

	void createLight(const Vec3 &position, const Quat &orientation)
	{
		Entity *e = engineEntities()->createAnonymous();
		LightComponent &t = e->value<LightComponent>();
		t.model.position = position;
		t.model.orientation = orientation;
		t.target = t.model.position + t.model.orientation * Vec3(0, 0, -1);
		cage::LightComponent &l = e->value<cage::LightComponent>();
		l.lightType = LightTypeEnum::Spot;
		l.color = randomChance3() * 0.3 + 0.7;
		l.attenuation = Vec3(1.5, 0, 0.05);
		ShadowmapComponent &s = e->value<ShadowmapComponent>();
		s.resolution = 2048;
		s.worldSize = Vec3(0.1, 100, 0);
	}

	void createGun(const Vec3 &position, const Quat &orientation)
	{
		Entity *muzzle = nullptr;
		{
			Entity *e = muzzle = engineEntities()->createAnonymous();
			GunMuzzleComponent &m = e->value<GunMuzzleComponent>();
			m.model.position = position;
			m.model.orientation = orientation;
			RenderComponent &r = e->value<RenderComponent>();
			r.object = HashString("flittermouse/player/muzzle.object");
		}
		{
			Entity *e = engineEntities()->createAnonymous();
			GunTowerComponent &t = e->value<GunTowerComponent>();
			t.muzzle = muzzle;
			RenderComponent &r = e->value<RenderComponent>();
			r.object = HashString("flittermouse/player/tower.object");
		}
	}

	void aimAtClosestWallTarget(const Vec3 &origin, const Vec3 &initialDirection, Vec3 &target, const Rads maxDeviation, const uint32 maxAttempts, const Real maxReach)
	{
		const Real maxDeviDot = cos(maxDeviation);

		const auto &check = [&](const Vec3 &p) -> bool
		{
			Real c = dot(normalize(p - origin), initialDirection);
			return c > maxDeviDot;
		};

		const auto &reposition = [&](const Vec3 &p) -> Vec3
		{
			Vec3 t = origin + normalize(p - origin) * maxReach;
			Vec3 q = terrainIntersection(makeSegment(origin, t));
			return q.valid() ? q : t;
		};

		CAGE_ASSERT(check(target));
		target = reposition(target);

		{
			Vec3 p = origin + initialDirection;
			p = reposition(p);
			if (distanceSquared(origin, p) < distanceSquared(origin, target))
				target = p;
		}

		for (uint32 attempt = 0; attempt < maxAttempts; attempt++)
		{
			Vec3 p = target + randomDirection3() * 0.1;
			if (!check(p))
				continue;
			p = reposition(p);
			if (distanceSquared(origin, p) < distanceSquared(origin, target))
				target = p;
		}
		CAGE_ASSERT(target.valid() && check(target) && distance(origin, target) < maxReach + 1e-5);
	}

	void magnetDischargeImpl(const Vec3 &a, const Vec3 &b, const Vec3 &cam, const Vec3 &color, Real lightProb)
	{
		Real d = distance(a, b);
		Vec3 v = normalize(b - a);
		Vec3 c = (a + b) * 0.5;
		Vec3 up = normalize(cam - c);
#ifdef CAGE_DEBUG
		constexpr Real threshold = 0.15;
#else
		constexpr Real threshold = 0.03;
#endif // CAGE_DEBUG
		if (d > threshold)
		{
			Vec3 side = normalize(cross(v, up));
			c += side * (d * randomRange(-0.2, 0.2));
			magnetDischargeImpl(a, c, cam, color, lightProb * 0.5);
			magnetDischargeImpl(c, b, cam, color, lightProb * 0.5);
			return;
		}
		Entity *e = engineEntities()->createUnique();
		TransformComponent &t = e->value<TransformComponent>();
		t.position = c;
		t.orientation = Quat(v, up, true);
		t.scale = d;
		RenderComponent &r = e->value<RenderComponent>();
		r.object = HashString("flittermouse/lightning/lightning.obj");
		r.color = color;
		TextureAnimationComponent &anim = e->value<TextureAnimationComponent>();
		anim.offset = randomChance() * 100;
		TimeoutComponent &ttl = e->value<TimeoutComponent>();
		ttl.ttl = 1;
		if (randomChance() < lightProb)
		{
			cage::LightComponent &light = e->value<cage::LightComponent>();
			light.color = color;
			light.intensity = 1.5;
			light.lightType = LightTypeEnum::Point;
			light.attenuation = Vec3(0.5, 0, 0.4);
		}
	}

	void magnetDischarge(const Transform &tc, const Vec3 &pc)
	{
		if (randomChance() > 0.3 / (1 + sqr(distanceSquared(pc, tc.position))))
			return;
		Vec3 color = randomChance3() * 0.4 + Vec3(0, 0, 0.4);
		TransformComponent &cam = engineEntities()->get(1)->value<TransformComponent>();
		Vec3 start = tc.position + tc.orientation * Vec3(0, 0, -0.005);
		Vec3 end = pc + (randomChance3() - 0.5) * 0.01;
		magnetDischargeImpl(start, end, cam.position, color, 2);
	}

	void engineUpdate()
	{
		if (!engineEntities()->has(10))
			return;

		TransformComponent &p = engineEntities()->get(10)->value<TransformComponent>();
		TransformComponent &cameraTransform = engineEntities()->get(1)->value<TransformComponent>();
		CameraComponent &cameraProperties = engineEntities()->get(1)->value<CameraComponent>();

		for (Entity *e : engineEntities()->component<MagnetComponent>()->entities())
		{
			MagnetComponent &m = e->value<MagnetComponent>();
			TransformComponent &t = e->value<TransformComponent>();
			t = p * m.model;
			aimAtClosestWallTarget(t.position, t.orientation * Vec3(0, 0, -1), m.target, Degs(40), 1, 3);
			t.orientation = Quat(normalize(m.target - t.position), t.orientation * Vec3(0, 1, 0));
			magnetDischarge(t, m.target);
		}

		for (Entity *e : engineEntities()->component<LightComponent>()->entities())
		{
			LightComponent &l = e->value<LightComponent>();
			TransformComponent &t = e->value<TransformComponent>();
			t = p * l.model;
			aimAtClosestWallTarget(t.position, t.orientation * Vec3(0, 0, -1), l.target, Degs(15), 5, 12);
			t.orientation = Quat(normalize(l.target - t.position), t.orientation * Vec3(0, 1, 0));
			cage::LightComponent &ll = e->value<cage::LightComponent>();
			ll.intensity = interpolate(ll.intensity, sqr(distance(l.target, t.position) + 1), 0.02);
			const Real focus = distance(cameraTransform.position, l.target);
			cameraProperties.depthOfField.focusDistance = interpolate(cameraProperties.depthOfField.focusDistance, focus, 0.05);
		}
		cameraProperties.depthOfField.focusRadius = 1;
		cameraProperties.depthOfField.blendRadius = cameraProperties.depthOfField.focusDistance * 1.2;

		for (Entity *e : engineEntities()->component<GunMuzzleComponent>()->entities())
		{
			GunMuzzleComponent &gm = e->value<GunMuzzleComponent>();
			TransformComponent &t = e->value<TransformComponent>();
			t = p * gm.model;
		}

		for (Entity *e : engineEntities()->component<GunTowerComponent>()->entities())
		{
			GunTowerComponent &gt = e->value<GunTowerComponent>();
			TransformComponent &gm = gt.muzzle->value<TransformComponent>();
			TransformComponent &t = e->value<TransformComponent>();
			t = gm;
			t.orientation = Quat(t.orientation * Vec3(0, 0, -1), t.orientation * Vec3(0, 1, 0), true);
		}
	}

	Vec3 convPos(const Vec3 &v)
	{
		return Vec3(v[0], v[2], -v[1]) * 0.02;
	}

	Quat convRot(const Vec3 &v)
	{
		return Quat(Rads(), Rads(v[2]), Rads()) * Quat(Rads(), Rads(), Rads(-v[1])) * Quat(Rads(v[0]), Rads(), Rads()) * Quat(Degs(90), Degs(), Degs());
	}

	void engineInitialize()
	{
		engineEntities()->defineComponent(MagnetComponent());
		engineEntities()->defineComponent(LightComponent());
		engineEntities()->defineComponent(GunMuzzleComponent());
		engineEntities()->defineComponent(GunTowerComponent());
		Holder<Ini> ini = newIni();
		ini->importBuffer({ playerDoodadsPositionsIni, playerDoodadsPositionsIni + std::strlen(playerDoodadsPositionsIni) });
		for (const String &s : ini->sections())
		{
			const Vec3 p = convPos(Vec3::parse(ini->getString(s, "pos")));
			const Quat r = convRot(Vec3::parse(ini->getString(s, "rot")));
			if (isPattern(s, "", "magnet", ""))
				createMagnet(p, r);
			else if (isPattern(s, "", "light", ""))
				createLight(p, r);
			else if (isPattern(s, "", "cannon", ""))
				createGun(p, r);
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
