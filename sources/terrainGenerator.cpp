
#include "terrain.h"

#include <cage-core/log.h>
#include <cage-core/geometry.h>
#include <cage-core/noise.h>
#include <cage-core/color.h>
#include <cage-core/random.h>
#include <cage-core/png.h>

#include "dualmc.h"

namespace
{
	const uint32 globalSeed = (uint32)currentRandomGenerator().next();

	const uint32 quadsPerTile = 20;
	const uint32 texelsPerQuad = 10;
	const real uvBorderFraction = 0.2;

	holder<noiseClass> newClouds(uint32 seed, uint32 octaves)
	{
		noiseCreateConfig cfg;
		cfg.octaves = octaves;
		cfg.type = noiseTypeEnum::Value;
		return newNoise(cfg);
	}

	template <class T>
	T rescale(const T &v, real ia, real ib, real oa, real ob)
	{
		return (v - ia) / (ib - ia) * (ob - oa) + oa;
	}

	struct meshGenStruct
	{
		std::vector<dualmc::Vertex> quadVertices; // model space, indexed
		std::vector<dualmc::Quad> quadIndices;
		std::vector<vec3> quadPositions; // world space, NOT indexed
		std::vector<vec3> quadNormals;
		std::vector<float> densities;
		vec3 tp;
		uint32 quadsPerLine;

		meshGenStruct()
		{
			densities.reserve(quadsPerTile * quadsPerTile * quadsPerTile);
		}

		void genDensities(const tilePosStruct &tilePos)
		{
			tp = (vec3(tilePos.x, tilePos.y, tilePos.z) - 0.5) * tileLength;
			for (uint32 z = 0; z < quadsPerTile; z++)
			{
				for (uint32 y = 0; y < quadsPerTile; y++)
				{
					for (uint32 x = 0; x < quadsPerTile; x++)
					{
						vec3 d = vec3(x, y, z) * tileLength / (quadsPerTile - 3);
						densities.push_back(terrainDensity(tp + d).value);
					}
				}
			}
		}

		void genSurface()
		{
			dualmc::DualMC<float> mc;
			mc.build(densities.data(), quadsPerTile, quadsPerTile, quadsPerTile, 0, true, false, quadVertices, quadIndices);
		}

		vec3 mc2c(const dualmc::Vertex &v)
		{
			return (vec3(v.x, v.y, v.z) / (quadsPerTile - 3) - 0.5) * tileLength;
		}

		void genNormals()
		{
			quadNormals.resize(quadVertices.size());
			for (dualmc::Quad q : quadIndices)
			{
				vec3 p[4] = {
					mc2c(quadVertices[q.i0]),
					mc2c(quadVertices[q.i1]),
					mc2c(quadVertices[q.i2]),
					mc2c(quadVertices[q.i3])
				};
				vec3 n = cross(p[1] - p[0], p[3] - p[0]).normalize();
				for (uint32 i : { q.i0, q.i1, q.i2, q.i3 })
					quadNormals[i] += n;
			}
			for (vec3 &n : quadNormals)
				n = n.normalize();
		}

		void genOutput(std::vector<vertexStruct> &meshVertices, std::vector<uint32> &meshIndices)
		{
			CAGE_ASSERT_RUNTIME(meshVertices.empty());
			CAGE_ASSERT_RUNTIME(meshIndices.empty());
			meshVertices.reserve(quadIndices.size() * 6 / 4);
			quadPositions.reserve(meshVertices.size());
			uint32 quadsCount = numeric_cast<uint32>(quadIndices.size());
			quadsPerLine = numeric_cast<uint32>(sqrt(quadsCount));
			if (quadsPerLine * quadsPerLine < quadsCount)
				quadsPerLine++;
			uint32 quadIndex = 0;
			real uvw = real(1) / quadsPerLine;
			for (const auto &q : quadIndices)
			{
				vec3 p[4] = {
					mc2c(quadVertices[q.i0]),
					mc2c(quadVertices[q.i1]),
					mc2c(quadVertices[q.i2]),
					mc2c(quadVertices[q.i3])
				};
				for (const vec3 &pi : p)
					quadPositions.push_back(pi + tp);
				vec3 n[4] = {
					quadNormals[q.i0],
					quadNormals[q.i1],
					quadNormals[q.i2],
					quadNormals[q.i3]
				};
				uint32 qx = quadIndex % quadsPerLine;
				uint32 qy = quadIndex / quadsPerLine;
				vec2 uv[4] = {
					vec2(0, 0),
					vec2(1, 0),
					vec2(1, 1),
					vec2(0, 1)
				};
				for (vec2 &u : uv)
				{
					u = rescale(u, 0, 1, uvBorderFraction, 1 - uvBorderFraction);
					u = (vec2(qx, qy) + u) * uvw;
				}
				bool which = p[0].squaredDistance(p[2]) < p[1].squaredDistance(p[3]); // split the quad by shorter diagonal
				static const int first[6] = { 0,1,2, 0,2,3 };
				static const int second[6] = { 1,2,3, 1,3,0 };
				for (uint32 i : which ? first : second)
				{
					vertexStruct v;
					v.position = p[i];
					v.normal = n[i];
					v.uv = uv[i];
					meshVertices.push_back(v);
				}
				quadIndex++;
			}
		}

