#include "terrain.h"

#include <cage-core/image.h>
#include <cage-core/mesh.h>
#include <cage-core/collider.h>
#include <cage-core/marchingCubes.h>
#include <cage-core/noiseFunction.h>
#include <cage-core/random.h>
#include <cage-core/color.h>

#include <algorithm>
#include <vector>
#include <array>

namespace
{
	const uint32 GlobalSeed = (uint32)detail::globalRandomGenerator().next();

	uint32 newSeed()
	{
		static uint32 index = 35741890;
		index = hash(index);
		return GlobalSeed + index;
	}

	Holder<NoiseFunction> newClouds(uint32 octaves)
	{
		NoiseFunctionCreateConfig cfg;
		cfg.seed = newSeed();
		cfg.octaves = octaves;
		cfg.type = NoiseTypeEnum::Value;
		return newNoiseFunction(cfg);
	}

	Holder<NoiseFunction> newValue()
	{
		return newClouds(0);
	}

	Holder<NoiseFunction> newCell(NoiseOperationEnum operation = NoiseOperationEnum::Distance, NoiseDistanceEnum distance = NoiseDistanceEnum::Euclidean)
	{
		NoiseFunctionCreateConfig cfg;
		cfg.seed = newSeed();
		cfg.type = NoiseTypeEnum::Cellular;
		cfg.operation = operation;
		cfg.distance = distance;
		return newNoiseFunction(cfg);
	}

	template<class T>
	real evaluateOrig(Holder<NoiseFunction> &noiseFunction, const T &position)
	{
		return noiseFunction->evaluate(position);
	}

	template<class T>
	real evaluateClamp(Holder<NoiseFunction> &noiseFunction, const T &position)
	{
		return noiseFunction->evaluate(position) * 0.5 + 0.5;
	}

	real rerange(real v, real ia, real ib, real oa, real ob)
	{
		return (v - ia) / (ib - ia) * (ob - oa) + oa;
	}

	real sharpEdge(real v)
	{
		return rerange(clamp(v, 0.45, 0.55), 0.45, 0.55, 0, 1);
	}

	vec3 pdnToRgb(real h, real s, real v)
	{
		return colorHsvToRgb(vec3(h / 360, s / 100, v / 100));
	}

	template<uint32 N, class T>
	T ninterpolate(const T v[N], real f) // f is 0..1
	{
		CAGE_ASSERT(f >= 0 && f < 1);
		f *= (N - 1); // 0..(N-1)
		uint32 i = numeric_cast<uint32>(f);
		CAGE_ASSERT(i + 1 < N);
		return interpolate(v[i], v[i + 1], f - i);
	}

	vec3 recolor(const vec3 &color, real deviation, const vec3 &pos)
	{
		static Holder<NoiseFunction> value1 = newValue();
		static Holder<NoiseFunction> value2 = newValue();
		static Holder<NoiseFunction> value3 = newValue();

		real h = evaluateClamp(value1, pos) * 0.5 + 0.25;
		real s = evaluateClamp(value2, pos);
		real v = evaluateClamp(value3, pos);
		vec3 hsv = colorRgbToHsv(color) + (vec3(h, s, v) - 0.5) * deviation;
		hsv[0] = (hsv[0] + 1) % 1;
		return colorHsvToRgb(clamp(hsv, 0, 1));
	}

	void darkRockGeneral(const vec3 &pos, vec3 &color, real &roughness, real &metallic, const vec3 *colors, uint32 colorsCount)
	{
		static Holder<NoiseFunction> clouds1 = newClouds(3);
		static Holder<NoiseFunction> clouds2 = newClouds(3);
		static Holder<NoiseFunction> clouds3 = newClouds(3);
		static Holder<NoiseFunction> clouds4 = newClouds(3);
		static Holder<NoiseFunction> clouds5 = newClouds(3);

		vec3 off = vec3(evaluateClamp(clouds1, pos * 0.065), evaluateClamp(clouds2, pos * 0.104), evaluateClamp(clouds3, pos * 0.083));
		real f = evaluateClamp(clouds4, pos * 0.0756 + off);
		switch (colorsCount)
		{
		case 3: color = ninterpolate<3>(colors, f); break;
		case 4: color = ninterpolate<4>(colors, f); break;
		default: CAGE_THROW_CRITICAL(NotImplemented, "unsupported colorsCount");
		}
		color = recolor(color, 0.1, pos * 2.1);
		roughness = evaluateClamp(clouds5, pos * 1.132) * 0.4 + 0.3;
		metallic = 0.02;
	}

