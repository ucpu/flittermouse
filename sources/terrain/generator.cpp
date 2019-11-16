#include "terrain.h"

#include <cage-core/geometry.h>
#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>
#include <cage-core/image.h>
#include <cage-core/enumerate.h>
#include <cage-engine/core.h>

#include "dualmc.h"
#include "xatlas.h"

#include <cstdarg>
#include <algorithm>

namespace
{
	const uint32 globalSeed = (uint32)currentRandomGenerator().next();

	const uint32 quadsPerTile = 16;

	inline void destroyAtlas(void *ptr)
	{
		xatlas::Destroy((xatlas::Atlas*)ptr);
	}

	inline holder<xatlas::Atlas> newAtlas()
	{
		xatlas::Atlas *a = xatlas::Create();
		return holder<xatlas::Atlas>(a, a, delegate<void(void*)>().bind<&destroyAtlas>());
	}

	inline holder<noiseFunction> newClouds(uint32 seed, uint32 octaves)
	{
		noiseFunctionCreateConfig cfg;
		cfg.octaves = octaves;
		cfg.type = noiseTypeEnum::Value;
		cfg.seed = seed;
		return newNoiseFunction(cfg);
	}

	template <class T>
	inline T rescale(const T &v, real ia, real ib, real oa, real ob)
	{
		return (v - ia) / (ib - ia) * (ob - oa) + oa;
	}

	inline vec3 pdnToRgb(real h, real s, real v)
	{
		return colorHsvToRgb(vec3(h / 360, s / 100, v / 100));
	}

	inline real sharpEdge(real v)
	{
		return rescale(clamp(v, 0.45, 0.55), 0.45, 0.55, 0, 1);
	}

	inline vec2 barycoord(const triangle &t, const vec2 &p)
	{
		vec2 a = vec2(t[0]);
		vec2 b = vec2(t[1]);
		vec2 c = vec2(t[2]);
		vec2 v0 = b - a;
		vec2 v1 = c - a;
		vec2 v2 = p - a;
		real d00 = dot(v0, v0);
		real d01 = dot(v0, v1);
		real d11 = dot(v1, v1);
		real d20 = dot(v2, v0);
		real d21 = dot(v2, v1);
		real invDenom = 1.0 / (d00 * d11 - d01 * d01);
		real v = (d11 * d20 - d01 * d21) * invDenom;
		real w = (d00 * d21 - d01 * d20) * invDenom;
		real u = 1 - v - w;
		return vec2(u, v);
	}

	inline vec3 interpolate(const triangle &t, const vec2 &p)
	{
		vec3 a = t[0];
		vec3 b = t[1];
		vec3 c = t[2];
		return p[0] * a + p[1] * b + (1 - p[0] - p[1]) * c;
	}

	inline bool inside(const vec2 &b)
	{
		return b[0] >= 0 && b[1] >= 0 && b[0] + b[1] <= 1;
	}

	inline ivec2 operator + (const ivec2 &a, const ivec2 &b)
	{
		return ivec2(a.x + b.x, a.y + b.y);
	}

	inline ivec2 operator - (const ivec2 &a, const ivec2 &b)
	{
		return ivec2(a.x - b.x, a.y - b.y);
	}

	inline ivec2 operator * (const ivec2 &a, float b)
	{
		return ivec2(sint32(a.x * b), sint32(a.y * b));
	}

	template<class T>
	inline void turnLeft(T &a, T &b, T &c)
	{
		std::swap(a, b); // bac
		std::swap(b, c); // bca
	}

	inline void get(holder<image> &img, uint32 x, uint32 y, real &result) { result = img->get1(x, y); }
	inline void get(holder<image> &img, uint32 x, uint32 y, vec2 &result) { result = img->get2(x, y); }
	inline void get(holder<image> &img, uint32 x, uint32 y, vec3 &result) { result = img->get3(x, y); }
	inline void get(holder<image> &img, uint32 x, uint32 y, vec4 &result) { result = img->get4(x, y); }

