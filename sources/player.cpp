#include "common.h"

#include <cage-core/geometry.h>
#include <cage-core/entities.h>
#include <cage-core/collider.h>
#include <cage-core/hashString.h>
#include <cage-core/color.h>
#include <cage-core/spatialStructure.h>
#include <cage-core/variableSmoothingBuffer.h>

#include <cage-engine/graphics.h>
#include <cage-engine/engine.h>
#include <cage-engine/window.h>

vec3 playerPosition;
real terrainGenerationProgress;

namespace
{
	bool keyboardKeys[6]; // wsadeq
	vec3 mouseMoved; // x, y, wheel

	VariableSmoothingBuffer<quat, 3> cameraSmoothing;
	vec3 playerSpeed;

	void engineUpdate()
	{
		OPTICK_EVENT("player");

		uint64 time = engineControlTime();
		CAGE_COMPONENT_ENGINE(Transform, ct, engineEntities()->get(1));
		CAGE_COMPONENT_ENGINE(Transform, pt, engineEntities()->get(10));

		{ // rotate camera
			quat q = quat(degs(-mouseMoved[1] * 0.5), degs(-mouseMoved[0] * 0.5), degs(mouseMoved[2] * 15));
			mouseMoved = vec3();
			cameraSmoothing.add(q);
			ct.orientation = ct.orientation * cameraSmoothing.smooth();
		}

		{ // turn the ship
			pt.orientation = interpolate(pt.orientation, ct.orientation, 0.15);
		}

		{ // move the ship
			vec3 a = vec3((int)keyboardKeys[3] - (int)keyboardKeys[2], (int)keyboardKeys[4] - (int)keyboardKeys[5], (int)keyboardKeys[1] - (int)keyboardKeys[0]);
			if (a != vec3())
				a = normalize(a);
			a = pt.orientation * a;
			playerSpeed = playerSpeed * 0.93 + a * 0.006;
			pt.position += playerSpeed;
		}

		{ // update camera position
			CAGE_COMPONENT_ENGINE(Transform, ct, engineEntities()->get(1));
			ct.position = pt.position + ct.orientation * vec3(0, 0.05, 0.2);
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

	ivec2 centerMouse()
	{
		ivec2 pt2 = engineWindow()->resolution();
		pt2[0] /= 2;
		pt2[1] /= 2;
		engineWindow()->mousePosition(pt2);
		return pt2;
	}

	bool mousePress(MouseButtonsFlags b, ModifiersFlags m, const ivec2 &p)
	{
		if (b == MouseButtonsFlags::Left)
			centerMouse();
		return false;
	}

	bool mouseMove(MouseButtonsFlags b, ModifiersFlags m, const ivec2 &p)
	{
		if (b == MouseButtonsFlags::Left)
		{
			ivec2 c = centerMouse();
			mouseMoved[0] += p[0] - c[0];
			mouseMoved[1] += p[1] - c[1];
		}
		return false;
	}

	bool mouseWheel(sint32 wheel, ModifiersFlags m, const ivec2 &p)
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
			CAGE_COMPONENT_ENGINE(Render, r, e);
			r.object = HashString("flittermouse/player/ship.object");
		}

		{ // camera
			Entity *e = engineEntities()->create(1);
			CAGE_COMPONENT_ENGINE(Transform, t, e);
			CAGE_COMPONENT_ENGINE(Camera, c, e);
			c.near = 0.05;
			c.far = 200;
			c.ambientColor = c.ambientDirectionalColor = vec3(1);
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
