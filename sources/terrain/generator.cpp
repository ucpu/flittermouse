#include "terrain.h"

#include <cage-core/geometry.h>
#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>
#include <cage-core/image.h>
#include <cage-core/enumerate.h>

#include "dualmc.h"
#include "xatlas.h"

#include <cstdarg>

namespace
{
	const uint32 globalSeed = (uint32)currentRandomGenerator().next();

	const uint32 quadsPerTile = 12;

	void destroyAtlas(void *ptr)
	{
		xatlas::Destroy((xatlas::Atlas*)ptr);
	}

	holder<xatlas::Atlas> newAtlas()
	{
		xatlas::Atlas *a = xatlas::Create();
		return holder<xatlas::Atlas>(a, a, delegate<void(void*)>().bind<&destroyAtlas>());
	}

	holder<noiseFunction> newClouds(uint32 seed, uint32 octaves)
	{
		noiseFunctionCreateConfig cfg;
		cfg.octaves = octaves;
		cfg.type = noiseTypeEnum::Value;
		cfg.seed = seed;
		return newNoiseFunction(cfg);
	}

	template <class T>
	T rescale(const T &v, real ia, real ib, real oa, real ob)
	{
		return (v - ia) / (ib - ia) * (ob - oa) + oa;
	}

	vec3 pdnToRgb(real h, real s, real v)
	{
		return convertHsvToRgb(vec3(h / 360, s / 100, v / 100));
	}