	holder<noiseFunction> densityNoise1 = newClouds(globalSeed + 1, 3);
	holder<noiseFunction> densityNoise2 = newClouds(globalSeed + 2, 3);
	holder<noiseFunction> colorNoise1 = newClouds(globalSeed + 3, 3);
	holder<noiseFunction> colorNoise2 = newClouds(globalSeed + 4, 2);
	holder<noiseFunction> colorNoise3 = newClouds(globalSeed + 5, 4);

	struct meshGenStruct
	{
		const tilePosStruct tilePos;
		transform tr;
		std::vector<real> densities;
		std::vector<dualmc::Vertex> mcVertices;
		std::vector<dualmc::Quad> mcIndices;
		std::vector<vertexStruct> &meshVertices;
		std::vector<uint32> &meshIndices;
		holder<xatlas::Atlas> atlas;

		// marching cubes space -> local (normalized) space
		vec3 m2l(const dualmc::Vertex &v) const
		{
			return (vec3(v.x, v.y, v.z) - 2) / (quadsPerTile - 5) * 2 - 1; // returns -1..+1
		}

		// local (normalized) space -> world space
		vec3 l2w(const vec3 &p) const
		{
			return tr.scale * p + tr.position;
		}

		meshGenStruct(const tilePosStruct &tilePos, std::vector<vertexStruct> &meshVertices, std::vector<uint32> &meshIndices) : tilePos(tilePos), meshVertices(meshVertices), meshIndices(meshIndices)
		{
			tr = tilePos.getTransform();
		}

		void genDensities()
		{
			OPTICK_EVENT("genDensities");
			densities.resize(quadsPerTile * quadsPerTile * quadsPerTile);
			std::vector<real> tmp;
			tmp.resize(densities.size());
			std::vector<vec3> positions;
			positions.reserve(densities.size());
			for (uint32 z = 0; z < quadsPerTile; z++)
			{
				for (uint32 y = 0; y < quadsPerTile; y++)
				{
					for (uint32 x = 0; x < quadsPerTile; x++)
					{
						dualmc::Vertex v = dualmc::Vertex(x, y, z);
						vec3 w = l2w(m2l(v));
						positions.push_back(w * 0.2);
					}
				}
			}
			densityNoise1->evaluate(numeric_cast<uint32>(densities.size()), positions.data(), densities.data());
			for (vec3 &p : positions)
				p = vec3(p[1], -p[2], p[0]) * (0.13 / 0.2);
			densityNoise2->evaluate(numeric_cast<uint32>(densities.size()), positions.data(), tmp.data());
			real *t = tmp.data();
			for (real &d : densities)
				d -= *t++;
		}

		void genSurface()
		{
			OPTICK_EVENT("genSurface");
			dualmc::DualMC<float> mc;
			mc.build((float*)densities.data(), quadsPerTile, quadsPerTile, quadsPerTile, 0, true, false, mcVertices, mcIndices);
			std::vector<real>().swap(densities);
		}

		void genTriangles()
		{
			OPTICK_EVENT("genTriangles");
			CAGE_ASSERT(meshVertices.empty());
			CAGE_ASSERT(meshIndices.empty());
			meshVertices.reserve(mcVertices.size());
			for (const auto &it : mcVertices)
			{
				vertexStruct v;
				v.position = m2l(it);
				meshVertices.push_back(v);
			}
			meshIndices.reserve(mcIndices.size() * 6 / 4);
			for (const auto &q : mcIndices)
			{
				const uint32 is[4] = { q.i0, q.i1, q.i2, q.i3 };
#define P(I) meshVertices[is[I]].position
				bool which = distanceSquared(P(0), P(2)) < distanceSquared(P(1), P(3)); // split the quad by shorter diagonal
#undef P
				static const int first[6] = { 0,1,2, 0,2,3 };
				static const int second[6] = { 1,2,3, 1,3,0 };
				for (uint32 i : (which ? first : second))
					meshIndices.push_back(is[i]);
				vec3 n;
				{
					vec3 a = meshVertices[is[0]].position;
					vec3 b = meshVertices[is[1]].position;
					vec3 c = meshVertices[is[2]].position;
					n = normalize(cross(b - a, c - a));
				}
				for (uint32 i : is)
					meshVertices[i].normal += n;
			}
			for (auto &it : meshVertices)
				it.normal = normalize(it.normal);
			std::vector<dualmc::Vertex>().swap(mcVertices);
			std::vector<dualmc::Quad>().swap(mcIndices);
		}

