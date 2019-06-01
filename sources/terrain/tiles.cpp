#include <array>
#include <atomic>

#include "terrain.h"

#include <cage-core/entities.h>
#include <cage-core/geometry.h>
#include <cage-core/concurrent.h>
#include <cage-core/assetManager.h>
#include <cage-core/memoryBuffer.h>
#include <cage-core/image.h>
#include <cage-core/collider.h>

#include <cage-client/core.h>
#include <cage-client/engine.h>
#include <cage-client/graphics.h>
#include <cage-client/opengl.h>
#include <cage-client/assetStructs.h>
#include <cage-client/graphics/shaderConventions.h>

namespace
{
	enum class tileStatusEnum
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

	struct tileStruct
	{
		std::atomic<tileStatusEnum> status;
		std::vector<vertexStruct> cpuMeshVertices;
		std::vector<uint32> cpuMeshIndices;
		holder<colliderClass> cpuCollider;
		holder<meshClass> gpuMesh;
		holder<textureClass> gpuAlbedo;
		holder<imageClass> cpuAlbedo;
		holder<textureClass> gpuMaterial;
		holder<imageClass> cpuMaterial;
		//holder<textureClass> gpuNormal;
		//holder<imageClass> cpuNormal;
		holder<objectClass> gpuObject;
		tilePosStruct pos;
		entityClass *entity;
		uint32 meshName;
		uint32 albedoName;
		uint32 materialName;
		//uint32 normalName;
		uint32 objectName;

		tileStruct() : status(tileStatusEnum::Init), meshName(0), albedoName(0), materialName(0), /*normalName(0),*/ objectName(0)
		{}

		real distanceToPlayer() const
		{
			return pos.distanceToPlayer();
		}
	};

	std::array<tileStruct, 2048> tiles;
	bool stopping;

	/////////////////////////////////////////////////////////////////////////////
	// CONTROL
	/////////////////////////////////////////////////////////////////////////////

	std::set<tilePosStruct> findReadyTiles()
	{
		std::set<tilePosStruct> readyTiles;
		for (tileStruct &t : tiles)
		{
			if (t.status == tileStatusEnum::Ready)
				readyTiles.insert(t.pos);
		}
		return readyTiles;
	}

