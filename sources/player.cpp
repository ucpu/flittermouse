
#include "common.h"

#include <cage-core/log.h>
#include <cage-core/geometry.h>
#include <cage-core/entities.h>
#include <cage-core/collider.h>
#include <cage-core/hashString.h>
#include <cage-core/color.h>
#include <cage-core/spatial.h>

#include <cage-client/core.h>
#include <cage-client/graphics.h>
#include <cage-client/engine.h>
#include <cage-client/window.h>

vec3 playerPosition;

namespace
{
	static const vec3 nozzlePositions[4 + 8 + 8 + 7] =
	{
		// forward [0]
		vec3(-18.75, -18.75, -80.23642) * 0.001,
		vec3(+18.75, -18.75, -80.23642) * 0.001,
		vec3(-18.75, +18.75, -80.23642) * 0.001,
		vec3(+18.75, +18.75, -80.23642) * 0.001,
		// front [4]
		vec3(-18.75, -21.25, -65.882) * 0.001,
		vec3(+18.75, -21.25, -65.882) * 0.001,
		vec3(-21.25, -18.75, -65.882) * 0.001,
		vec3(+21.25, -18.75, -65.882) * 0.001,
		vec3(-21.25, +18.75, -65.882) * 0.001,
		vec3(+21.25, +18.75, -65.882) * 0.001,
		vec3(-18.75, +21.25, -65.882) * 0.001,
		vec3(+18.75, +21.25, -65.882) * 0.001,
		// back [12]
		vec3(-18.75, -21.25, +65.882) * 0.001,
		vec3(+18.75, -21.25, +65.882) * 0.001,
		vec3(-21.25, -18.75, +65.882) * 0.001,
		vec3(+21.25, -18.75, +65.882) * 0.001,
		vec3(-21.25, +18.75, +65.882) * 0.001,
		vec3(+21.25, +18.75, +65.882) * 0.001,
		vec3(-18.75, +21.25, +65.882) * 0.001,
		vec3(+18.75, +21.25, +65.882) * 0.001,
		// backward [20]
		vec3(0, 0, 90.59878) * 0.001,
		vec3(-23.42824, 0, 90.59878) * 0.001,
		vec3(+23.42824, 0, 90.59878) * 0.001,
		vec3(-11.71412, -19.96571, 90.59878) * 0.001,
		vec3(+11.71412, -19.96571, 90.59878) * 0.001,
		vec3(-11.71412, +19.96571, 90.59878) * 0.001,
		vec3(+11.71412, +19.96571, 90.59878) * 0.001,
	};
	static const vec3 nozzleDirections[4 + 8 + 8 + 7] =
	{
		// forward [0]
		vec3(0, 0, -1),
		vec3(0, 0, -1),
		vec3(0, 0, -1),
		vec3(0, 0, -1),
		// front [4]
		vec3(0, -1, 0),
		vec3(0, -1, 0),
		vec3(-1, 0, 0),
		vec3(+1, 0, 0),
		vec3(-1, 0, 0),
		vec3(+1, 0, 0),
		vec3(0, +1, 0),
		vec3(0, +1, 0),
		// back [12]
		vec3(0, -1, 0),
		vec3(0, -1, 0),
		vec3(-1, 0, 0),
		vec3(+1, 0, 0),
		vec3(-1, 0, 0),
		vec3(+1, 0, 0),
		vec3(0, +1, 0),
		vec3(0, +1, 0),
		// backward [20]
		vec3(0, 0, 1),
		vec3(0, 0, 1),
		vec3(0, 0, 1),
		vec3(0, 0, 1),
		vec3(0, 0, 1),
		vec3(0, 0, 1),
		vec3(0, 0, 1),
	};
	static const quat nozzleOrientations[4 + 8 + 8 + 7] =
	{
		// forward [0]
		quat(degs(), degs(), degs()),
		quat(degs(), degs(), degs()),
		quat(degs(), degs(), degs()),
		quat(degs(), degs(), degs()),
		// front [4]
		quat(degs(-90), degs(), degs()),
		quat(degs(-90), degs(), degs()),
		quat(degs(), degs(+90), degs()),
		quat(degs(), degs(-90), degs()),
		quat(degs(), degs(+90), degs()),
		quat(degs(), degs(-90), degs()),
		quat(degs(+90), degs(), degs()),
		quat(degs(+90), degs(), degs()),
		// back [12]
		quat(degs(-90), degs(), degs()),
		quat(degs(-90), degs(), degs()),
		quat(degs(), degs(+90), degs()),
		quat(degs(), degs(-90), degs()),
		quat(degs(), degs(+90), degs()),
		quat(degs(), degs(-90), degs()),
		quat(degs(+90), degs(), degs()),
		quat(degs(+90), degs(), degs()),
		// backward [20]
		quat(degs(180), degs(), degs()),
		quat(degs(180), degs(), degs()),
		quat(degs(180), degs(), degs()),
		quat(degs(180), degs(), degs()),
		quat(degs(180), degs(), degs()),
		quat(degs(180), degs(), degs()),
		quat(degs(180), degs(), degs()),
	};
	static const vec3 weaponBaseOffset[3] = {
		vec3(-25.77877, 0, -72.05989) * 0.001,
		vec3(0, -25.77877, -72.05989) * 0.001,
		vec3(+25.77877, 0, -72.05989) * 0.001,
	};
}