		uint32 clipAddPoint(uint32 ai, uint32 bi, uint32 axis, real value)
		{
			vec3 a = meshVertices[ai].position;
			vec3 b = meshVertices[bi].position;
			real pu = (value - a[axis]) / (b[axis] - a[axis]);
			CAGE_ASSERT(pu >= 0 && pu <= 1);
			vertexStruct v;
			v.position = interpolate(a, b, pu);
			v.normal = normalize(interpolate(meshVertices[ai].normal, meshVertices[bi].normal, pu));
			uint32 res = numeric_cast<uint32>(meshVertices.size());
			meshVertices.push_back(v);
			return res;
		}

		void clipTriangles(const std::vector<uint32> &in, std::vector<uint32> &out, uint32 axis, real value, bool side)
		{
			CAGE_ASSERT((in.size() % 3) == 0);
			CAGE_ASSERT(out.size() == 0);
			const uint32 tris = numeric_cast<uint32>(in.size() / 3);
			for (uint32 tri = 0; tri < tris; tri++)
			{
				uint32 ids[3] = { in[tri * 3 + 0], in[tri * 3 + 1], in[tri * 3 + 2]};
				vec3 a = meshVertices[ids[0]].position;
				vec3 b = meshVertices[ids[1]].position;
				vec3 c = meshVertices[ids[2]].position;
				bool as = a[axis] > value;
				bool bs = b[axis] > value;
				bool cs = c[axis] > value;
				uint32 m = as + bs + cs;
				if ((m == 0 && side) || (m == 3 && !side))
				{
					// all passes
					out.push_back(ids[0]);
					out.push_back(ids[1]);
					out.push_back(ids[2]);
					continue;
				}
				if ((m == 0 && !side) || (m == 3 && side))
				{
					// all rejected
					continue;
				}
				while (as || (as == bs))
				{
					turnLeft(ids[0], ids[1], ids[2]);
					turnLeft(a, b, c);
					turnLeft(as, bs, cs);
				}
				CAGE_ASSERT(!as && bs, as, bs, cs);
				uint32 pi = clipAddPoint(ids[0], ids[1], axis, value);
				if (m == 1)
				{
					CAGE_ASSERT(!cs);
					/*
					 *         |
					 * a +-------+ b
					 *    \    |/
					 *     \   /
					 *      \ /|
					 *     c + |
					 */
					uint32 qi = clipAddPoint(ids[1], ids[2], axis, value);
					if (side)
					{
						out.push_back(ids[0]);
						out.push_back(pi);
						out.push_back(qi);

						out.push_back(ids[0]);
						out.push_back(qi);
						out.push_back(ids[2]);
					}
					else
					{
						out.push_back(pi);
						out.push_back(ids[1]);
						out.push_back(qi);
					}
				}
				else if (m == 2)
				{
					CAGE_ASSERT(cs);
					/*
					 *     |
					 * a +-------+ b
					 *    \|    /
					 *     \   /
					 *     |\ /
					 *     | + c
					 */
					uint32 qi = clipAddPoint(ids[0], ids[2], axis, value);
					if (side)
					{
						out.push_back(ids[0]);
						out.push_back(pi);
						out.push_back(qi);
					}
					else
					{
						out.push_back(pi);
						out.push_back(ids[1]);
						out.push_back(qi);

						out.push_back(qi);
						out.push_back(ids[1]);
						out.push_back(ids[2]);
					}
				}
				else
				{
					CAGE_ASSERT(false, "invalid m");
				}
			}
		}