	void engineUpdate()
	{
		std::set<tilePosStruct> neededTiles = stopping ? std::set<tilePosStruct>() : findNeededTiles(findReadyTiles());
		for (tileStruct &t : tiles)
		{
			bool visible = false;
			bool requested = false;
			// find visibility
			if (t.status != tileStatusEnum::Init)
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
			if (t.status == tileStatusEnum::Ready && (!requested || stopping))
			{
				if (t.cpuCollider)
				{
					t.cpuCollider.clear();
					t.entity->destroy();
					t.entity = nullptr;
				}
				t.status = tileStatusEnum::Defabricate;
			}
			// create entity
			else if (t.status == tileStatusEnum::Entity)
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
					{ // create the entity
						t.entity = entities()->createAnonymous();
						ENGINE_GET_COMPONENT(transform, tr, t.entity);
						tr = t.pos.getTransform();
					}
				}
				t.status = tileStatusEnum::Ready;
			}
			if (t.entity)
			{
				CAGE_ASSERT_RUNTIME(t.status == tileStatusEnum::Ready);
				CAGE_ASSERT_RUNTIME(!!t.cpuCollider);
				if (t.pos.visible != visible)
				{
					if (visible)
					{
						terrainAddCollider(t.objectName, t.cpuCollider.get(), t.pos.getTransform());
						ENGINE_GET_COMPONENT(render, r, t.entity);
						r.object = t.objectName;
					}
					else
					{
						terrainRemoveCollider(t.objectName);
						t.entity->remove(renderComponent::component);
					}
					t.pos.visible = visible;
				}
			}
			else
				t.pos.visible = false;
		}

		// generate new needed tiles
		for (tileStruct &t : tiles)
		{
			if (neededTiles.empty())
				break;
			if (t.status == tileStatusEnum::Init)
			{
				t.pos = *neededTiles.begin();
				t.status = tileStatusEnum::Generate;
				neededTiles.erase(neededTiles.begin());
			}
		}

		if (!neededTiles.empty())
		{
			CAGE_LOG(severityEnum::Warning, "flittermouse", "not enough terrain tile slots");
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
		if (stopping)
			engineUpdate();
		for (tileStruct &t : tiles)
		{
			if (t.status == tileStatusEnum::Fabricate)
			{
				t.albedoName = assets()->generateUniqueName();
				t.materialName = assets()->generateUniqueName();
				//t.normalName = assets()->generateUniqueName();
				t.meshName = assets()->generateUniqueName();
				t.objectName = assets()->generateUniqueName();
				assets()->fabricate(assetSchemeIndexTexture, t.albedoName, string() + "albedo " + t.pos);
				assets()->fabricate(assetSchemeIndexTexture, t.materialName, string() + "material " + t.pos);
				//assets()->fabricate(assetSchemeIndexTexture, t.normalName, string() + "normal " + t.pos);
				assets()->fabricate(assetSchemeIndexMesh, t.meshName, string() + "mesh " + t.pos);
				assets()->fabricate(assetSchemeIndexObject, t.objectName, string() + "object " + t.pos);
				assets()->set<assetSchemeIndexTexture, textureClass>(t.albedoName, t.gpuAlbedo.get());
				assets()->set<assetSchemeIndexTexture, textureClass>(t.materialName, t.gpuMaterial.get());
				//assets()->set<assetSchemeIndexTexture, textureClass>(t.normalName, t.gpuNormal.get());
				assets()->set<assetSchemeIndexMesh, meshClass>(t.meshName, t.gpuMesh.get());
				assets()->set<assetSchemeIndexObject, objectClass>(t.objectName, t.gpuObject.get());
				t.status = tileStatusEnum::Entity;
				break;
			}
			else if (t.status == tileStatusEnum::Defabricate)
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
				t.status = tileStatusEnum::Unload1;
				break;
			}
		}
	}

	/////////////////////////////////////////////////////////////////////////////
	// DISPATCH
	/////////////////////////////////////////////////////////////////////////////

	holder<textureClass> dispatchTexture(holder<imageClass> &image)
	{
		if (!image)
			return holder<textureClass>();
		holder<textureClass> t = newTexture();
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

	holder<meshClass> dispatchMesh(std::vector<vertexStruct> &vertices, std::vector<uint32> &indices)
	{
		if (vertices.size() == 0)
			return holder<meshClass>();
		holder<meshClass> m = newMesh();
		meshHeaderStruct::materialDataStruct material;
		m->setBuffers(numeric_cast<uint32>(vertices.size()), sizeof(vertexStruct), vertices.data(), numeric_cast<uint32>(indices.size()), indices.data(), sizeof(material), &material);
		m->setPrimitiveType(GL_TRIANGLES);
		m->setAttribute(CAGE_SHADER_ATTRIB_IN_POSITION, 3, GL_FLOAT, sizeof(vertexStruct), 0);
		m->setAttribute(CAGE_SHADER_ATTRIB_IN_NORMAL, 3, GL_FLOAT, sizeof(vertexStruct), 12);
		m->setAttribute(CAGE_SHADER_ATTRIB_IN_UV, 2, GL_FLOAT, sizeof(vertexStruct), 24);
		m->setBoundingBox(aabb(vec3(-1.5), vec3(1.5)));
		std::vector<vertexStruct>().swap(vertices);
		std::vector<uint32>().swap(indices);
		return m;
	}

	holder<objectClass> dispatchObject()
	{
		holder<objectClass> o = newObject();
		return o;
	}

	void engineDispatch()
	{
		CAGE_CHECK_GL_ERROR_DEBUG();
		for (tileStruct &t : tiles)
		{
			switch (t.status)
			{
			case tileStatusEnum::Unload1:
				t.status = tileStatusEnum::Unload2;
				break;
			case tileStatusEnum::Unload2:
			{
				t.gpuAlbedo.clear();
				t.gpuMaterial.clear();
				t.gpuMesh.clear();
				//t.gpuNormal.clear();
				t.gpuObject.clear();
				t.status = tileStatusEnum::Init;
			} break;
			case tileStatusEnum::Upload:
			{
				t.gpuAlbedo = dispatchTexture(t.cpuAlbedo);
				t.gpuMaterial = dispatchTexture(t.cpuMaterial);
				//t.gpuNormal = dispatchTexture(t.cpuNormal);
				t.gpuMesh = dispatchMesh(t.cpuMeshVertices, t.cpuMeshIndices);
				t.gpuObject = dispatchObject();
				t.status = tileStatusEnum::Fabricate;
			} break;
			}
		}
		CAGE_CHECK_GL_ERROR_DEBUG();
	}

	/////////////////////////////////////////////////////////////////////////////
	// GENERATOR
	/////////////////////////////////////////////////////////////////////////////

	tileStruct *generatorChooseTile()
	{
		static holder<mutexClass> mut = newMutex();
		scopeLock<mutexClass> lock(mut);
		tileStruct *result = nullptr;
		for (tileStruct &t : tiles)
		{
			if (t.status != tileStatusEnum::Generate)
				continue;
			if (result && t.distanceToPlayer() > result->distanceToPlayer())
				continue;
			result = &t;
		}
		if (result)
			result->status = tileStatusEnum::Generating;
		return result;
	}

	void generateCollider(tileStruct &t)
	{
		if (t.cpuMeshVertices.empty())
			return;
		t.cpuCollider = newCollider();
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
			tileStruct *t = generatorChooseTile();
			if (!t)
			{
				threadSleep(3000);
				continue;
			}
			terrainGenerate(t->pos, t->cpuMeshVertices, t->cpuMeshIndices, t->cpuAlbedo, t->cpuMaterial);
			generateCollider(*t);
			t->status = tileStatusEnum::Upload;
		}
	}

	/////////////////////////////////////////////////////////////////////////////
	// INITIALIZE
	/////////////////////////////////////////////////////////////////////////////


	class callbacksInitClass
	{
		std::vector<holder<threadClass>> generatorThreads;
		eventListener<void()> engineUpdateListener;
		eventListener<void()> engineAssetsListener;
		eventListener<void()> engineFinalizeListener;
		eventListener<void()> engineDispatchListener;
	public:
		callbacksInitClass()
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
				generatorThreads.push_back(newThread(delegate<void()>().bind<&generatorEntry>(), string() + "generator " + i));
		}
	} callbacksInitInstance;
}