namespace
{
	bool mouseButtons[3]; // left, middle, right
	bool keysPressedArrows[6]; // wsadeq
	bool keysPressedNumpad[10];
	bool keysPressedWeapons[3];
	bool keysPressedSpace;

	real nozzles[4 + 8 + 8 + 7];
	quat targetCameraOrientationShip;
	quat targetCameraOrientationView;
	vec3 weaponDirectionCurrent[3]; // relative to ship
	vec3 weaponDirectionTargets[3]; // relative to ship
	vec3 playerInertiaMotion;

	windowEventListeners listeners;

	void nozzlesTurning(real pitch, real yaw, real roll)
	{
		if (yaw > 0)
		{ // yaw left
			real t = yaw * 0.5 * 0.25;
			nozzles[4 + 3] += t;
			nozzles[4 + 5] += t;
			nozzles[12 + 2] += t;
			nozzles[12 + 4] += t;
		}
		if (yaw < 0)
		{ // yaw right
			real t = -yaw * 0.5 * 0.25;
			nozzles[4 + 2] += t;
			nozzles[4 + 4] += t;
			nozzles[12 + 3] += t;
			nozzles[12 + 5] += t;
		}
		if (pitch < 0)
		{ // pitch down
			real t = -pitch * 0.5 * 0.25;
			nozzles[4 + 6] += t;
			nozzles[4 + 7] += t;
			nozzles[12 + 0] += t;
			nozzles[12 + 1] += t;
		}
		if (pitch > 0)
		{ // pitch up
			real t = pitch * 0.5 * 0.25;
			nozzles[4 + 0] += t;
			nozzles[4 + 1] += t;
			nozzles[12 + 6] += t;
			nozzles[12 + 7] += t;
		}
		if (roll > 0)
		{ // roll left
			real t = roll * 0.5 * 0.125;
			nozzles[4 + 1] += t;
			nozzles[4 + 2] += t;
			nozzles[4 + 5] += t;
			nozzles[4 + 6] += t;
			nozzles[12 + 1] += t;
			nozzles[12 + 2] += t;
			nozzles[12 + 5] += t;
			nozzles[12 + 6] += t;
		}
		if (roll < 0)
		{ // roll right
			real t = -roll * 0.5 * 0.125;
			nozzles[4 + 0] += t;
			nozzles[4 + 3] += t;
			nozzles[4 + 4] += t;
			nozzles[4 + 7] += t;
			nozzles[12 + 0] += t;
			nozzles[12 + 3] += t;
			nozzles[12 + 4] += t;
			nozzles[12 + 7] += t;
		}
	}