		void clipMesh()
		{
			OPTICK_EVENT("clipMesh");
			{
				const aabb clipBox = aabb(vec3(-1), vec3(1));
				const vec3 clipBoxArr[2] = { clipBox.a, clipBox.b };
				std::vector<uint32> sourceIndices;
				sourceIndices.reserve(meshIndices.size());
				sourceIndices.swap(meshIndices);
				std::vector<uint32> tmp, tmp2;
				const uint32 tris = numeric_cast<uint32>(sourceIndices.size() / 3);
				for (uint32 tri = 0; tri < tris; tri++)
				{
					const uint32 *ids = sourceIndices.data() + tri * 3;
					{
						const vec3 &a = meshVertices[ids[0]].position;
						const vec3 &b = meshVertices[ids[1]].position;
						const vec3 &c = meshVertices[ids[2]].position;
						if (intersects(a, clipBox) && intersects(b, clipBox) && intersects(c, clipBox))
						{
							// triangle fully inside the box
							meshIndices.push_back(ids[0]);
							meshIndices.push_back(ids[1]);
							meshIndices.push_back(ids[2]);
							continue;
						}
						if (!intersects(triangle(a, b, c), clipBox))
							continue; // triangle fully outside
					}
					tmp.push_back(ids[0]);
					tmp.push_back(ids[1]);
					tmp.push_back(ids[2]);
					for (uint32 axis = 0; axis < 3; axis++)
					{
						for (uint32 side = 0; side < 2; side++)
						{
							clipTriangles(tmp, tmp2, axis, clipBoxArr[side][axis], side);
							tmp.swap(tmp2);
							tmp2.clear();
						}
					}
					CAGE_ASSERT((tmp.size() % 3) == 0);
					for (uint32 i : tmp)
						meshIndices.push_back(i);
					tmp.clear();
				}
			}

			{ // filter vertices
				std::vector<bool> used;
				used.resize(meshVertices.size());
				for (uint32 i : meshIndices)
					used[i] = true;
				{
					uint32 i = 0;
					meshVertices.erase(std::remove_if(meshVertices.begin(), meshVertices.end(), [&](const auto &) {
						return !used[i++];
						}), meshVertices.end());
				}
				std::vector<uint32> indices;
				indices.reserve(meshIndices.size());
				uint32 c = 0;
				for (bool u : used)
				{
					indices.push_back(c);
					if (u)
						c++;
				}
				for (uint32 &it : meshIndices)
					it = indices[it];
			}
		}

