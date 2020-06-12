#include "terrain.h"

#include <cage-core/image.h>
#include <cage-core/polyhedron.h>
#include <cage-core/collider.h>
#include <cage-core/marchingCubes.h>
#include <cage-core/noiseFunction.h>
#include <cage-core/random.h>
#include <cage-core/color.h>

#include <algorithm>
#include <vector>

namespace
{
	const uint32 globalSeed = (uint32)detail::getApplicationRandomGenerator().next();

	Holder<NoiseFunction> newClouds(uint32 seed, uint32 octaves)
	{
		NoiseFunctionCreateConfig cfg;
		cfg.octaves = octaves;
		cfg.type = NoiseTypeEnum::Value;
		cfg.seed = seed;
		return newNoiseFunction(cfg);
	}

	Holder<NoiseFunction> densityNoise1 = newClouds(globalSeed + 1, 3);
	Holder<NoiseFunction> densityNoise2 = newClouds(globalSeed + 2, 3);
	Holder<NoiseFunction> colorNoise1 = newClouds(globalSeed + 3, 3);
	Holder<NoiseFunction> colorNoise2 = newClouds(globalSeed + 4, 2);
	Holder<NoiseFunction> colorNoise3 = newClouds(globalSeed + 5, 4);

	vec3 pdnToRgb(real h, real s, real v)
	{
		return colorHsvToRgb(vec3(h / 360, s / 100, v / 100));
	}

	template <class T>
	T rescale(const T &v, real ia, real ib, real oa, real ob)
	{
		return (v - ia) / (ib - ia) * (ob - oa) + oa;
	}

	real sharpEdge(real v)
	{
		return rescale(clamp(v, 0.45, 0.55), 0.45, 0.55, 0, 1);
	}

	struct ProcTile
	{
		TilePos pos;
		Holder<Polyhedron> mesh;
		Holder<Collider> collider;
		Holder<Image> albedo;
		Holder<Image> special;
		uint32 textureResolution = 0;
	};

	real meshgenerator(ProcTile *t, const vec3 &pl)
	{
		vec3 p = t->pos.getTransform() * pl;
		real a = densityNoise1->evaluate(p * 0.2);
		real b = densityNoise2->evaluate(vec3(p[1], -p[2], p[0]) * 0.13);
		return a - b;
	}

	void generateMesh(ProcTile &t)
	{
		OPTICK_EVENT("generateMesh");

		{
			MarchingCubesCreateConfig cfg;
			cfg.resolutionX = cfg.resolutionY = cfg.resolutionZ = 16;
			cfg.box = aabb(vec3(-1), vec3(1));
			cfg.clip = false;
			Holder<MarchingCubes> cubes = newMarchingCubes(cfg);
			{
				OPTICK_EVENT("densities");
				cubes->updateByPosition(Delegate<real(const vec3 &)>().bind<ProcTile *, &meshgenerator>(&t));
			}
			{
				OPTICK_EVENT("marchingCubes");
				t.mesh = cubes->makePolyhedron();
			}
			//t.mesh->exportObjFile({}, stringizer() + "debug/" + t.pos + "/1.obj");
		}

		{
			//OPTICK_EVENT("simplify");
			//PolyhedronSimplificationConfig cfg;
			//cfg.minEdgeLength = 0.02;
			//cfg.maxEdgeLength = 0.5;
			//cfg.approximateError = 0.03;
			//cfg.useProjection = false;
			//t.mesh->simplify(cfg);
			//t.mesh->discardInvalid(); // simplification occasionally generates nan points
			//t.mesh->exportObjFile({}, stringizer() + "debug/" + t.pos + "/2.obj");
		}

		{
			OPTICK_EVENT("clip");
			t.mesh->clip(aabb(vec3(-1.01), vec3(1.01)));
			t.mesh->mergeCloseVertices(0.02); // clipping sometimes generates very small triangles
			//t.mesh->exportObjFile({}, stringizer() + "debug/" + t.pos + "/3.obj");
		}

		{
			OPTICK_EVENT("unwrap");
			PolyhedronUnwrapConfig cfg;
			cfg.texelsPerUnit = 50.0f;
			t.textureResolution = t.mesh->unwrap(cfg);
			//CAGE_LOG(SeverityEnum::Info, "generator", stringizer() + "texture resolution: " + t.textureResolution + " (" + t.pos + ")");
			//t.mesh->exportObjFile({}, stringizer() + "debug/" + t.pos + "/4.obj");
			CAGE_ASSERT(t.textureResolution <= 4096);
		}

		//auto msh = t.mesh->copy();
		//msh->applyTransform(t.pos.getTransform());
		//msh->exportObjFile({}, stringizer() + "debug/" + t.pos + ".obj");
	}

	void generateCollider(ProcTile &t)
	{
		OPTICK_EVENT("generateCollider");
		t.collider = newCollider();
		t.collider->importPolyhedron(t.mesh.get());
		t.collider->rebuild();
	}

	void textureGenerator(ProcTile *t, uint32 x, uint32 y, const ivec3 &idx, const vec3 &weights)
	{
		vec3 position = t->mesh->positionAt(idx, weights) * t->pos.getTransform();

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

		real c = colorNoise3->evaluate(position * 0.042);
		real u = ((c * 0.5 + 0.5) * 16) % 8;
		uint32 i = numeric_cast<uint32>(u);
		real f = sharpEdge(u - i);
		vec3 color = interpolate(colors[i], colors[(i + 1) % 8], f);
		real hi = (colorNoise1->evaluate(position * 3) * +0.5 + 0.5) * 0.5 + 0.25;
		real vi = colorNoise1->evaluate(position * 4) * 0.5 + 0.5;
		vec3 hsv = colorRgbToHsv(color) + (vec3(hi, 1 - vi, vi) - 0.5) * 0.1;
		hsv[0] = (hsv[0] + 1) % 1;
		color = colorHsvToRgb(clamp(hsv, vec3(0), vec3(1)));

		t->albedo->set(x, y, color);
		t->special->set(x, y, vec2(0.8, 0.002));
	}

	void generateTextures(ProcTile &t)
	{
		OPTICK_EVENT("generateTextures");
		t.albedo = newImage();
		t.albedo->initialize(t.textureResolution, t.textureResolution, 3);
		t.special = newImage();
		t.special->initialize(t.textureResolution, t.textureResolution, 2);
		PolyhedronTextureGenerationConfig cfg;
		cfg.generator.bind<ProcTile *, &textureGenerator>(&t);
		cfg.width = cfg.height = t.textureResolution;
		t.mesh->generateTexture(cfg);
		{
			OPTICK_EVENT("inpaint");
			t.albedo->inpaint(2);
			t.special->inpaint(2);
		}
		t.special->colorConfig.gammaSpace = GammaSpaceEnum::Linear;

		//auto tex = t.albedo->copy();
		//tex->verticalFlip();
		//tex->exportFile(stringizer() + "debug/" + t.pos + ".png");
	}
}

void terrainGenerate(const TilePos &tilePos, Holder<Polyhedron> &mesh, Holder<Collider> &collider, Holder<Image> &albedo, Holder<Image> &special)
{
	OPTICK_EVENT("terrainGenerate");
	
	ProcTile t;
	t.pos = tilePos;

	generateMesh(t);
	generateCollider(t);
	generateTextures(t);

	mesh = templates::move(t.mesh);
	collider = templates::move(t.collider);
	albedo = templates::move(t.albedo);
	special = templates::move(t.special);
}
