#include "common.h"

#include <cage-core/geometry.h>
#include <cage-core/entities.h>
#include <cage-core/collider.h>
#include <cage-core/hashString.h>
#include <cage-core/color.h>
#include <cage-core/spatialStructure.h>
#include <cage-core/variableSmoothingBuffer.h>

#include <cage-engine/engine.h>
#include <cage-engine/window.h>

Vec3 playerPosition;
Real terrainGenerationProgress;

namespace
{
	bool keyboardKeys[6]; // wsadeq
	Vec3 mouseMoved; 

	VariableSmoothingBuffer<Quat, 3> cameraSmoothing;
	Vec3 playerSpeed;

	void engineUpdate()
	{
		uint64 time = engineControlTime();
		TransformComponent &ct = engineEntities()->get(1)->value<TransformComponent>();
		TransformComponent &pt = engineEntities()->get(10)->value<TransformComponent>();

		{ // rotate camera
			Quat q = Quat(Degs(-mouseMoved[1] * 0.5), Degs(-mouseMoved[0] * 0.5), Degs(mouseMoved[2] * 15));
			mouseMoved = Vec3();
			cameraSmoothing.add(q);
			ct.orientation = ct.orientation * cameraSmoothing.smooth();
		}

		{ // turn the ship
			pt.orientation = interpolate(pt.orientation, ct.orientation, 0.15);
		}

		{ // move the ship
			Vec3 a = Vec3((int)keyboardKeys[3] - (int)keyboardKeys[2], (int)keyboardKeys[4] - (int)keyboardKeys[5], (int)keyboardKeys[1] - (int)keyboardKeys[0]);
			if (a != Vec3())
				a = normalize(a);
			a = pt.orientation * a;
			playerSpeed = playerSpeed * 0.93 + a * 0.006;
			pt.position += playerSpeed;
		}

		{ // update camera position
			TransformComponent &ct = engineEntities()->get(1)->value<TransformComponent>();
			ct.position = pt.position + ct.orientation * Vec3(0, 0.05, 0.2);
		}

		playerPosition = pt.position;
	}

	void setKeyboardKey(uint32 key, bool v)
	{
		switch (key)
		{
		case 87: // w
		case 265: // up
			keyboardKeys[0] = v;
			break;
		case 83: // s
		case 264: // down
			keyboardKeys[1] = v;
			break;
		case 65: // a
		case 263: // left
			keyboardKeys[2] = v;
			break;
		case 68: // d
		case 262: // right
			keyboardKeys[3] = v;
			break;
		case 69: // e
			keyboardKeys[4] = v;
			break;
		case 81: // q
			keyboardKeys[5] = v;
			break;
		}
	}

	bool keyPress(uint32 key, ModifiersFlags m)
	{
		setKeyboardKey(key, true);
		return false;
	}

	bool keyRelease(uint32 key, ModifiersFlags m)
	{
		setKeyboardKey(key, false);
		return false;
	}

	Vec2i centerMouse()
	{
		Vec2i pt2 = engineWindow()->resolution();
		pt2[0] /= 2;
		pt2[1] /= 2;
		engineWindow()->mousePosition(pt2);
		return pt2;
	}

	bool mousePress(MouseButtonsFlags b, ModifiersFlags m, const Vec2i &p)
	{
		if (b == MouseButtonsFlags::Left)
			centerMouse();
		return false;
	}

	bool mouseMove(MouseButtonsFlags b, ModifiersFlags m, const Vec2i &p)
	{
		if (b == MouseButtonsFlags::Left)
		{
			Vec2i c = centerMouse();
			mouseMoved[0] += p[0] - c[0];
			mouseMoved[1] += p[1] - c[1];
		}
		return false;
	}

	bool mouseWheel(sint32 wheel, ModifiersFlags m, const Vec2i &p)
	{
		mouseMoved[2] += wheel;
		return false;
	}

	WindowEventListeners windowListeners;

	void engineInitialize()
	{
		windowListeners.attachAll(engineWindow());
		windowListeners.keyPress.bind<&keyPress>();
		windowListeners.keyRelease.bind<&keyRelease>();
		windowListeners.mousePress.bind<&mousePress>();
		windowListeners.mouseMove.bind<&mouseMove>();
		windowListeners.mouseWheel.bind<&mouseWheel>();

		{ // the spaceship
			Entity *e = engineEntities()->create(10);
			RenderComponent &r = e->value<RenderComponent>();
			r.object = HashString("flittermouse/player/ship.object");
		}

		{ // camera
			Entity *e = engineEntities()->create(1);
			TransformComponent &t = e->value<TransformComponent>();
			CameraComponent &c = e->value<CameraComponent>();
			c.near = 0.05;
			c.far = 200;
			c.ambientColor = c.ambientDirectionalColor = Vec3(1);
			c.ambientIntensity = 0.02;
			c.ambientDirectionalIntensity = 0.15;
			c.effects = CameraEffectsFlags::Default | CameraEffectsFlags::DepthOfField;
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