		void genUvs()
		{
			OPTICK_EVENT("genUvs");
			atlas = newAtlas();

			{
				OPTICK_EVENT("AddMesh");
				xatlas::MeshDecl decl;
				decl.indexCount = numeric_cast<uint32>(meshIndices.size());
				decl.indexData = meshIndices.data();
				decl.indexFormat = xatlas::IndexFormat::UInt32;
				decl.vertexCount = numeric_cast<uint32>(meshVertices.size());
				decl.vertexPositionData = &meshVertices[0].position;
				decl.vertexNormalData = &meshVertices[0].normal;
				decl.vertexPositionStride = sizeof(vertexStruct);
				decl.vertexNormalStride = sizeof(vertexStruct);
				xatlas::AddMesh(atlas.get(), decl);
			}

			{
				{
					OPTICK_EVENT("ComputeCharts");
					xatlas::ChartOptions chart;
					xatlas::ComputeCharts(atlas.get(), chart);
				}
				{
					OPTICK_EVENT("ParameterizeCharts");
					xatlas::ParameterizeCharts(atlas.get());
				}
				{
					OPTICK_EVENT("PackCharts");
					xatlas::PackOptions pack;
					pack.texelsPerUnit = 16;
					pack.padding = 2;
					pack.bilinear = true;
					pack.blockAlign = true;
					xatlas::PackCharts(atlas.get(), pack);
				}
				CAGE_ASSERT(atlas->meshCount == 1);
				CAGE_ASSERT(atlas->atlasCount == 1);
			}

			{
				OPTICK_EVENT("apply");
				std::vector<vertexStruct> vs;
				xatlas::Mesh *m = atlas->meshes;
				vs.reserve(m->vertexCount);
				const vec2 whInv = 1 / vec2(atlas->width - 1, atlas->height - 1);
				for (uint32 i = 0; i < m->vertexCount; i++)
				{
					const xatlas::Vertex &a = m->vertexArray[i];
					vertexStruct v;
					v.position = meshVertices[a.xref].position;
					v.normal = meshVertices[a.xref].normal;
					v.uv = vec2(a.uv[0], a.uv[1]) * whInv;
					vs.push_back(v);
				}
				meshVertices.swap(vs);
				std::vector<uint32> is(m->indexArray, m->indexArray + m->indexCount);
				meshIndices.swap(is);
			}
		}

