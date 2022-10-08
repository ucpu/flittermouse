#include "terrain.h"

#include <cage-core/entities.h>
#include <cage-core/concurrent.h>
#include <cage-core/assetManager.h>
#include <cage-core/debug.h>
#include <cage-core/meshImport.h>
#include <cage-core/serialization.h>
#include <cage-engine/scene.h>
#include <cage-engine/opengl.h>
#include <cage-engine/assetStructs.h>
#include <cage-engine/model.h>
#include <cage-engine/texture.h>
#include <cage-engine/renderObject.h>
#include <cage-engine/graphicsError.h>
#include <cage-simple/engine.h>

#include <vector>
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
		Entity,
		Ready,
	};

	struct TileBase
	{
		Holder<Collider> cpuCollider;
		Holder<Mesh> cpuMesh;
		Holder<Model> gpuMesh;
		Holder<Image> cpuAlbedo;
		Holder<Texture> gpuAlbedo;
		Holder<Image> cpuSpecial;
		Holder<Texture> gpuSpecial;
		Holder<RenderObject> renderObject;
		TilePos pos;
		Entity *entity = nullptr;
		uint32 meshName = 0;
		uint32 albedoName = 0;
		uint32 specialName = 0;
		uint32 objectName = 0;

		Real distanceToPlayer() const
		{
			return pos.distanceToPlayer();
		}
	};

	struct Tile : public TileBase
	{
		std::atomic<TileStateEnum> status {TileStateEnum::Init};
	};

	std::vector<Holder<Thread>> generatorThreads;
	std::array<Tile, 4096> tiles;
	std::atomic<bool> stopping;

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
		AssetManager *ass = engineAssets();
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
				if (t.entity)
				{
					ass->remove(t.meshName);
					ass->remove(t.albedoName);
					ass->remove(t.specialName);
					ass->remove(t.objectName);
					t.entity->destroy();
				}
				if (t.pos.visible)
					terrainRemoveCollider(t.objectName);
				(TileBase&)t = TileBase();
				t.status = TileStateEnum::Init;
			}

			// create entity
			else if (t.status == TileStateEnum::Entity)
			{
				{ // create the entity
					t.entity = engineEntities()->createAnonymous();
					TransformComponent &tr = t.entity->value<TransformComponent>();
					tr = t.pos.getTransform();
				}

				t.status = TileStateEnum::Ready;
			}

			if (t.entity)
			{
				CAGE_ASSERT(t.status == TileStateEnum::Ready);
				CAGE_ASSERT(!!t.cpuCollider);
				if (t.pos.visible != visible)
				{
					if (visible)
					{
						terrainAddCollider(t.objectName, t.cpuCollider.share(), t.pos.getTransform());
						RenderComponent &r = t.entity->value<RenderComponent>();
						r.object = t.objectName;
					}
					else
					{
						terrainRemoveCollider(t.objectName);
						t.entity->remove<RenderComponent>();
					}
					t.pos.visible = visible;
				}
			}
			else
				t.pos.visible = false;
		}

		terrainRebuildColliders();

		// generate new needed tiles
		for (Tile &t : tiles)
		{
			if (neededTiles.empty())
				break;
			if (t.status == TileStateEnum::Init)
			{
				t.pos = *neededTiles.begin();
				neededTiles.erase(neededTiles.begin());
				t.status = TileStateEnum::Generate;
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
		generatorThreads.clear();
	}

	/////////////////////////////////////////////////////////////////////////////
	// DISPATCH
	/////////////////////////////////////////////////////////////////////////////

	Holder<Texture> dispatchTexture(Holder<Image> &image)
	{
		Holder<Texture> t = newTexture();
		t->importImage(+image);
		t->filters(GL_LINEAR, GL_LINEAR, 100);
		t->wraps(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
		image.clear();
		return t;
	}

	Holder<Model> dispatchMesh(Holder<Mesh> &poly)
	{
		Holder<Model> m = newModel();
		MeshImportMaterial mat;
		m->importMesh(+poly, bufferView(mat));
		poly.clear();
		return m;
	}

	void engineDispatch()
	{
		AssetManager *ass = engineAssets();
		CAGE_CHECK_GL_ERROR_DEBUG();
		for (Tile &t : tiles)
		{
			if (t.status == TileStateEnum::Upload)
			{
				t.gpuAlbedo = dispatchTexture(t.cpuAlbedo);
				t.gpuSpecial = dispatchTexture(t.cpuSpecial);
				t.gpuMesh = dispatchMesh(t.cpuMesh);
				t.gpuMesh->textureNames[0] = t.albedoName;
				t.gpuMesh->textureNames[1] = t.specialName;

				// transfer asset ownership
				ass->fabricate<AssetSchemeIndexTexture, Texture>(t.albedoName, std::move(t.gpuAlbedo), Stringizer() + "albedo " + t.pos);
				ass->fabricate<AssetSchemeIndexTexture, Texture>(t.specialName, std::move(t.gpuSpecial), Stringizer() + "special " + t.pos);
				ass->fabricate<AssetSchemeIndexModel, Model>(t.meshName, std::move(t.gpuMesh), Stringizer() + "mesh " + t.pos);
				ass->fabricate<AssetSchemeIndexRenderObject, RenderObject>(t.objectName, std::move(t.renderObject), Stringizer() + "object " + t.pos);

				t.status = TileStateEnum::Entity;
				break;
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

	void generateRenderObject(Tile &t)
	{
		t.renderObject = newRenderObject();
		Real thresholds[1] = { 0 };
		uint32 meshIndices[2] = { 0, 1 };
		uint32 meshNames[1] = { t.meshName };
		t.renderObject->setLods(thresholds, meshIndices, meshNames);
	}

	void generatorEntry()
	{
		AssetManager *ass = engineAssets();
		while (!stopping)
		{
			Tile *t = generatorChooseTile();
			if (!t)
			{
				threadSleep(10000);
				continue;
			}

			terrainGenerate(t->pos, t->cpuMesh, t->cpuCollider, t->cpuAlbedo, t->cpuSpecial);
			if (!t->cpuMesh)
			{
				t->status = TileStateEnum::Ready;
				continue;
			}

			// assets names
			t->albedoName = ass->generateUniqueName();
			t->specialName = ass->generateUniqueName();
			t->meshName = ass->generateUniqueName();
			t->objectName = ass->generateUniqueName();

			generateRenderObject(*t);

			t->status = TileStateEnum::Upload;
		}
	}

	/////////////////////////////////////////////////////////////////////////////
	// INITIALIZE
	/////////////////////////////////////////////////////////////////////////////

	void engineInitialize()
	{
		uint32 cpuCount = max(processorsCount(), 2u) - 1;
		for (uint32 i = 0; i < cpuCount; i++)
			generatorThreads.push_back(newThread(Delegate<void()>().bind<&generatorEntry>(), Stringizer() + "generator " + i));
	}

	class Callbacks
	{
		EventListener<void()> engineUpdateListener;
		EventListener<void()> engineInitializeListener;
		EventListener<void()> engineFinalizeListener;
		EventListener<void()> engineUnloadListener;
		EventListener<void()> engineDispatchListener;
	public:
		Callbacks()
		{
			engineUpdateListener.attach(controlThread().update);
			engineUpdateListener.bind<&engineUpdate>();
			engineInitializeListener.attach(controlThread().initialize);
			engineInitializeListener.bind<&engineInitialize>();
			engineFinalizeListener.attach(controlThread().finalize);
			engineFinalizeListener.bind<&engineFinalize>();
			engineUnloadListener.attach(controlThread().unload);
			engineUnloadListener.bind<&engineUpdate>();
			engineDispatchListener.attach(graphicsDispatchThread().dispatch);
			engineDispatchListener.bind<&engineDispatch>();
		}
	} callbacksInstance;
}
