#include "terrain.h"

#include <cage-core/geometry.h>
#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>
#include <cage-core/image.h>

#include "dualmc.h"

namespace
{
	const uint32 globalSeed = (uint32)currentRandomGenerator().next();

	const uint32 quadsPerTile = 24;
	const uint32 texelsPerQuad = 10;
	const real uvBorderFraction = 0.2;

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

	holder<noiseFunction> densityNoise1 = newClouds(globalSeed + 1, 3);
	holder<noiseFunction> densityNoise2 = newClouds(globalSeed + 2, 3);
	holder<noiseFunction> colorNoise1 = newClouds(globalSeed + 3, 3);
	holder<noiseFunction> colorNoise2 = newClouds(globalSeed + 4, 2);
	holder<noiseFunction> colorNoise3 = newClouds(globalSeed + 5, 4);

	struct meshGenStruct
	{
		const tilePosStruct tilePos;
		mat4 tr;
		std::vector<real> densities;
		std::vector<dualmc::Vertex> mcVertices;
		std::vector<dualmc::Quad> mcIndices;
		std::vector<vec3> mcNormals;
		std::vector<vec3> quadPositions;
		std::vector<vec3> quadNormals;
		uint32 quadsPerLine;

		meshGenStruct(const tilePosStruct &tilePos) : tilePos(tilePos)
		{
			tr = mat4(tilePos.getTransform());
		}

		void genDensities()
		{
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
						vec3 w = l2w(m2c(v));
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
			dualmc::DualMC<float> mc;
			mc.build((float*)densities.data(), quadsPerTile, quadsPerTile, quadsPerTile, 0, true, false, mcVertices, mcIndices);
		}

		// marching cubes space -> local (normalized) space
		vec3 m2c(const dualmc::Vertex &v) const
		{
			return (vec3(v.x, v.y, v.z) - 2) / (quadsPerTile - 5) * 2 - 1; // returns -1..+1
		}

		vec3 l2w(const vec3 &p) const
		{
			return vec3(tr * vec4(p, 1));
		}

		void genNormals()
		{
			mcNormals.resize(mcVertices.size());
			for (dualmc::Quad q : mcIndices)
			{
				vec3 p[4] = {
					m2c(mcVertices[q.i0]),
					m2c(mcVertices[q.i1]),
					m2c(mcVertices[q.i2]),
					m2c(mcVertices[q.i3])
				};
				vec3 n = cross(p[1] - p[0], p[3] - p[0]).normalize();
				for (uint32 i : { q.i0, q.i1, q.i2, q.i3 })
					mcNormals[i] += n;
			}
			for (vec3 &n : mcNormals)
				n = n.normalize();
		}

		void genOutput(std::vector<vertexStruct> &meshVertices, std::vector<uint32> &meshIndices)
		{
			CAGE_ASSERT_RUNTIME(meshVertices.empty());
			CAGE_ASSERT_RUNTIME(meshIndices.empty());
			meshVertices.reserve(mcIndices.size() * 6 / 4);
			quadPositions.reserve(meshVertices.capacity());
			quadNormals.reserve(meshVertices.capacity());
			uint32 quadsCount = numeric_cast<uint32>(mcIndices.size());
			quadsPerLine = numeric_cast<uint32>(sqrt(quadsCount));
			if (quadsPerLine * quadsPerLine < quadsCount)
				quadsPerLine++;
			uint32 quadIndex = 0;
			real uvw = real(1) / quadsPerLine;
			for (const auto &q : mcIndices)
			{
				vec3 p[4] = {
					m2c(mcVertices[q.i0]),
					m2c(mcVertices[q.i1]),
					m2c(mcVertices[q.i2]),
					m2c(mcVertices[q.i3])
				};
				for (const vec3 &pi : p)
					quadPositions.push_back(l2w(pi));
				vec3 n[4] = {
					mcNormals[q.i0],
					mcNormals[q.i1],
					mcNormals[q.i2],
					mcNormals[q.i3]
				};
				for (const vec3 &ni : n)
					quadNormals.push_back(ni);
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

		void genTextures(holder<image> &albedo, holder<image> &special)
		{
			uint32 quadsCount = numeric_cast<uint32>(quadPositions.size() / 4);
			uint32 res = quadsPerLine * texelsPerQuad;
			albedo = newImage();
			albedo->empty(res, res, 3);
			special = newImage();
			special->empty(res, res, 2);
			std::vector<vec3> positions;
			positions.reserve(res * res);
			std::vector<vec3> normals;
			normals.reserve(res * res);
			std::vector<uint32> xs;
			xs.reserve(res * res);
			std::vector<uint32> ys;
			ys.reserve(res * res);
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
					vec3 pos = interpolate(
						interpolate(quadPositions[quadIndex * 4 + 0], quadPositions[quadIndex * 4 + 1], f[0]),
						interpolate(quadPositions[quadIndex * 4 + 3], quadPositions[quadIndex * 4 + 2], f[0]),
						f[1]);
					vec3 nor = interpolate(
						interpolate(quadNormals[quadIndex * 4 + 0], quadNormals[quadIndex * 4 + 1], f[0]),
						interpolate(quadNormals[quadIndex * 4 + 3], quadNormals[quadIndex * 4 + 2], f[0]),
						f[1]).normalize();
					positions.push_back(pos);
					normals.push_back(nor);
					xs.push_back(x);
					ys.push_back(y);
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

			std::vector<real> h;
			h.resize(pos.size());
			vec3 *position = positions.data();
			for (vec3 &p : pos)
				p = *position++ * 3;
			colorNoise1->evaluate(numeric_cast<uint32>(pos.size()), pos.data(), h.data());
			std::vector<real> v;
			v.resize(pos.size());
			position = positions.data();
			for (vec3 &p : pos)
				p = *position++ * 4;
			colorNoise2->evaluate(numeric_cast<uint32>(pos.size()), pos.data(), v.data());
			real *hi = h.data();
			real *vi = v.data();
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
		}
	};
}

void terrainGenerate(const tilePosStruct &tilePos, std::vector<vertexStruct> &meshVertices, std::vector<uint32> &meshIndices, holder<image> &albedo, holder<image> &special)
{
	// generate mesh
	meshGenStruct generator(tilePos);
	generator.genDensities();
	generator.genSurface();
	generator.genNormals();
	generator.genOutput(meshVertices, meshIndices);
	CAGE_LOG_DEBUG(severityEnum::Info, "generator", string() + "generated mesh with " + meshVertices.size() + " vertices and " + meshIndices.size() + " indices");

	if (meshVertices.size() == 0)
		return;

	// generate textures
	generator.genTextures(albedo, special);
}
