#include "terrain.h"

#include <cage-core/entities.h>
#include <cage-core/geometry.h>
#include <cage-core/concurrent.h>
#include <cage-core/assetManager.h>
#include <cage-core/memoryBuffer.h>
#include <cage-core/image.h>
#include <cage-core/collisionMesh.h>

#include <cage-engine/core.h>
#include <cage-engine/engine.h>
#include <cage-engine/graphics.h>
#include <cage-engine/opengl.h>
#include <cage-engine/assetStructs.h>
#include <cage-engine/graphics/shaderConventions.h>

#include <array>
#include <atomic>

namespace
{
	enum class TileStateEnum
	{
		Init,
		Generate,
		Generating,
		Upload,
		Fabricate,
		Entity,
		Ready,
		Defabricate,
		Unload1,
		Unload2,
	};

	struct Tile
	{
		std::atomic<TileStateEnum> status;
		std::vector<Vertex> cpuMeshVertices;
		std::vector<uint32> cpuMeshIndices;
		Holder<CollisionMesh> cpuCollider;
		Holder<Mesh> gpuMesh;
		Holder<Texture> gpuAlbedo;
		Holder<Image> cpuAlbedo;
		Holder<Texture> gpuMaterial;
		Holder<Image> cpuMaterial;
		//Holder<Texture> gpuNormal;
		//Holder<Image> cpuNormal;
		Holder<RenderObject> gpuObject;
		TilePos pos;
		Entity *entity_;
		uint32 meshName;
		uint32 albedoName;
		uint32 materialName;
		//uint32 normalName;
		uint32 objectName;

		Tile() : status(TileStateEnum::Init), meshName(0), albedoName(0), materialName(0), /*normalName(0),*/ objectName(0)
		{}

		real distanceToPlayer() const
		{
			return pos.distanceToPlayer();
		}
	};

	std::array<Tile, 4096> tiles;
	bool stopping;

	/////////////////////////////////////////////////////////////////////////////
	// CONTROL
	/////////////////////////////////////////////////////////////////////////////

	std::set<TilePos> findReadyTiles()
	{
		std::set<TilePos> readyTiles;
		for (Tile &t : tiles)
		{
			if (t.status == TileStateEnum::Ready)
				readyTiles.insert(t.pos);
		}
		return readyTiles;
	}

	void engineUpdate()
	{
		OPTICK_EVENT("terrainTiles");

		std::set<TilePos> neededTiles = stopping ? std::set<TilePos>() : findNeededTiles(findReadyTiles());
		for (Tile &t : tiles)
		{
			bool visible = false;
			bool requested = false;
			// find visibility
			if (t.status != TileStateEnum::Init)
			{
				auto it = neededTiles.find(t.pos);
				if (it != neededTiles.end())
				{
					visible = it->visible;
					requested = true;
					neededTiles.erase(it);
				}
			}
			// remove tiles
			if (t.status == TileStateEnum::Ready && (!requested || stopping))
			{
				if (t.cpuCollider)
				{
					t.cpuCollider.clear();
					t.entity_->destroy();
					t.entity_ = nullptr;
				}
				t.status = TileStateEnum::Defabricate;
			}
			// create entity
			else if (t.status == TileStateEnum::Entity)
			{
				if (t.cpuCollider)
				{
					{ // set texture names for the mesh
						uint32 textures[MaxTexturesCountPerMaterial];
						detail::memset(textures, 0, sizeof(textures));
						textures[0] = t.albedoName;
						textures[1] = t.materialName;
						//textures[2] = t.normalName;
						t.gpuMesh->setTextureNames(textures);
					}
					{ // set object properties
						float thresholds[1] = { 0 };
						uint32 meshIndices[2] = { 0, 1 };
						uint32 meshNames[1] = { t.meshName };
						t.gpuObject->setLods(1, 1, thresholds, meshIndices, meshNames);
					}
					{ // create the entity_
						t.entity_ = entities()->createAnonymous();
						CAGE_COMPONENT_ENGINE(Transform, tr, t.entity_);
						tr = t.pos.getTransform();
					}
				}
				t.status = TileStateEnum::Ready;
			}
			if (t.entity_)
			{
				CAGE_ASSERT(t.status == TileStateEnum::Ready);
				CAGE_ASSERT(!!t.cpuCollider);
				if (t.pos.visible != visible)
				{
					if (visible)
					{
						terrainAddCollider(t.objectName, t.cpuCollider.get(), t.pos.getTransform());
						CAGE_COMPONENT_ENGINE(Render, r, t.entity_);
						r.object = t.objectName;
					}
					else
					{
						terrainRemoveCollider(t.objectName);
						t.entity_->remove(RenderComponent::component);
					}
					t.pos.visible = visible;
				}
			}
			else
				t.pos.visible = false;
		}

		// generate new needed tiles
		for (Tile &t : tiles)
		{
			if (neededTiles.empty())
				break;
			if (t.status == TileStateEnum::Init)
			{
				t.pos = *neededTiles.begin();
				t.status = TileStateEnum::Generate;
				neededTiles.erase(neededTiles.begin());
			}
		}

		if (!neededTiles.empty())
		{
			CAGE_LOG(SeverityEnum::Warning, "flittermouse", "not enough terrain tile slots");
			detail::debugBreakpoint();
		}
	}

