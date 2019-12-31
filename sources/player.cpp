#include "common.h"

#include <cage-core/geometry.h>
#include <cage-core/entities.h>
#include <cage-core/collisionMesh.h>
#include <cage-core/hashString.h>
#include <cage-core/color.h>
#include <cage-core/spatial.h>
#include <cage-core/variableSmoothingBuffer.h>

#include <cage-engine/core.h>
#include <cage-engine/graphics.h>
#include <cage-engine/engine.h>
#include <cage-engine/window.h>

vec3 playerPosition;

namespace
{
	bool keyboardKeys[6]; // wsadeq
	vec3 mouseMoved; // x, y, wheel

	VariableSmoothingBuffer<quat, 3> cameraSmoothing;
	vec3 playerSpeed;

	void engineUpdate()
	{
		OPTICK_EVENT("player");

		uint64 time = currentControlTime();
		CAGE_COMPONENT_ENGINE(Transform, ct, entities()->get(1));
		CAGE_COMPONENT_ENGINE(Transform, pt, entities()->get(10));

		{ // rotate camera
			quat q = quat(degs(-mouseMoved[1]), degs(-mouseMoved[0]), degs(mouseMoved[2] * 20));
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
			playerSpeed = playerSpeed * 0.93 + a * 0.01;
			pt.position += pt.orientation * playerSpeed;
		}

		{ // update camera position
			CAGE_COMPONENT_ENGINE(Transform, ct, entities()->get(1));
			ct.position = pt.position + ct.orientation * vec3(0, 0.05, 0.2);
		}

		playerPosition = pt.position;
	}

	void setKeyboardKey(uint32 a, uint32 b, bool v)
	{
		switch (a)
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

	bool keyPress(uint32 a, uint32 b, ModifiersFlags m)
	{
		setKeyboardKey(a, b, true);
		return false;
	}

	bool keyRelease(uint32 a, uint32 b, ModifiersFlags m)
	{
		setKeyboardKey(a, b, false);
		return false;
	}

	ivec2 centerMouse()
	{
		ivec2 pt2 = window()->resolution();
		pt2.x /= 2;
		pt2.y /= 2;
		window()->mousePosition(pt2);
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
			mouseMoved[0] += p.x - c.x;
			mouseMoved[1] += p.y - c.y;
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
		windowListeners.attachAll(window());
		windowListeners.keyPress.bind<&keyPress>();
		windowListeners.keyRelease.bind<&keyRelease>();
		windowListeners.mousePress.bind<&mousePress>();
		windowListeners.mouseMove.bind<&mouseMove>();
		windowListeners.mouseWheel.bind<&mouseWheel>();

		{ // the spaceship
			Entity *e = entities()->create(10);
			CAGE_COMPONENT_ENGINE(Render, r, e);
			r.object = HashString("flittermouse/player/ship.object");
		}

		{ // camera
			Entity *e = entities()->create(1);
			CAGE_COMPONENT_ENGINE(Transform, t, e);
			CAGE_COMPONENT_ENGINE(Camera, c, e);
			c.near = 0.05;
			c.far = 100;
			c.ambientLight = vec3(0.01);
			c.effects = CameraEffectsFlags::CombinedPass;
			CAGE_COMPONENT_ENGINE(Light, l, e);
			l.lightType = LightTypeEnum::Directional;
			l.color = vec3(1);
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