	void nozzlesMoving(real dx, real dy, real dz, real boost)
	{
		if (dz < 0)
			nozzles[20] += -dz * 1;
		if (dz > 0)
			for (int i = 0; i < 4; i++)
				nozzles[i] += dz * 0.25;
		if (dx < 0)
		{
			real t = -dx * 0.25;
			nozzles[4 + 3] += t;
			nozzles[4 + 5] += t;
			nozzles[12 + 3] += t;
			nozzles[12 + 5] += t;
		}
		if (dx > 0)
		{
			real t = dx * 0.25;
			nozzles[4 + 2] += t;
			nozzles[4 + 4] += t;
			nozzles[12 + 2] += t;
			nozzles[12 + 4] += t;
		}
		if (dy < 0)
		{
			real t = -dy * 0.25;
			nozzles[4 + 6] += t;
			nozzles[4 + 7] += t;
			nozzles[12 + 6] += t;
			nozzles[12 + 7] += t;
		}
		if (dy > 0)
		{
			real t = dy * 0.25;
			nozzles[4 + 0] += t;
			nozzles[4 + 1] += t;
			nozzles[12 + 0] += t;
			nozzles[12 + 1] += t;
		}
		if (boost > 0)
		{
			for (int i = 0; i < 6; i++)
				nozzles[21 + i] += boost;
		}
	}

	const pointStruct centerMouse()
	{
		pointStruct pt2 = window()->resolution();
		pt2.x /= 2;
		pt2.y /= 2;
		window()->mousePosition(pt2);
		return pt2;
	}

	void setButtons(mouseButtonsFlags b, bool v)
	{
		if ((b & mouseButtonsFlags::Left) == mouseButtonsFlags::Left)
			mouseButtons[0] = v;
		if ((b & mouseButtonsFlags::Middle) == mouseButtonsFlags::Middle)
			mouseButtons[1] = v;
		if ((b & mouseButtonsFlags::Right) == mouseButtonsFlags::Right)
			mouseButtons[2] = v;
	}

	bool mouseRelease(mouseButtonsFlags buttons, modifiersFlags, const pointStruct &)
	{
		centerMouse();
		setButtons(buttons, true);
		return false;
	}

	bool mousePress(mouseButtonsFlags buttons, modifiersFlags, const pointStruct &)
	{
		setButtons(buttons, false);
		return false;
	}

	bool mouseMove(mouseButtonsFlags buttons, modifiersFlags, const pointStruct &pt)
	{
		sint32 dx = 0;
		sint32 dy = 0;
		if (buttons != mouseButtonsFlags::None)
		{
			pointStruct pt2 = centerMouse();
			dx = pt2.x - pt.x;
			dy = pt2.y - pt.y;
		}
		if ((buttons & mouseButtonsFlags::Left) == mouseButtonsFlags::Left)
		{
			targetCameraOrientationShip = targetCameraOrientationShip * quat(degs(dy * 0.1), degs(dx * 0.1), degs());
			nozzlesTurning(dy * 0.1, dx * 0.1, 0);
		}
		else if ((buttons & mouseButtonsFlags::Right) == mouseButtonsFlags::Right)
			targetCameraOrientationView = targetCameraOrientationView * quat(degs(dy * 0.1), degs(dx * 0.1), degs());
		return false;
	}

	bool mouseWheel(sint8 wheel, modifiersFlags, const pointStruct &)
	{
		targetCameraOrientationShip = targetCameraOrientationShip * quat(degs(), degs(), degs(wheel * 5));
		nozzlesTurning(0, 0, wheel * 5);
		return false;
	}

	void setKey(uint32 k, bool v)
	{
		if (k >= 320 && k < 330)
			keysPressedNumpad[k - 320] = v;
		switch (k)
		{
		case 87: // w
		case 265: // up
			keysPressedArrows[0] = v;
			break;
		case 83: // s
		case 264: // down
			keysPressedArrows[1] = v;
			break;
		case 65: // a
		case 263: // left
			keysPressedArrows[2] = v;
			break;
		case 68: // d
		case 262: // right
			keysPressedArrows[3] = v;
			break;
		case 69: // e
			keysPressedArrows[4] = v;
			break;
		case 81: // q
			keysPressedArrows[5] = v;
			break;
		case ' ': // space
			keysPressedSpace = v;
			break;
		case 49: // 1
		case 50: // 2
		case 51: // 3
			keysPressedWeapons[k - 49] = v;
			break;
		}
	}