	void basePaper(const vec3 &pos, vec3 &color, real &roughness, real &metallic)
	{
		static Holder<NoiseFunction> clouds1 = newClouds(5);
		static Holder<NoiseFunction> clouds2 = newClouds(5);
		static Holder<NoiseFunction> clouds3 = newClouds(5);
		static Holder<NoiseFunction> clouds4 = newClouds(3);
		static Holder<NoiseFunction> clouds5 = newClouds(3);
		static Holder<NoiseFunction> cell1 = newCell(NoiseOperationEnum::Distance2, NoiseDistanceEnum::Euclidean);
		static Holder<NoiseFunction> cell2 = newCell(NoiseOperationEnum::Distance2, NoiseDistanceEnum::Euclidean);
		static Holder<NoiseFunction> cell3 = newCell(NoiseOperationEnum::Distance2, NoiseDistanceEnum::Euclidean);

		vec3 off = vec3(evaluateClamp(cell1, pos * 0.063), evaluateClamp(cell2, pos * 0.063), evaluateClamp(cell3, pos * 0.063));
		if (evaluateClamp(clouds4, pos * 0.097 + off * 2.2) < 0.6)
		{ // rock 1
			color = colorHsvToRgb(vec3(
				evaluateClamp(clouds1, pos * 0.134) * 0.01 + 0.08,
				evaluateClamp(clouds2, pos * 0.344) * 0.2 + 0.2,
				evaluateClamp(clouds3, pos * 0.100) * 0.4 + 0.55
			));
			roughness = evaluateClamp(clouds5, pos * 0.848) * 0.5 + 0.3;
			metallic = 0.02;
		}
		else
		{ // rock 2
			color = colorHsvToRgb(vec3(
				evaluateClamp(clouds1, pos * 0.321) * 0.02 + 0.094,
				evaluateClamp(clouds2, pos * 0.258) * 0.3 + 0.08,
				evaluateClamp(clouds3, pos * 0.369) * 0.2 + 0.59
			));
			roughness = 0.5;
			metallic = 0.049;
		}
	}

	void baseSphinx(const vec3 &pos, vec3 &color, real &roughness, real &metallic)
	{
		// https://www.canstockphoto.com/egyptian-sphinx-palette-26815891.html

		static const vec3 colors[4] = {
			pdnToRgb(31, 34, 96),
			pdnToRgb(31, 56, 93),
			pdnToRgb(26, 68, 80),
			pdnToRgb(21, 69, 55)
		};

		static Holder<NoiseFunction> clouds1 = newClouds(4);
		static Holder<NoiseFunction> clouds2 = newClouds(3);

		real off = evaluateClamp(clouds1, pos * 0.0041);
		real y = (pos[1] * 0.012 + 1000) % 4;
		real c = (y + off * 2 - 1 + 4) % 4;
		uint32 i = numeric_cast<uint32>(c);
		real f = sharpEdge(c - i);
		if (i < 3)
			color = interpolate(colors[i], colors[i + 1], f);
		else
			color = interpolate(colors[3], colors[0], f);
		color = recolor(color, 0.1, pos * 1.1);
		roughness = evaluateClamp(clouds2, pos * 0.941) * 0.3 + 0.4;
		metallic = 0.02;
	}

	void baseWhite(const vec3 &pos, vec3 &color, real &roughness, real &metallic)
	{
		// https://www.pinterest.com/pin/432908582921844576/

		static const vec3 colors[3] = {
			pdnToRgb(19, 1, 96),
			pdnToRgb(14, 3, 88),
			pdnToRgb(217, 9, 74)
		};

		static Holder<NoiseFunction> clouds1 = newClouds(3);
		static Holder<NoiseFunction> clouds2 = newClouds(3);
		static Holder<NoiseFunction> clouds3 = newClouds(3);
		static Holder<NoiseFunction> clouds4 = newClouds(3);
		static Holder<NoiseFunction> value1 = newValue();

		vec3 off = vec3(evaluateClamp(clouds1, pos * 0.1), evaluateClamp(clouds2, pos * 0.1), evaluateClamp(clouds3, pos * 0.1));
		real n = evaluateClamp(value1, pos * 0.1 + off);
		color = ninterpolate<3>(colors, n);
		color = recolor(color, 0.2, pos * 0.72);
		color = recolor(color, 0.13, pos * 1.3);
		roughness = pow(evaluateClamp(clouds4, pos * 1.441), 0.5) * 0.7 + 0.01;
		metallic = 0.05;
	}

