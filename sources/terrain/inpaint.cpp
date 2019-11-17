#include "terrain.h"

#include <cage-core/geometry.h>
#include <cage-core/color.h>
#include <cage-core/image.h>

#include <cstdarg>
#include <algorithm>

namespace
{
	void getN(uint8 *const v, real &result) { result = *v; }
	void getN(uint8 *const v, vec2 &result) { result = vec2(v[0], v[1]); }
	void getN(uint8 *const v, vec3 &result) { result = vec3(v[0], v[1], v[2]); }
	void getN(uint8 *const v, vec4 &result) { result = vec4(v[0], v[1], v[2], v[3]); }

	template<class T>
	struct inpainter
	{
		image *const img;
		uint8 *const imgData;
		const uint32 channels;
		const sint32 w;
		const sint32 h;

		T get(sint32 x, sint32 y)
		{
			T res;
			getN(imgData + (y * w + x) * channels, res);
			return res;
		}

		struct border
		{
			sint32 x, y;
			T color;
		};

		bool blank(sint32 x, sint32 y)
		{
			return get(x, y) == T();
		}

		bool in(sint32 x, sint32 y)
		{
			return x >= 0 && y >= 0 && x < w && y < h;
		}

		bool hasValidNeighbor(sint32 x, sint32 y)
		{
#define TEST(X, Y) || (in(x + X, y + Y) && !blank(x + X, y + Y))
			return false
			TEST(-1, -1)
			TEST(0, -1)
			TEST(1, -1)
			TEST(-1, 0)
			TEST(1, 0)
			TEST(-1, 1)
			TEST(0, 1)
			TEST(1, 1);
#undef TEST
		}

		T inpaintColor(sint32 x, sint32 y)
		{
			T t;
			uint32 cnt = 0;
#define TEST(X, Y) if (in(x + X, y + Y)) { T v = get(x + X, y + Y); if (v != T()) { t += v; cnt++; } }
			TEST(-1, -1)
			TEST(0, -1)
			TEST(1, -1)
			TEST(-1, 0)
			TEST(1, 0)
			TEST(-1, 1)
			TEST(0, 1)
			TEST(1, 1)
#undef TEST
			CAGE_ASSERT(cnt > 0, x, y);
			return t / cnt;
		}

		inpainter(image *img, uint32 radius) : img(img), imgData((uint8*)img->bufferData()), channels(img->channels()), w(img->width()), h(img->height())
		{
			std::vector<border> borders;
			borders.reserve(w * h / 10);

			{ // find borders
				for (sint32 y = 0; y < h; y++)
				{
					for (sint32 x = 0; x < w; x++)
					{
						if (blank(x, y) && hasValidNeighbor(x, y))
						{
							border b;
							b.x = x;
							b.y = y;
							borders.push_back(b);
						}
					}
				}
			}

			while (true)
			{
				{ // find colors for borders
					for (border &it : borders)
						it.color = inpaintColor(it.x, it.y);
				}

				{ // apply colors at borders
					for (const border &it : borders)
						img->set(it.x, it.y, it.color / 255);
				}

				if (--radius == 0)
					break;

				{ // expand borders
					std::vector<border> bs;
					bs.reserve(borders.size() * 2);
					for (const border &it : borders)
					{
#define TEST(X, Y) if (in(it.x + X, it.y + Y)) { T v = get(it.x + X, it.y + Y); if (v == T()) { border b; b.x = it.x + X; b.y = it.y + Y; bs.push_back(b); } }
						TEST(-1, -1)
						TEST(0, -1)
						TEST(1, -1)
						TEST(-1, 0)
						TEST(1, 0)
						TEST(-1, 1)
						TEST(0, 1)
						TEST(1, 1)
#undef TEST
					}
					bs.erase(std::unique(bs.begin(), bs.end(), [](const auto &a, const auto &b) { return a.x == b.x && a.y == b.y; }), bs.end());
					std::swap(bs, borders);
				}
			}
		}
	};
}

void inpaint(image *img, uint32 radius)
{
	if (!img)
		return;
	CAGE_ASSERT(img->bytesPerChannel() == 1);
	switch (img->channels())
	{
	case 1: inpainter<real>(img, radius); break;
	case 2: inpainter<vec2>(img, radius); break;
	case 3: inpainter<vec3>(img, radius); break;
	case 4: inpainter<vec4>(img, radius); break;
	}
}