	void engineFinalize()
	{
		stopping = true;
	}

	/////////////////////////////////////////////////////////////////////////////
	// ASSETS
	/////////////////////////////////////////////////////////////////////////////

	void engineAssets()
	{
		OPTICK_EVENT("terrainAssets");

		if (stopping)
			engineUpdate();

		for (Tile &t : tiles)
		{
			if (t.status == TileStateEnum::Fabricate)
			{
				t.albedoName = assets()->generateUniqueName();
				t.materialName = assets()->generateUniqueName();
				//t.normalName = assets()->generateUniqueName();
				t.meshName = assets()->generateUniqueName();
				t.objectName = assets()->generateUniqueName();
				assets()->fabricate(assetSchemeIndexTexture, t.albedoName, stringizer() + "albedo " + t.pos);
				assets()->fabricate(assetSchemeIndexTexture, t.materialName, stringizer() + "material " + t.pos);
				//assets()->fabricate(assetSchemeIndexTexture, t.normalName, stringizer() + "normal " + t.pos);
				assets()->fabricate(assetSchemeIndexMesh, t.meshName, stringizer() + "mesh " + t.pos);
				assets()->fabricate(assetSchemeIndexRenderObject, t.objectName, stringizer() + "object " + t.pos);
				assets()->set<assetSchemeIndexTexture, Texture>(t.albedoName, t.gpuAlbedo.get());
				assets()->set<assetSchemeIndexTexture, Texture>(t.materialName, t.gpuMaterial.get());
				//assets()->set<assetSchemeIndexTexture, Texture>(t.normalName, t.gpuNormal.get());
				assets()->set<assetSchemeIndexMesh, Mesh>(t.meshName, t.gpuMesh.get());
				assets()->set<assetSchemeIndexRenderObject, RenderObject>(t.objectName, t.gpuObject.get());
				t.status = TileStateEnum::Entity;
			}
			else if (t.status == TileStateEnum::Defabricate)
			{
				assets()->remove(t.albedoName);
				assets()->remove(t.materialName);
				//assets()->remove(t.normalName);
				assets()->remove(t.meshName);
				assets()->remove(t.objectName);
				t.albedoName = 0;
				t.materialName = 0;
				//t.normalName = 0;
				t.meshName = 0;
				t.objectName = 0;
				t.status = TileStateEnum::Unload1;
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////////
	// DISPATCH
	/////////////////////////////////////////////////////////////////////////////

	Holder<Texture> dispatchTexture(Holder<Image> &image)
	{
		OPTICK_EVENT("dispatchTexture");
		if (!image)
			return Holder<Texture>();
		Holder<Texture> t = newTexture();
		switch (image->channels())
		{
		case 2:
			t->image2d(image->width(), image->height(), GL_RG8, GL_RG, GL_UNSIGNED_BYTE, image->bufferData());
			break;
		case 3:
			t->image2d(image->width(), image->height(), GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE, image->bufferData());
			break;
		}
		//t->filters(GL_NEAREST, GL_NEAREST, 0);
		t->filters(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, 100);
		t->wraps(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
		t->generateMipmaps();
		image.clear();
		return t;
	}

	Holder<Mesh> dispatchMesh(std::vector<Vertex> &vertices, std::vector<uint32> &indices)
	{
		OPTICK_EVENT("dispatchMesh");
		if (vertices.size() == 0)
			return Holder<Mesh>();
		Holder<Mesh> m = newMesh();
		MeshHeader::MaterialData material;
		m->setBuffers(numeric_cast<uint32>(vertices.size()), sizeof(Vertex), vertices.data(), numeric_cast<uint32>(indices.size()), indices.data(), sizeof(material), &material);
		m->setPrimitiveType(GL_TRIANGLES);
		m->setAttribute(CAGE_SHADER_ATTRIB_IN_POSITION, 3, GL_FLOAT, sizeof(Vertex), 0);
		m->setAttribute(CAGE_SHADER_ATTRIB_IN_NORMAL, 3, GL_FLOAT, sizeof(Vertex), 12);
		m->setAttribute(CAGE_SHADER_ATTRIB_IN_UV, 2, GL_FLOAT, sizeof(Vertex), 24);
		m->setBoundingBox(aabb(vec3(-1.5), vec3(1.5)));
		std::vector<Vertex>().swap(vertices);
		std::vector<uint32>().swap(indices);
		return m;
	}

	Holder<RenderObject> dispatchObject()
	{
		Holder<RenderObject> o = newRenderObject();
		return o;
	}

	void engineDispatch()
	{
		OPTICK_EVENT("terrainDispatch");

		CAGE_CHECK_GL_ERROR_DEBUG();
		bool uploaded = false;
		for (Tile &t : tiles)
		{
			switch (t.status)
			{
			case TileStateEnum::Unload1:
				t.status = TileStateEnum::Unload2;
				break;
			case TileStateEnum::Unload2:
			{
				t.gpuAlbedo.clear();
				t.gpuMaterial.clear();
				t.gpuMesh.clear();
				//t.gpuNormal.clear();
				t.gpuObject.clear();
				t.status = TileStateEnum::Init;
			} break;
			case TileStateEnum::Upload:
			{
				if (uploaded)
					continue; // upload at most one tile per frame
				t.gpuAlbedo = dispatchTexture(t.cpuAlbedo);
				t.gpuMaterial = dispatchTexture(t.cpuMaterial);
				//t.gpuNormal = dispatchTexture(t.cpuNormal);
				t.gpuMesh = dispatchMesh(t.cpuMeshVertices, t.cpuMeshIndices);
				t.gpuObject = dispatchObject();
				t.status = TileStateEnum::Fabricate;
				uploaded = true;
			} break;
			}
		}
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	/////////////////////////////////////////////////////////////////////////////
	// GENERATOR
	/////////////////////////////////////////////////////////////////////////////

	Tile *generatorChooseTile()
	{
		static Holder<Mutex> mut = newMutex();
		ScopeLock<Mutex> lock(mut);
		Tile *result = nullptr;
		for (Tile &t : tiles)
		{
			if (t.status != TileStateEnum::Generate)
				continue;
			if (result)
			{
				if (t.pos.radius < result->pos.radius)
					continue;
				if (t.distanceToPlayer() > result->distanceToPlayer())
					continue;
			}
			result = &t;
		}
		if (result)
			result->status = TileStateEnum::Generating;
		return result;
	}

	void generateCollider(Tile &t)
	{
		OPTICK_EVENT("generateCollider");

		if (t.cpuMeshVertices.empty())
			return;
		t.cpuCollider = newCollisionMesh();
		if (t.cpuMeshIndices.empty())
		{
			uint32 trisCount = numeric_cast<uint32>(t.cpuMeshVertices.size() / 3);
			for (uint32 ti = 0; ti < trisCount; ti++)
			{
				triangle tr;
				for (uint32 j = 0; j < 3; j++)
					tr.vertices[j] = t.cpuMeshVertices[ti * 3 + j].position;
				t.cpuCollider->addTriangle(tr);
			}
		}
		else
		{
			uint32 trisCount = numeric_cast<uint32>(t.cpuMeshIndices.size() / 3);
			for (uint32 ti = 0; ti < trisCount; ti++)
			{
				triangle tr;
				for (uint32 j = 0; j < 3; j++)
					tr.vertices[j] = t.cpuMeshVertices[t.cpuMeshIndices[ti * 3 + j]].position;
				t.cpuCollider->addTriangle(tr);
			}
		}
		t.cpuCollider->rebuild();
	}

	void generatorEntry()
	{
		while (!stopping)
		{
			Tile *t = generatorChooseTile();
			if (!t)
			{
				threadSleep(3000);
				continue;
			}
			terrainGenerate(t->pos, t->cpuMeshVertices, t->cpuMeshIndices, t->cpuAlbedo, t->cpuMaterial);
			generateCollider(*t);
			t->status = TileStateEnum::Upload;
		}
	}

	/////////////////////////////////////////////////////////////////////////////
	// INITIALIZE
	/////////////////////////////////////////////////////////////////////////////

	class Callbacks
	{
		std::vector<Holder<Thread>> generatorThreads;
		EventListener<void()> engineUpdateListener;
		EventListener<void()> engineAssetsListener;
		EventListener<void()> engineFinalizeListener;
		EventListener<void()> engineDispatchListener;
	public:
		Callbacks()
		{
			engineUpdateListener.attach(controlThread().update);
			engineUpdateListener.bind<&engineUpdate>();
			engineAssetsListener.attach(controlThread().assets);
			engineAssetsListener.bind<&engineAssets>();
			engineFinalizeListener.attach(controlThread().finalize);
			engineFinalizeListener.bind<&engineFinalize>();
			engineDispatchListener.attach(graphicsDispatchThread().render);
			engineDispatchListener.bind<&engineDispatch>();

			uint32 cpuCount = max(processorsCount(), 2u) - 1;
			for (uint32 i = 0; i < cpuCount; i++)
				generatorThreads.push_back(newThread(Delegate<void()>().bind<&generatorEntry>(), stringizer() + "generator " + i));
		}
	} callbacksInstance;
}