	real sharpEdge(real v)
	{
		return rescale(clamp(v, 0.45, 0.55), 0.45, 0.55, 0, 1);
	}

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
		std::vector<vec3> mcNormals;
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
		}

		void genNormals()
		{
			OPTICK_EVENT("genNormals");
			mcNormals.resize(mcVertices.size());
			for (dualmc::Quad q : mcIndices)
			{
				vec3 p[4] = {
					m2l(mcVertices[q.i0]),
					m2l(mcVertices[q.i1]),
					m2l(mcVertices[q.i2]),
					m2l(mcVertices[q.i3])
				};
				vec3 n = cross(p[1] - p[0], p[3] - p[0]).normalize();
				for (uint32 i : { q.i0, q.i1, q.i2, q.i3 })
					mcNormals[i] += n;
			}
			for (vec3 &n : mcNormals)
				n = n.normalize();
		}

		void genTriangles()
		{
			OPTICK_EVENT("genTriangles");
			CAGE_ASSERT_RUNTIME(meshVertices.empty());
			CAGE_ASSERT_RUNTIME(meshIndices.empty());
			meshVertices.reserve(mcVertices.size());
			for (auto it : enumerate(mcVertices))
			{
				vertexStruct v;
				v.position = m2l(*it);
				v.normal = mcNormals[it.cnt];
				meshVertices.push_back(v);
			}
			meshIndices.reserve(mcIndices.size() * 6 / 4);
			for (const auto &q : mcIndices)
			{
				const uint32 is[4] = { q.i0, q.i1, q.i2, q.i3 };
#define P(I) meshVertices[is[I]].position
				bool which = P(0).squaredDistance(P(2)) < P(1).squaredDistance(P(3)); // split the quad by shorter diagonal
#undef P
				static const int first[6] = { 0,1,2, 0,2,3 };
				static const int second[6] = { 1,2,3, 1,3,0 };
				for (uint32 i : (which ? first : second))
					meshIndices.push_back(is[i]);
			}
		}

		void genUvs()
		{
			OPTICK_EVENT("genUvs");
			atlas = newAtlas();

			{
				OPTICK_EVENT("AddMesh");

				xatlas::MeshDecl decl;
				decl.indexCount = meshIndices.size();
				decl.indexData = meshIndices.data();
				decl.indexFormat = xatlas::IndexFormat::UInt32;
				decl.vertexCount = meshVertices.size();
				decl.vertexPositionData = &meshVertices[0].position;
				decl.vertexNormalData = &meshVertices[0].normal;
				decl.vertexPositionStride = sizeof(vertexStruct);
				decl.vertexNormalStride = sizeof(vertexStruct);
				xatlas::AddMesh(atlas.get(), decl);
			}

			{
				OPTICK_EVENT("Generate");

				xatlas::ChartOptions chart;
				xatlas::PackOptions pack;
				pack.texelsPerUnit = 30;
				pack.createImage = true;
				pack.padding = 1;
				xatlas::Generate(atlas.get(), chart, nullptr, pack);

				CAGE_ASSERT_RUNTIME(atlas->meshCount == 1);
				CAGE_ASSERT_RUNTIME(atlas->atlasCount == 1);
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

			const uint32 w = atlas->width;
			const uint32 h = atlas->height;

			albedo = newImage();
			albedo->empty(w, h, 3);
			special = newImage();
			special->empty(w, h, 2);

			std::vector<std::vector<triangle>> triPos;
			std::vector<std::vector<triangle>> triNorms;
			std::vector<std::vector<triangle>> triUvs;
			{
				OPTICK_EVENT("prepTris");
				triPos.resize(atlas->chartCount);
				triNorms.resize(atlas->chartCount);
				triUvs.resize(atlas->chartCount);
				for (uint32 ch = 0; ch < atlas->chartCount; ch++)
				{
					const auto &chr = atlas->meshes->chartArray[ch];
					triPos[ch].reserve(chr.faceCount);
					triNorms[ch].reserve(chr.faceCount);
					triUvs[ch].reserve(chr.faceCount);
					for (uint32 faceIt = 0; faceIt < chr.faceCount; faceIt++)
					{
						const uint32 face = chr.faceArray[faceIt];
						const uint32 *ids = meshIndices.data() + face * 3;
						triangle p;
						triangle n;
						triangle u;
						for (uint32 i = 0; i < 3; i++)
						{
							const vertexStruct &v = meshVertices[ids[i]];
							p[i] = l2w(v.position);
							n[i] = v.normal;
							u[i] = vec3(v.uv, 0);
						}
						triPos[ch].push_back(p);
						triNorms[ch].push_back(n);
						triUvs[ch].push_back(u);
					}
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
				for (uint32 y = 0; y < h; y++)
				{
					for (uint32 x = 0; x < w; x++)
					{
						if (atlas->image[y * w + x] == 0)
							continue;
						vec2 uv = vec2(x, y) * whInv;
						uint32 ch = atlas->image[y * w + x] & 0x1FFFFFFF; //xatlas::kImageChartIndexMask;
						const uint32 tricnt = numeric_cast<uint32>(triPos[ch].size());
						for (uint32 i = 0; i < tricnt; i++)
						{
							vec2 b = barycoord(triUvs[ch][i], uv);
							if (!inside(b))
								continue;
							positions.push_back(interpolate(triPos[ch][i], b));
							normals.push_back(normalize(interpolate(triNorms[ch][i], b)));
							xs.push_back(x);
							ys.push_back(y);
							break;
						}
					}
				}
			}

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
				*hi = (*hi * + 0.5 + 0.5) * 0.5 + 0.25;
				*vi = *vi * 0.5 + 0.5;
				vec3 hsv = convertRgbToHsv(albedo) + (vec3(*hi, 1 - *vi, *vi) - 0.5) * 0.1;
				hsv[0] = (hsv[0] + 1) % 1;
				albedo = convertHsvToRgb(clamp(hsv, vec3(0), vec3(1)));
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

			//albedo->verticalFlip();
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

	// generate mesh
	meshGenStruct generator(tilePos, meshVertices, meshIndices);
	generator.genDensities();
	generator.genSurface();
	if (generator.mcIndices.size() == 0)
	{
		CAGE_ASSERT_RUNTIME(meshVertices.empty());
		CAGE_ASSERT_RUNTIME(meshIndices.empty());
		return;
	}
	generator.genNormals();
	generator.genTriangles();
	generator.genUvs();
	generator.genTextures(albedo, special);
	CAGE_LOG_DEBUG(severityEnum::Info, "generator", string() + "generated mesh with " + meshVertices.size() + " vertices, " + meshIndices.size() + " indices and texture resolution: " + albedo->width() + "x" + albedo->height());
}
