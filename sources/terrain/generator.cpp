#include "terrain.h"

#include <cage-core/geometry.h>
#include <cage-core/image.h>
#include <cage-core/enumerate.h>
#include <cage-core/noiseFunction.h>
#include <cage-engine/core.h> // ivec2

#include "dualmc.h"

#ifdef UV_MODE_XATLAS
#include "xatlas.h"
#endif

#ifdef UV_MODE_TRIANGLEPACKER
#include "trianglepacker.h"
#endif

#include <cstdarg>
#include <algorithm>

namespace
{
	const uint32 quadsPerTile = 16;

#ifdef UV_MODE_XATLAS
	inline void destroyAtlas(void *ptr)
	{
		xatlas::Destroy((xatlas::Atlas*)ptr);
	}

	inline holder<xatlas::Atlas> newAtlas()
	{
		xatlas::Atlas *a = xatlas::Create();
		return holder<xatlas::Atlas>(a, a, delegate<void(void*)>().bind<&destroyAtlas>());
	}
#endif

	vec2 barycoord(const triangle &t, const vec2 &p)
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

	vec3 interpolate(const triangle &t, const vec2 &p)
	{
		vec3 a = t[0];
		vec3 b = t[1];
		vec3 c = t[2];
		return p[0] * a + p[1] * b + (1 - p[0] - p[1]) * c;
	}

	bool inside(const vec2 &b)
	{
		return b[0] >= 0 && b[1] >= 0 && b[0] + b[1] <= 1;
	}

	ivec2 operator + (const ivec2 &a, const ivec2 &b)
	{
		return ivec2(a.x + b.x, a.y + b.y);
	}

	ivec2 operator - (const ivec2 &a, const ivec2 &b)
	{
		return ivec2(a.x - b.x, a.y - b.y);
	}

	ivec2 operator * (const ivec2 &a, float b)
	{
		return ivec2(sint32(a.x * b), sint32(a.y * b));
	}

	template<class T>
	void turnLeft(T &a, T &b, T &c)
	{
		std::swap(a, b); // bac
		std::swap(b, c); // bca
	}

	holder<noiseFunction> densityNoise1 = newClouds(globalSeed() + 1, 3);
	holder<noiseFunction> densityNoise2 = newClouds(globalSeed() + 2, 3);

	struct meshGenStruct
	{
		const tilePosStruct tilePos;
		transform tr;
		std::vector<real> densities;
		std::vector<dualmc::Vertex> mcVertices;
		std::vector<dualmc::Quad> mcIndices;
		std::vector<vertexStruct> &meshVertices;
		std::vector<uint32> &meshIndices;
#ifdef UV_MODE_XATLAS
		holder<xatlas::Atlas> atlas;
#endif
		uint32 texWidth, texHeight;

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

		meshGenStruct(const tilePosStruct &tilePos, std::vector<vertexStruct> &meshVertices, std::vector<uint32> &meshIndices) : tilePos(tilePos), meshVertices(meshVertices), meshIndices(meshIndices), texWidth(0), texHeight(0)
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
			OPTICK_TAG("count", densities.size());
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
				const uint32 is[4] = { numeric_cast<uint32>(q.i0), numeric_cast<uint32>(q.i1), numeric_cast<uint32>(q.i2), numeric_cast<uint32>(q.i3) };
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
			OPTICK_TAG("vertices", meshVertices.size());
			OPTICK_TAG("indices", meshIndices.size());
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
			OPTICK_TAG("vertices", meshVertices.size());
			OPTICK_TAG("indices", meshIndices.size());
		}

#ifdef UV_MODE_XATLAS
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

			texWidth = atlas->width * 2;
			texHeight = atlas->height * 2;

			OPTICK_TAG("vertices", meshVertices.size());
			OPTICK_TAG("indices", meshIndices.size());
		}
#endif