	bool keyPress(uint32 a, uint32 b, modifiersFlags m)
	{
		setKey(a, true);
		return false;
	}

	bool keyRelease(uint32 a, uint32 b, modifiersFlags m)
	{
		setKey(a, false);
		return false;
	}

	vec3 processKeys()
	{
		{ // numeric keys
			vec3 r;
			if (keysPressedNumpad[8]) // pitch up
				r[0] += 1;
			if (keysPressedNumpad[2]) // pitch down
				r[0] -= 1;
			if (keysPressedNumpad[4]) // yaw left
				r[1] += 1;
			if (keysPressedNumpad[6]) // yaw right
				r[1] -= 1;
			if (keysPressedNumpad[7]) // roll left
				r[2] += 1;
			if (keysPressedNumpad[9]) // roll right
				r[2] -= 1;
			if (r != vec3())
			{
				r = normalize(r);
				bool keysPressedWeaponsAny = false;
				for (uint32 i = 0; i < 3; i++)
					keysPressedWeaponsAny = keysPressedWeaponsAny || keysPressedWeapons[i];
				if (keysPressedWeaponsAny)
				{ // weapons
					quat q = quat(degs(r[0]), degs(r[1]), degs());
					for (uint32 i = 0; i < 3; i++)
					{
						if (keysPressedWeapons[i])
							weaponDirectionTargets[i] = q * weaponDirectionTargets[i];
					}
				}
				else
				{ // camera
					r *= 3;
					targetCameraOrientationShip = targetCameraOrientationShip * quat(degs(r[0]), degs(r[1]), degs(r[2]));
					nozzlesTurning(r[0], r[1], r[2]);
				}
			}
		}
		vec3 m;
		if (keysPressedArrows[0]) // w
			m[2] -= 1;
		if (keysPressedArrows[1]) // s
			m[2] += 1;
		if (keysPressedArrows[2]) // a
			m[0] -= 1;
		if (keysPressedArrows[3]) // d
			m[0] += 1;
		if (keysPressedArrows[4]) // e
			m[1] += 1;
		if (keysPressedArrows[5]) // q
			m[1] -= 1;
		if (m != vec3())
			m = normalize(m);
		nozzlesMoving(m[0], m[1], m[2], keysPressedSpace ? 1 : 0);
		if (keysPressedSpace)
			m[2] -= 6;
		return m;
	}

	void updateNozzleEntities()
	{
		entityManagerClass *ents = entities();
		entityClass *player = ents->get(1);
		ENGINE_GET_COMPONENT(transform, pt, player);
		for (uint32 i = 0; i < 27; i++)
		{
			entityClass *e = entities()->get(30 + i);
			ENGINE_GET_COMPONENT(transform, t, e);
			t.position = pt.position + pt.orientation * nozzlePositions[i];
			t.orientation = pt.orientation * nozzleOrientations[i];
			t.scale = sqrt(nozzles[i]);
		}
	}

	void updateWeaponDirection(uint32 index, quat &roll, quat &yaw, quat &pitch)
	{
		weaponDirectionCurrent[index] = weaponDirectionTargets[index]; // todo limit turning speed
		roll = quat(degs(), degs(), degs(index * 90 + 90));
		vec3 normDir = roll.conjugate() * weaponDirectionCurrent[index];
		rads normYaw = (-aTan2(normDir[0], normDir[2]) - degs(90) + degs(180)).normalize() - degs(180);
		rads normPitch = (-aCos(normDir[1]) + degs(90) + degs(180)).normalize() - degs(180);
		rads limit = interpolate(degs(-20), degs(20), abs(real(degs(normYaw))) / 180);
		normPitch = max(normPitch, limit);
		yaw = quat(degs(), normYaw, degs());
		pitch = quat(normPitch, degs(), degs());
	}

	real quatDiff(const quat &a, const quat &b)
	{
		quat q = b - a;
		vec4 v = *(vec4*)&q;
		return v.squaredLength();
	}