		void genTextures(holder<pngImageClass> &albedo, holder<pngImageClass> &special)
		{
			uint32 quadsCount = numeric_cast<uint32>(quadPositions.size() / 4);
			uint32 res = quadsPerLine * texelsPerQuad;
			albedo = newPngImage();
			albedo->empty(res, res, 3);
			special = newPngImage();
			special->empty(res, res, 2);
			for (uint32 y = 0; y < res; y++)
			{
				for (uint32 x = 0; x < res; x++)
				{
					uint32 xx = x / texelsPerQuad;
					uint32 yy = y / texelsPerQuad;
					CAGE_ASSERT_RUNTIME(xx < quadsPerLine && yy < quadsPerLine, x, y, xx, yy, texelsPerQuad, quadsPerLine, res);
					uint32 quadIndex = yy * quadsPerLine + xx;
					if (quadIndex >= quadsCount)
						break;
					vec2 f = vec2(x % texelsPerQuad, y % texelsPerQuad) / texelsPerQuad;
					CAGE_ASSERT_RUNTIME(f[0] >= 0 && f[0] <= 1 && f[1] >= 0 && f[1] <= 1, f);
					f = rescale(f, uvBorderFraction, 1 - uvBorderFraction, 0, 1);
					//f = (f - uvBorderFraction) / (1 - uvBorderFraction * 2);
					vec3 pos = interpolate(
						interpolate(quadPositions[quadIndex * 4 + 0], quadPositions[quadIndex * 4 + 1], f[0]),
						interpolate(quadPositions[quadIndex * 4 + 3], quadPositions[quadIndex * 4 + 2], f[0]),
						f[1]);
					vec3 alb; vec2 spc;
					terrainMaterial(pos, alb, spc);
					for (uint32 i = 0; i < 3; i++)
						albedo->value(x, y, i, alb[i].value);
					for (uint32 i = 0; i < 2; i++)
						special->value(x, y, i, spc[i].value);
				}
			}
			//albedo->encodeFile(string() + "textures/" + tp + ".png");
		}
	};

	vec3 pdnToRgb(real h, real s, real v)
	{
		return convertHsvToRgb(vec3(h / 360, s / 100, v / 100));
	}

	real sharpEdge(real v)
	{
		return rescale(clamp(v, 0.45, 0.55), 0.45, 0.55, 0, 1);
	}

	vec3 recolor(const vec3 &color, real deviation, uint32 seed, const vec3 &pos)
	{
		static holder<noiseClass> value1 = newClouds(globalSeed + 100, 1);
		static holder<noiseClass> value2 = newClouds(globalSeed + 101, 1);
		real h = (value1->evaluate(pos * 3) * 0.5 + 0.5) * 0.5 + 0.25;
		real v = (value2->evaluate(pos * 4) * 0.5 + 0.5);
		vec3 hsv = convertRgbToHsv(color) + (vec3(h, 1 - v, v) - 0.5) * deviation;
		hsv[0] = (hsv[0] + 1) % 1;
		return convertHsvToRgb(clamp(hsv, vec3(), vec3(1, 1, 1)));
	}
}

real terrainDensity(const vec3 &pos)
{
	vec3 p2(pos[1], -pos[2], pos[0]);
	static holder<noiseClass> clouds1 = newClouds(globalSeed + 200, 3);
	static holder<noiseClass> clouds2 = newClouds(globalSeed + 201, 3);
	return clouds1->evaluate(pos * 0.2) - clouds2->evaluate(p2 * 0.13);
}

void terrainMaterial(const vec3 &pos, vec3 &albedo, vec2 &special)
{
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

	static holder<noiseClass> clouds1 = newClouds(globalSeed + 300, 8);
	real c = ((clouds1->evaluate(pos * 0.042) * 0.5 + 0.5) * 16) % 8;
	uint32 i = numeric_cast<uint32>(c);
	real f = sharpEdge(c - i);
	albedo = interpolate(colors[i], colors[(i + 1) % 8], f);
	albedo = recolor(albedo, 0.1, globalSeed + 548, pos);
	special = vec2(0.8, 0.002);
}

vec3 colorDeviation(const vec3 &color, real deviation)
{
	vec3 hsv = convertRgbToHsv(color) + (randomChance3() - 0.5) * deviation;
	hsv[0] = (hsv[0] + 1) % 1;
	return convertHsvToRgb(clamp(hsv, vec3(), vec3(1, 1, 1)));
}

void terrainGenerate(const tilePosStruct &tilePos, std::vector<vertexStruct> &meshVertices, std::vector<uint32> &meshIndices, holder<pngImageClass> &albedo, holder<pngImageClass> &special)
{
	// generate mesh
	meshGenStruct mesh;
	mesh.genDensities(tilePos);
	mesh.genSurface();
	mesh.genNormals();
	mesh.genOutput(meshVertices, meshIndices);
	CAGE_LOG_DEBUG(severityEnum::Info, "generator", string() + "generated mesh with " + meshVertices.size() + " vertices and " + meshIndices.size() + " indices");

	if (meshVertices.size() == 0)
		return;

	// generate textures
	mesh.genTextures(albedo, special);
}
