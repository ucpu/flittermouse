#include "terrain.h"

#include <cage-core/image.h>
#include <cage-core/geometry.h>
#include <cage-core/noiseFunction.h>
#include <cage-core/color.h>
#include <cage-core/random.h>

Holder<NoiseFunction> newClouds(uint32 seed, uint32 octaves)
{
	NoiseFunctionCreateConfig cfg;
	cfg.octaves = octaves;
	cfg.type = NoiseTypeEnum::Value;
	cfg.seed = seed;
	return newNoiseFunction(cfg);
}

uint32 globalSeed()
{
	static const uint32 s = (uint32)detail::getApplicationRandomGenerator().next();
	return s;
}

namespace
{
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

	Holder<NoiseFunction> colorNoise1 = newClouds(globalSeed() + 3, 3);
	Holder<NoiseFunction> colorNoise2 = newClouds(globalSeed() + 4, 2);
	Holder<NoiseFunction> colorNoise3 = newClouds(globalSeed() + 5, 4);
}

void textureData(Holder<Image> &albedo, Holder<Image> &special, std::vector<vec3> &positions, std::vector<vec3> &normals, std::vector<uint32> &xs, std::vector<uint32> &ys)
{
	OPTICK_EVENT("textureData");
	OPTICK_TAG("count", positions.size());

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