		void genTextures(holder<image> &albedo, holder<image> &special)
		{
			OPTICK_EVENT("genTextures");
			static const uint32 textureScale = 2;
			const uint32 w = atlas->width * textureScale;
			const uint32 h = atlas->height * textureScale;

			albedo = newImage();
			albedo->empty(w, h, 3);
			special = newImage();
			special->empty(w, h, 2);

			std::vector<triangle> triPos;
			std::vector<triangle> triNorms;
			std::vector<triangle> triUvs;
			{
				OPTICK_EVENT("prepTris");
				const xatlas::Mesh *m = atlas->meshes;
				const uint32 triCount = m->indexCount / 3;
				triPos.reserve(triCount);
				triNorms.reserve(triCount);
				triUvs.reserve(triCount);
				for (uint32 triIdx = 0; triIdx < triCount; triIdx++)
				{
					triangle p;
					triangle n;
					triangle u;
					const uint32 *ids = m->indexArray + triIdx * 3;
					for (uint32 i = 0; i < 3; i++)
					{
						const vertexStruct &v = meshVertices[ids[i]];
						p[i] = l2w(v.position);
						n[i] = v.normal;
						u[i] = vec3(v.uv, 0);
					}
					triPos.push_back(p);
					triNorms.push_back(n);
					triUvs.push_back(u);
				}
			}

			std::vector<vec3> positions;
			std::vector<vec3> normals;
			std::vector<uint32> xs;
			std::vector<uint32> ys;
			{
				OPTICK_EVENT("prepNoise");
				positions.reserve(w * h);
				normals.reserve(w * h);
				xs.reserve(w * h);
				ys.reserve(w * h);
				const vec2 whInv = 1 / vec2(w - 1, h - 1);

				const xatlas::Mesh *m = atlas->meshes;
				const uint32 triCount = m->indexCount / 3;
				for (uint32 triIdx = 0; triIdx < triCount; triIdx++)
				{
					const uint32 *vertIds = m->indexArray + triIdx * 3;
					const float *vertUvs[3] = {
						m->vertexArray[vertIds[0]].uv,
						m->vertexArray[vertIds[1]].uv,
						m->vertexArray[vertIds[2]].uv
					};
					ivec2 t0 = ivec2(sint32(vertUvs[0][0] * textureScale), sint32(vertUvs[0][1] * textureScale));
					ivec2 t1 = ivec2(sint32(vertUvs[1][0] * textureScale), sint32(vertUvs[1][1] * textureScale));
					ivec2 t2 = ivec2(sint32(vertUvs[2][0] * textureScale), sint32(vertUvs[2][1] * textureScale));
					// inspired by https://github.com/ssloy/tinyrenderer/wiki/Lesson-2:-Triangle-rasterization-and-back-face-culling
					if (t0.y > t1.y)
						std::swap(t0, t1);
					if (t0.y > t2.y)
						std::swap(t0, t2);
					if (t1.y > t2.y)
						std::swap(t1, t2);
					sint32 totalHeight = t2.y - t0.y;
					float totalHeightInv = 1.f / totalHeight;
					for (sint32 i = 0; i < totalHeight; i++)
					{
						bool secondHalf = i > t1.y - t0.y || t1.y == t0.y;
						uint32 segmentHeight = secondHalf ? t2.y - t1.y : t1.y - t0.y;
						float alpha = i * totalHeightInv;
						float beta = (float)(i - (secondHalf ? t1.y - t0.y : 0)) / segmentHeight;
						ivec2 A = t0 + (t2 - t0) * alpha;
						ivec2 B = secondHalf ? t1 + (t2 - t1) * beta : t0 + (t1 - t0) * beta;
						if (A.x > B.x)
							std::swap(A, B);
						for (sint32 x = A.x; x <= B.x; x++)
						{
							sint32 y = t0.y + i;
							vec2 uv = vec2(x, y) * whInv;
							vec2 b = barycoord(triUvs[triIdx], uv);
							positions.push_back(interpolate(triPos[triIdx], b));
							normals.push_back(normalize(interpolate(triNorms[triIdx], b)));
							xs.push_back(x);
							ys.push_back(y);
						}
					}
				}
			}

			{
				OPTICK_EVENT("pixelColors");
				static const vec3 colors[] = {
					pdnToRgb(240, 1, 45),
					pdnToRgb(230, 6, 35),
					pdnToRgb(240, 11, 28),
					pdnToRgb(232, 27, 21),
					pdnToRgb(31, 34, 96),
					pdnToRgb(31, 56, 93),
					pdnToRgb(26, 68, 80),
					pdnToRgb(21, 69, 55)
				};

				std::vector<vec3> pos(positions.begin(), positions.end());
				for (vec3 &p : pos)
					p *= 0.042;
				std::vector<real> c;
				c.resize(xs.size());
				colorNoise3->evaluate(numeric_cast<uint32>(pos.size()), pos.data(), c.data());
				std::vector<vec3> albedos;
				albedos.reserve(pos.size());
				for (real &u : c)
				{
					u = ((u * 0.5 + 0.5) * 16) % 8;
					uint32 i = numeric_cast<uint32>(u);
					real f = sharpEdge(u - i);
					albedos.push_back(interpolate(colors[i], colors[(i + 1) % 8], f));
				}

				std::vector<real> v1;
				v1.resize(pos.size());
				vec3 *position = positions.data();
				for (vec3 &p : pos)
					p = *position++ * 3;
				colorNoise1->evaluate(numeric_cast<uint32>(pos.size()), pos.data(), v1.data());
				std::vector<real> v2;
				v2.resize(pos.size());
				position = positions.data();
				for (vec3 &p : pos)
					p = *position++ * 4;
				colorNoise2->evaluate(numeric_cast<uint32>(pos.size()), pos.data(), v2.data());
				real *hi = v1.data();
				real *vi = v2.data();
				for (vec3 &albedo : albedos)
				{
					*hi = (*hi * +0.5 + 0.5) * 0.5 + 0.25;
					*vi = *vi * 0.5 + 0.5;
					vec3 hsv = colorRgbToHsv(albedo) + (vec3(*hi, 1 - *vi, *vi) - 0.5) * 0.1;
					hsv[0] = (hsv[0] + 1) % 1;
					albedo = colorHsvToRgb(clamp(hsv, vec3(0), vec3(1)));
					hi++;
					vi++;
				}

				uint32 *xi = xs.data();
				uint32 *yi = ys.data();
				for (vec3 &alb : albedos)
				{
					albedo->set(*xi, *yi, alb);
					special->set(*xi, *yi, vec2(0.8, 0.002));
					xi++;
					yi++;
				}
			}
		}