	void baseDarkRock1(const vec3 &pos, vec3 &color, real &roughness, real &metallic)
	{
		// https://www.goodfreephotos.com/united-states/colorado/other-colorado/rock-cliff-in-the-fog-in-colorado.jpg.php

		static Holder<NoiseFunction> clouds1 = newClouds(3);
		static Holder<NoiseFunction> clouds2 = newClouds(3);
		static Holder<NoiseFunction> clouds3 = newClouds(3);
		static Holder<NoiseFunction> clouds4 = newClouds(3);
		static Holder<NoiseFunction> clouds5 = newClouds(3);
		static Holder<NoiseFunction> cell1 = newCell(NoiseOperationEnum::Subtract);
		static Holder<NoiseFunction> value1 = newValue();

		vec3 off = vec3(evaluateClamp(clouds1, pos * 0.043), evaluateClamp(clouds2, pos * 0.043), evaluateClamp(clouds3, pos * 0.043));
		real f = evaluateClamp(cell1, pos * 0.0147 + off * 0.23);
		real m = evaluateClamp(clouds4, pos * 0.018);
		if (f < 0.017 && m < 0.35)
		{ // the vein
			static const vec3 vein[2] = {
				pdnToRgb(18, 18, 60),
				pdnToRgb(21, 22, 49)
			};

			color = interpolate(vein[0], vein[1], evaluateClamp(value1, pos));
			roughness = evaluateClamp(clouds5, pos * 0.718) * 0.3 + 0.3;
			metallic = 0.6;
		}
		else
		{ // the rocks
			static const vec3 colors[3] = {
				pdnToRgb(240, 1, 45),
				pdnToRgb(230, 5, 41),
				pdnToRgb(220, 25, 27)
			};

			darkRockGeneral(pos, color, roughness, metallic, colors, sizeof(colors) / sizeof(colors[0]));
		}
	}

	void baseDarkRock2(const vec3 &pos, vec3 &color, real &roughness, real &metallic)
	{
		// https://www.schemecolor.com/rocky-cliff-color-scheme.php

		static const vec3 colors[4] = {
			pdnToRgb(240, 1, 45),
			pdnToRgb(230, 6, 35),
			pdnToRgb(240, 11, 28),
			pdnToRgb(232, 27, 21)
		};

		darkRockGeneral(pos, color, roughness, metallic, colors, sizeof(colors) / sizeof(colors[0]));
	}

	void basesSwitch(uint32 baseIndex, const vec3 &pos, vec3 &color, real &roughness, real &metallic)
	{
		switch (baseIndex)
		{
		case 0: basePaper(pos, color, roughness, metallic); break;
		case 1: baseSphinx(pos, color, roughness, metallic); break;
		case 2: baseWhite(pos, color, roughness, metallic); break;
		case 3: baseDarkRock1(pos, color, roughness, metallic); break;
		case 4: baseDarkRock2(pos, color, roughness, metallic); break;
		default: CAGE_THROW_CRITICAL(NotImplemented, "unknown terrain base color enum");
		}
	}

	std::array<real, 5> basesWeights(const vec3 &pos)
	{
		static Holder<NoiseFunction> clouds1 = newClouds(3);
		static Holder<NoiseFunction> clouds2 = newClouds(3);
		static Holder<NoiseFunction> clouds3 = newClouds(3);
		static Holder<NoiseFunction> clouds4 = newClouds(3);
		static Holder<NoiseFunction> clouds5 = newClouds(3);

		const vec3 p = pos * 0.005;
		std::array<real, 5> result;
		result[0] = clouds1->evaluate(p);
		result[1] = clouds2->evaluate(p);
		result[2] = clouds3->evaluate(p);
		result[3] = clouds4->evaluate(p);
		result[4] = clouds5->evaluate(p);
		return result;
	}

