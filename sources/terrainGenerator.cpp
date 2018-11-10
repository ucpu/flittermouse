
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
	const uint32 resolution = 50;

	vec3 mc2c(const dualmc::Vertex &v)
	{
		return (vec3(v.x, v.y, v.z) / (resolution - 3) - 0.5) * tileLength;
	}

	struct meshGenStruct
	{
		std::vector<dualmc::Vertex> quadVertices;
		std::vector<dualmc::Quad> quadIndices;
		std::vector<vec3> quadNormals;
		std::vector<float> densities;

		meshGenStruct()
		{
			densities.reserve(resolution * resolution * resolution);
		}

		void genDensities(const tilePosStruct &tilePos)
		{
			vec3 t = (vec3(tilePos.x, tilePos.y, tilePos.z) - 0.5) * tileLength;
			for (uint32 z = 0; z < resolution; z++)
			{
				for (uint32 y = 0; y < resolution; y++)
				{
					for (uint32 x = 0; x < resolution; x++)
					{
						vec3 d = vec3(x, y, z) * tileLength / (resolution - 3);
						densities.push_back(terrainDensity(t + d).value);
					}
				}
			}
		}

		void genSurface()
		{
			dualmc::DualMC<float> mc;
			mc.build(densities.data(), resolution, resolution, resolution, 0, true, false, quadVertices, quadIndices);
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
			for (const auto &q : quadIndices)
			{
				vec3 p[4] = {
					mc2c(quadVertices[q.i0]),
					mc2c(quadVertices[q.i1]),
					mc2c(quadVertices[q.i2]),
					mc2c(quadVertices[q.i3])
				};
				vec3 n[4] = {
					quadNormals[q.i0],
					quadNormals[q.i1],
					quadNormals[q.i2],
					quadNormals[q.i3]
				};
				if (p[0].squaredDistance(p[2]) < p[1].squaredDistance(p[3]))
				{
					for (uint32 i : { 0,1,2, 0,2,3 })
					{
						vertexStruct v;
						v.position = p[i];
						v.normal = n[i];
						meshVertices.push_back(v);
					}
				}
				else
				{
					for (uint32 i : { 1,2,3, 1,3,0 })
					{
						vertexStruct v;
						v.position = p[i];
						v.normal = n[i];
						meshVertices.push_back(v);
					}
				}
			}
		}
	};
}

real terrainDensity(const vec3 &pos)
{
	return noiseValue(globalSeed, pos * 0.3) - 0.6;
	//return distance(pos, vec3(0, 0, 0)) - 12;
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
	albedo = newPngImage();
	albedo->empty(1, 1, 3);
	special = newPngImage();
	special->empty(1, 1, 2);
	// todo
}