#ifdef UV_MODE_TRIANGLEPACKER
		void genUvs()
		{
			OPTICK_EVENT("genUvs");

			uint32 count = meshIndices.size();
			std::vector<vec3> pos;
			pos.reserve(count);
			for (uint32 i = 0; i < count; i += 3)
			{
				pos.push_back(meshVertices[meshIndices[i + 0]].position);
				pos.push_back(meshVertices[meshIndices[i + 1]].position);
				pos.push_back(meshVertices[meshIndices[i + 2]].position);
			}

			std::vector<vec2> uvs;
			uvs.resize(count);

			uint32 w = 512, h = 512;
			while (true)
			{
				OPTICK_EVENT("tpPackWithFixedScaleIntoRect");
				auto res = tpPackWithFixedScaleIntoRect((float*)pos.data(), count, 30, w, h, 0, 5, (float*)uvs.data());
				if (res == count)
					break;
				if (w > h)
					h *= 2;
				else
					w *= 2;
			}
			texWidth = w;
			texHeight = h;

			std::vector<vertexStruct> vs;
			vs.reserve(count);
			for (uint32 i = 0; i < count; i += 3)
			{
				for (uint32 j = 0; j < 3; j++)
				{
					vertexStruct v = meshVertices[meshIndices[i + j]];
					v.uv = uvs[i + j];
					vs.push_back(v);
				}
			}

			std::vector<uint32> is;
			is.reserve(count);
			for (uint32 i = 0; i < count; i++)
				is.push_back(i);

			meshVertices.swap(vs);
			meshIndices.swap(is);

			OPTICK_TAG("vertices", meshVertices.size());
			OPTICK_TAG("indices", meshIndices.size());
		}
#endif

		void genTextures(holder<image> &albedo, holder<image> &special)
		{
			OPTICK_EVENT("genTextures");
			OPTICK_TAG("width", texWidth);
			OPTICK_TAG("height", texHeight);
			const uint32 w = texWidth;
			const uint32 h = texHeight;

			albedo = newImage();
			albedo->empty(w, h, 3);
			special = newImage();
			special->empty(w, h, 2);

			std::vector<triangle> triPos;
			std::vector<triangle> triNorms;
			std::vector<triangle> triUvs;
			{
				OPTICK_EVENT("prepTris");
				const uint32 triCount = meshIndices.size() / 3;
				triPos.reserve(triCount);
				triNorms.reserve(triCount);
				triUvs.reserve(triCount);
				for (uint32 triIdx = 0; triIdx < triCount; triIdx++)
				{
					triangle p;
					triangle n;
					triangle u;
					const uint32 *ids = meshIndices.data() + triIdx * 3;
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
				OPTICK_EVENT("rasterization");
				positions.reserve(w * h);
				normals.reserve(w * h);
				xs.reserve(w * h);
				ys.reserve(w * h);
				const vec2 whInv = 1 / vec2(w - 1, h - 1);

				const uint32 triCount = meshIndices.size() / 3;
				for (uint32 triIdx = 0; triIdx < triCount; triIdx++)
				{
					const uint32 *vertIds = meshIndices.data() + triIdx * 3;
					const vec2 vertUvs[3] = {
						meshVertices[vertIds[0]].uv,
						meshVertices[vertIds[1]].uv,
						meshVertices[vertIds[2]].uv
					};
					ivec2 t0 = ivec2(numeric_cast<sint32>(vertUvs[0][0] * w), numeric_cast<sint32>(vertUvs[0][1] * h));
					ivec2 t1 = ivec2(numeric_cast<sint32>(vertUvs[1][0] * w), numeric_cast<sint32>(vertUvs[1][1] * h));
					ivec2 t2 = ivec2(numeric_cast<sint32>(vertUvs[2][0] * w), numeric_cast<sint32>(vertUvs[2][1] * h));
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

			textureData(albedo, special, positions, normals, xs, ys);
		}
	};

#ifdef UV_MODE_XATLAS
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
#endif
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
	{
		OPTICK_EVENT("inpaint");
		imageInpaint(albedo.get(), 3);
		imageInpaint(special.get(), 3);
	}
	CAGE_LOG_DEBUG(severityEnum::Info, "generator", stringizer() + "generated mesh with " + meshVertices.size() + " vertices, " + meshIndices.size() + " indices and texture resolution: " + albedo->width() + "x" + albedo->height());
}