	struct WeightIndex
	{
		real weight;
		uint32 index = m;
	};

	struct ProcTile
	{
		TilePos pos;
		Holder<Mesh> mesh;
		Holder<Collider> collider;
		Holder<Image> albedo;
		Holder<Image> special;
		uint32 textureResolution = 0;
	};

	real meshGeneratorImpl(const vec3 &pt)
	{
		static Holder<NoiseFunction> baseNoise = []()
		{
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Cubic;
			cfg.seed = newSeed();
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 1;
			cfg.frequency = 0.12;
			return newNoiseFunction(cfg);
		}();
		static Holder<NoiseFunction> bumpsNoise = []()
		{
			NoiseFunctionCreateConfig cfg;
			cfg.type = NoiseTypeEnum::Value;
			cfg.fractalType = NoiseFractalTypeEnum::Fbm;
			cfg.octaves = 3;
			cfg.seed = newSeed();
			cfg.frequency = 0.4;
			return newNoiseFunction(cfg);
		}();

		const real base = baseNoise->evaluate(pt) + 0.15;
		const real bumps = bumpsNoise->evaluate(pt) * 0.05;
		return base + bumps;
	}

	real meshGenerator(ProcTile *t, const vec3 &pl)
	{
		const vec3 pt = t->pos.getTransform() * pl;
		return meshGeneratorImpl(pt);
	}

	void textureGeneratorImpl(const vec3 &pos, vec3 &color, real &roughness, real &metallic)
	{
		static Holder<NoiseFunction> cell1 = newCell(NoiseOperationEnum::Subtract);
		static Holder<NoiseFunction> cell2 = newCell();
		static Holder<NoiseFunction> clouds1 = newClouds(3);
		static Holder<NoiseFunction> clouds2 = newClouds(2);

		{ // base
			std::array<real, 5> weights5 = basesWeights(pos);
			std::array<WeightIndex, 5> indices5;
			for (uint32 i = 0; i < 5; i++)
			{
				indices5[i].index = i;
				indices5[i].weight = weights5[i] + 1;
			}
			std::sort(std::begin(indices5), std::end(indices5), [](const WeightIndex &a, const WeightIndex &b) {
				return a.weight > b.weight;
				});
			{ // normalize
				real l;
				for (uint32 i = 0; i < 5; i++)
					l += sqr(indices5[i].weight);
				l = 1 / sqrt(l);
				for (uint32 i = 0; i < 5; i++)
					indices5[i].weight *= l;
			}
			vec2 w2 = normalize(vec2(indices5[0].weight, indices5[1].weight));
			CAGE_ASSERT(w2[0] >= w2[1]);
			real d = w2[0] - w2[1];
			real f = clamp(rerange(d, 0, 0.1, 0.5, 0), 0, 0.5);
			vec3 c[2]; real r[2]; real m[2];
			for (uint32 i = 0; i < 2; i++)
				basesSwitch(indices5[i].index, pos, c[i], r[i], m[i]);
			color = interpolate(c[0], c[1], f);
			roughness = interpolate(r[0], r[1], f);
			metallic = interpolate(m[0], m[1], f);
		}

		{ // small cracks
			real f = evaluateClamp(cell1, pos * 0.187);
			real m = evaluateClamp(clouds1, pos * 0.43);
			if (f < 0.02 && m < 0.5)
			{
				color *= 0.6;
				roughness *= 1.2;
			}
		}

		{ // white glistering spots
			if (evaluateClamp(cell2, pos * 0.084) > 0.95)
			{
				real c = evaluateClamp(clouds2, pos * 3) * 0.2 + 0.8;
				color = vec3(c);
				roughness = 0.2;
				metallic = 0.4;
			}
		}
	}

	void textureGenerator(ProcTile *t, const ivec2 &xy, const ivec3 &idx, const vec3 &weights)
	{
		vec3 position = t->mesh->positionAt(idx, weights) * t->pos.getTransform() * 10;
		vec3 color; real roughness; real metallic;
		textureGeneratorImpl(position, color, roughness, metallic);
		t->albedo->set(xy, color);
		t->special->set(xy, vec2(roughness, metallic));
	}