	void engineUpdate()
	{
		uint64 time = currentControlTime();
		entityClass *player = entities()->get(1);
		ENGINE_GET_COMPONENT(transform, t, player);

		// lower nozzles power
		for (uint32 i = 0; i < 27; i++)
			nozzles[i] = nozzles[i] * 0.7;

		{ // update player position and orientation
			vec3 m = processKeys();
			{ // turning
				rads turning = degs(2);
				if (quatDiff(rotate(t.orientation, targetCameraOrientationShip, turning * 1.5), targetCameraOrientationShip) < real::epsilon)
					t.orientation = slerp(t.orientation, targetCameraOrientationShip, 0.5);
				else
					t.orientation = rotate(t.orientation, targetCameraOrientationShip, turning);
			}
			playerInertiaMotion *= 0.985;
			playerInertiaMotion += t.orientation * m * 0.001;
			t.position += playerInertiaMotion;
			playerPosition = t.position;
			updateNozzleEntities();
		}

		{ // camera
			if (!mouseButtons[2])
				targetCameraOrientationView = interpolate(quat(), targetCameraOrientationView, 0.9);

			entityClass *camera = entities()->get(2);
			ENGINE_GET_COMPONENT(transform, c, camera);
			c.orientation = interpolate(c.orientation, targetCameraOrientationShip * targetCameraOrientationView, 0.3);
			c.position = interpolate(c.position, t.position + c.orientation * vec3(0, 0.13, 0.25), 0.7);

			{ // sync skybox camera
				entityClass *cam2 = entities()->get(3);
				ENGINE_GET_COMPONENT(transform, t2, cam2);
				t2.orientation = c.orientation;
			}

			// weapon targets
			if (mouseButtons[2])
			{
				for (uint32 i = 0; i < 3; i++)
				{
					if (keysPressedWeapons[i])
						weaponDirectionTargets[i] = targetCameraOrientationView * vec3(0, 0, -1);
				}
			}
		}

		{ // habitats
			{ // 1
				entityClass *e = entities()->get(7);
				ENGINE_GET_COMPONENT(transform, h, e);
				h.position = t.position + t.orientation * vec3(0, 0, -15.625 * 0.001);
				h.orientation = t.orientation * quat(degs(), degs(), degs(time * 1e-5));
			}
			{ // 2
				entityClass *e = entities()->get(8);
				ENGINE_GET_COMPONENT(transform, h, e);
				h.position = t.position + t.orientation * vec3(0, 0, 15.625 * 0.001);
				h.orientation = t.orientation * quat(degs(), degs(180), degs(time * -1e-5 + 33));
			}
		}

		{ // weapons
			for (uint32 i = 0; i < 3; i++)
			{
				quat roll, yaw, pitch;
				updateWeaponDirection(i, roll, yaw, pitch);

				entityClass *eb = entities()->get(70 + i * 5);
				ENGINE_GET_COMPONENT(transform, tb, eb);
				tb.position = t.position + t.orientation * (weaponBaseOffset[i] + roll * vec3(0, 0.011, 0));
				tb.orientation = t.orientation * roll * yaw;

				entityClass *ec = entities()->get(71 + i * 5);
				ENGINE_GET_COMPONENT(transform, tc, ec);
				tc.position = tb.position;
				tc.orientation = tb.orientation * pitch;

				entityClass *el = entities()->get(72 + i * 5);
				ENGINE_GET_COMPONENT(transform, tl, el);
				tl.position = tc.position;
				tl.orientation = tc.orientation;
				vec3 target = terrainIntersection(makeRay(tl.position, tl.position + tl.orientation * vec3(0, 0, -1)));
				tl.scale = target.valid() ? target.distance(tl.position) : 0;

				entityClass *es = entities()->get(73 + i * 5);
				ENGINE_GET_COMPONENT(transform, ts, es);
				ts = tl;
				ts.scale = 1;
			}
		}
	}