		template<class T>
		void inpaintProcess(holder<image> &src, holder<image> &dst)
		{
			uint32 w = src->width();
			uint32 h = src->height();
			for (uint32 y = 0; y < h; y++)
			{
				for (uint32 x = 0; x < w; x++)
				{
					T m;
					get(src, x, y, m);
					if (m == T())
					{
						uint32 cnt = 0;
						uint32 sy = numeric_cast<uint32>(clamp(sint32(y) - 1, 0, sint32(h) - 1));
						uint32 ey = numeric_cast<uint32>(clamp(sint32(y) + 1, 0, sint32(h) - 1));
						uint32 sx = numeric_cast<uint32>(clamp(sint32(x) - 1, 0, sint32(w) - 1));
						uint32 ex = numeric_cast<uint32>(clamp(sint32(x) + 1, 0, sint32(w) - 1));
						T a;
						for (uint32 yy = sy; yy <= ey; yy++)
						{
							for (uint32 xx = sx; xx <= ex; xx++)
							{
								get(src, xx, yy, a);
								if (a != T())
								{
									m += a;
									cnt++;
								}
							}
						}
						if (cnt > 0)
							dst->set(x, y, m / cnt);
					}
					else
						dst->set(x, y, m);
				}
			}
		}

		void inpaint(holder<image> &img)
		{
			if (!img)
				return;

			OPTICK_EVENT("inpaint");
			uint32 w = img->width();
			uint32 h = img->height();
			uint32 c = img->channels();
			holder<image> tmp = newImage();
			tmp->empty(w, h, c, img->bytesPerChannel());
			switch (c)
			{
			case 1: inpaintProcess<real>(img, tmp); break;
			case 2: inpaintProcess<vec2>(img, tmp); break;
			case 3: inpaintProcess<vec3>(img, tmp); break;
			case 4: inpaintProcess<vec4>(img, tmp); break;
			}
			std::swap(img, tmp);
		}
	};

	int xAtlasPrint(const char *format, ...)
	{
		char buffer[1000];
		va_list arg;
		va_start(arg, format);
		auto result = vsprintf(buffer, format, arg);
		va_end(arg);
		CAGE_LOG_DEBUG(severityEnum::Warning, "xatlas", buffer);
		return result;
	}

	class InitializerClass
	{
	public:
		InitializerClass()
		{
			xatlas::SetPrint(&xAtlasPrint, true);
		}
	} initializerInstance;
}

void terrainGenerate(const tilePosStruct &tilePos, std::vector<vertexStruct> &meshVertices, std::vector<uint32> &meshIndices, holder<image> &albedo, holder<image> &special)
{
	OPTICK_EVENT("terrainGenerate");
	meshGenStruct generator(tilePos, meshVertices, meshIndices);
	generator.genDensities();
	generator.genSurface();
	if (generator.mcIndices.size() == 0)
	{
		CAGE_ASSERT(meshVertices.empty());
		CAGE_ASSERT(meshIndices.empty());
		return;
	}
	generator.genTriangles();
	generator.clipMesh();
	if (generator.meshIndices.size() == 0)
		return;
	generator.genUvs();
	generator.genTextures(albedo, special);
	for (uint32 i = 0; i < 3; i++)
		generator.inpaint(albedo);
	for (uint32 i = 0; i < 3; i++)
		generator.inpaint(special);
	CAGE_LOG_DEBUG(severityEnum::Info, "generator", stringizer() + "generated mesh with " + meshVertices.size() + " vertices, " + meshIndices.size() + " indices and texture resolution: " + albedo->width() + "x" + albedo->height());
}