	float averageEdgeLength(const Mesh *poly)
	{
		CAGE_ASSERT(poly->type() == MeshTypeEnum::Triangles);
		CAGE_ASSERT(poly->indicesCount() > 0);
		real len = 0;
		const uint32 inds = poly->indicesCount();
		for (uint32 i = 0; i < inds; i += 3)
		{
			const vec3 a = poly->positions()[poly->indices()[i + 0]];
			const vec3 b = poly->positions()[poly->indices()[i + 1]];
			const vec3 c = poly->positions()[poly->indices()[i + 2]];
			len += distance(a, b);
			len += distance(b, c);
			len += distance(c, a);
		}
		return (len / inds).value;
	}

	void generateMesh(ProcTile &t)
	{
		{
			MarchingCubesCreateConfig cfg;
			cfg.resolution = ivec3(24);
			cfg.box = Aabb(vec3(-1), vec3(1));
			cfg.clip = false;
			Holder<MarchingCubes> cubes = newMarchingCubes(cfg);
			cubes->updateByPosition(Delegate<real(const vec3 &)>().bind<ProcTile *, &meshGenerator>(&t));
			t.mesh = cubes->makeMesh();
		}

		/*
		{
			PolyhedronSimplificationConfig cfg;
			cfg.minEdgeLength = 0.01;
			cfg.maxEdgeLength = 0.25;
			cfg.approximateError = 0.01;
			cfg.useProjection = false;
			polyhedronSimplify(+t.mesh, cfg);
			OPTICK_TAG("faces", t.mesh->facesCount());
			OPTICK_TAG("avgEdgeLen", averageEdgeLength(+t.mesh));
		}
		*/

		{
			meshClip(+t.mesh, Aabb(vec3(-1.005), vec3(1.005)));
		}

		{
			MeshUnwrapConfig cfg;
			cfg.texelsPerUnit = 50.0f;
			t.textureResolution = meshUnwrap(+t.mesh, cfg);
			CAGE_ASSERT(t.textureResolution <= 2048);
			if (t.textureResolution == 0)
				t.mesh->clear();
		}
	}

	void generateCollider(ProcTile &t)
	{
		t.collider = newCollider();
		t.collider->importMesh(t.mesh.get());
		t.collider->rebuild();
	}

	void generateTextures(ProcTile &t)
	{
		CAGE_ASSERT(t.textureResolution > 0);
		t.albedo = newImage();
		t.albedo->initialize(t.textureResolution, t.textureResolution, 3);
		t.special = newImage();
		t.special->initialize(t.textureResolution, t.textureResolution, 2);
		t.special->colorConfig.gammaSpace = GammaSpaceEnum::Linear;
		MeshGenerateTextureConfig cfg;
		cfg.generator.bind<ProcTile *, &textureGenerator>(&t);
		cfg.width = cfg.height = t.textureResolution;
		{
			meshGenerateTexture(+t.mesh, cfg);
		}
		{
			imageDilation(+t.albedo, 2);
			imageDilation(+t.special, 2);
		}

		//auto tex = t.albedo->copy();
		//tex->verticalFlip();
		//tex->exportFile(stringizer() + "debug/" + t.pos + ".png");
	}

	class Initializer
	{
	public:
		Initializer()
		{
			// ensure consistent order of initialization of all the static noise functions
			vec3 p, c;
			real r, m;
			for (uint32 i = 0; i < 5; i++)
				basesSwitch(i, p, c, r, m);
			textureGeneratorImpl(p, c, r, m);
			meshGeneratorImpl(p);
		}
	} initializer;
}

void terrainGenerate(const TilePos &tilePos, Holder<Mesh> &mesh, Holder<Collider> &collider, Holder<Image> &albedo, Holder<Image> &special)
{
	ProcTile t;
	t.pos = tilePos;

	generateMesh(t);
	if (t.mesh->facesCount() == 0)
		return;
	generateCollider(t);
	generateTextures(t);

	mesh = std::move(t.mesh);
	collider = std::move(t.collider);
	albedo = std::move(t.albedo);
	special = std::move(t.special);
}