	void engineInitialize()
	{
		listeners.attachAll(window());
		listeners.mouseMove.bind<&mouseMove>();
		listeners.mousePress.bind<&mousePress>();
		listeners.mouseRelease.bind<&mouseRelease>();
		listeners.mouseWheel.bind<&mouseWheel>();
		listeners.keyPress.bind<&keyPress>();
		listeners.keyRelease.bind<&keyRelease>();

		{ // ship
			entityClass *ship = entities()->create(1);
			ENGINE_GET_COMPONENT(render, r, ship);
			r.object = hashString("flittermouse/player/ship/ship.object");
		}

		{ // player camera
			entityClass *cam = entities()->create(2);
			ENGINE_GET_COMPONENT(camera, c, cam);
			c.near = 0.005;
			c.far = 100;
			c.ambientLight = vec3(1, 1, 1) * 0.08;
			c.clear = cameraClearFlags::None;
		}

		{ // skybox camera
			entityClass *cam = entities()->create(3);
			ENGINE_GET_COMPONENT(camera, c, cam);
			c.ambientLight = vec3(1, 1, 1);
			c.renderMask = 10;
			c.cameraOrder = -1;
			c.near = 0.5;
			c.far = 2;
		}

		{ // skybox
			entityClass *sky = entities()->create(999);
			ENGINE_GET_COMPONENT(transform, ts, sky);
			ENGINE_GET_COMPONENT(render, r, sky);
			r.object = hashString("flittermouse/skybox/skybox.obj");
			r.renderMask = 10;
		}

		{ // ship habitable 1
			entityClass *e = entities()->create(7);
			ENGINE_GET_COMPONENT(render, r, e);
			r.object = hashString("flittermouse/player/ship/habitable.object");
		}

		{ // ship habitable 2
			entityClass *e = entities()->create(8);
			ENGINE_GET_COMPONENT(render, r, e);
			r.object = hashString("flittermouse/player/ship/habitable.object");
		}

		{ // ship nozzle flames
			for (uint32 i = 0; i < 27; i++)
			{
				entityClass *e = entities()->create(30 + i);
				ENGINE_GET_COMPONENT(render, r, e);
				r.object = hashString("flittermouse/player/ship/nozzleFlame.object");
				ENGINE_GET_COMPONENT(animatedTexture, at, e);
				at.offset = randomChance();
			}
		}

		// weapons
		for (uint32 i = 0; i < 3; i++)
		{
			static const vec3 colors[] = {
				vec3(1, 0.2, 0.2),
				vec3(1, 1, 1),
				vec3(0.2, 1, 0.2),
			};
			{ // base
				entityClass *e = entities()->create(70 + i * 5);
				ENGINE_GET_COMPONENT(render, r, e);
				r.object = hashString("flittermouse/player/weapons/machinegun_base.object");
			}
			{ // cannon
				entityClass *e = entities()->create(71 + i * 5);
				ENGINE_GET_COMPONENT(render, r, e);
				r.object = hashString("flittermouse/player/weapons/machinegun_cannon.object");
			}
			{ // laser
				entityClass *e = entities()->create(72 + i * 5);
				ENGINE_GET_COMPONENT(render, r, e);
				r.object = hashString("flittermouse/player/weapons/laser.object");
				r.color = colors[i];
			}
			{ // light
				entityClass *e = entities()->create(73 + i * 5);
				ENGINE_GET_COMPONENT(light, l, e);
				l.lightType = lightTypeEnum::Spot;
				l.attenuation = vec3(0.5, 0.03, 0.01);
				l.spotAngle = degs(30);
				l.spotExponent = 1.3;
				l.color = colors[i];
				ENGINE_GET_COMPONENT(shadowmap, s, e);
				s.resolution = 1024;
				s.worldRadius = vec3(0.05, 60, 0);
			}
		}
		weaponDirectionCurrent[0] = weaponDirectionTargets[0] = vec3(-0.6, 0, -1).normalize();
		weaponDirectionCurrent[1] = weaponDirectionTargets[1] = vec3(0, 0.01, -1).normalize();
		weaponDirectionCurrent[2] = weaponDirectionTargets[2] = vec3(+0.6, 0, -1).normalize();
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
