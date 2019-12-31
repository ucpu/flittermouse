#include "terrain.h"

#include <cage-core/geometry.h>
#include <cage-core/color.h>
#include <cage-core/image.h>
#include <cage-engine/core.h> // ivec2

#include <cstdarg>
#include <algorithm>

namespace
{
	void getN(uint8 *const v, real &result) { result = *v; }
	void getN(uint8 *const v, vec2 &result) { result = vec2(v[0], v[1]); }
	void getN(uint8 *const v, vec3 &result) { result = vec3(v[0], v[1], v[2]); }
	void getN(uint8 *const v, vec4 &result) { result = vec4(v[0], v[1], v[2], v[3]); }

	bool operator == (const ivec2 &a, const ivec2 &b)
	{
		return a.x == b.x && a.y == b.y;
	}

	bool operator < (const ivec2 &a, const ivec2 &b)
	{
		return a.y == b.y ? a.x < b.x : a.y < b.y;
	}

	struct Bits
	{
		std::vector<bool> data;
		uint32 w;

		Bits(uint32 w, uint32 h) : w(w)
		{
			data.resize(w * h, false);
		}

		void set(uint32 x, uint32 y)
		{
			data[w * y + x] = true;
		}

		bool get(uint32 x, uint32 y)
		{
			return data[w * y + x];
		}
	};

	template<class T>
	struct Inpainter
	{
		Image *const img;
		uint8 *const imgData;
		Bits bts;
		const uint32 channels;
		const sint32 w;
		const sint32 h;

		T get(sint32 x, sint32 y)
		{
			T res;
			getN(imgData + (y * w + x) * channels, res);
			return res;
		}

		bool in(sint32 x, sint32 y)
		{
			return x >= 0 && y >= 0 && x < w && y < h;
		}

		bool hasValidNeighbor(sint32 x, sint32 y)
		{
#define TEST(X, Y) || (in(x + X, y + Y) && bts.get(x + X, y + Y))
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
#define TEST(X, Y) if (in(x + X, y + Y)) { if (bts.get(x + X, y + Y)) { t += get(x + X, y + Y); cnt++; } }
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

		Inpainter(Image *img, uint32 radius) : img(img), imgData((uint8*)img->bufferData()), bts(img->width(), img->height()), channels(img->channels()), w(img->width()), h(img->height())
		{
			OPTICK_EVENT("imageInpaint");
			std::vector<ivec2> borders;
			borders.reserve(w * h / 10);

			{ // fill in the Bits
				OPTICK_EVENT("fill bits");
				for (sint32 y = 0; y < h; y++)
				{
					for (sint32 x = 0; x < w; x++)
					{
						if (get(x, y) != T())
							bts.set(x, y);
					}
				}
			}

			{ // find borders
				OPTICK_EVENT("find borders");
				for (sint32 y = 0; y < h; y++)
				{
					for (sint32 x = 0; x < w; x++)
					{
						if (!bts.get(x, y) && hasValidNeighbor(x, y))
						{
							ivec2 b;
							b.x = x;
							b.y = y;
							borders.push_back(b);
						}
					}
				}
			}

			uint32 inpaintTotal = 0;
			while (true)
			{
				OPTICK_EVENT("iteration");
				OPTICK_TAG("count", borders.size());
				inpaintTotal += numeric_cast<uint32>(borders.size());

				std::vector<T> colors;
				colors.reserve(borders.size());
				{ // find colors for borders
					for (ivec2 &it : borders)
						colors.push_back(inpaintColor(it.x, it.y));
				}

				{ // apply colors at borders
					auto c = colors.begin();
					for (const ivec2 &it : borders)
					{
						img->set(it.x, it.y, *(c++) / 255);
						bts.set(it.x, it.y);
					}
				}

				if (--radius == 0)
					break;

				{ // expand borders
					std::vector<ivec2> bs;
					bs.reserve(borders.size() * 2);
					Bits bts2 = bts;
					for (const ivec2 &it : borders)
					{
#define TEST(X, Y) if (in(it.x + X, it.y + Y)) { ivec2 b; b.x = it.x + X; b.y = it.y + Y; if (!bts2.get(b.x, b.y)) { bs.push_back(b); bts2.set(b.x, b.y); } }
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
					std::swap(bs, borders);
				}
			}

			OPTICK_TAG("count", inpaintTotal);
		}
	};
}

void imageInpaint(Image *img, uint32 radius)
{
	if (!img)
		return;
	CAGE_ASSERT(img->bytesPerChannel() == 1);
	switch (img->channels())
	{
	case 1: Inpainter<real>(img, radius); break;
	case 2: Inpainter<vec2>(img, radius); break;
	case 3: Inpainter<vec3>(img, radius); break;
	case 4: Inpainter<vec4>(img, radius); break;
	}
}
