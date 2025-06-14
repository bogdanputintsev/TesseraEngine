#include "Mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <third-party/tiny_obj_loader.h>

#include "engine/EngineCore.h"
#include "engine/utils/Utils.h"
#include "services/Services.h"
#include "services/world/World.h"


namespace parus
{
	namespace
	{
		void calculateTangents(std::vector<math::Vertex>& vertices, const std::vector<uint32_t>& indices)
		{
			for (size_t i = 0; i < indices.size(); i += 3)
			{
				math::Vertex& v0 = vertices[indices[i]];
				math::Vertex& v1 = vertices[indices[i + 1]];
				math::Vertex& v2 = vertices[indices[i + 2]];

				// Triangle edges
				const math::Vector3 edge1 = v1.position - v0.position;
				const math::Vector3 edge2 = v2.position - v0.position;

				// Texture coordinates subtraction
				const math::Vector2 deltaUv1 = v1.textureCoordinates - v0.textureCoordinates;
				const math::Vector2 deltaUv2 = v2.textureCoordinates - v0.textureCoordinates;

				// Tangent calculation
				const float f = 1.0f / (deltaUv1.x * deltaUv2.y - deltaUv2.x * deltaUv1.y);

				math::Vector3 tangent;
				tangent.x = f * (deltaUv2.y * edge1.x - deltaUv1.y * edge2.x);
				tangent.y = f * (deltaUv2.y * edge1.y - deltaUv1.y * edge2.y);
				tangent.z = f * (deltaUv2.y * edge1.z - deltaUv1.y * edge2.z);
				tangent = tangent.normalize();

				// Assign tangent to all vertices of triangle
				v0.tangent = tangent;
				v1.tangent = tangent;
				v2.tangent = tangent;
			}
		}
	}
	

    Mesh importMeshFromFile(const std::string& filePath)
    {
        ASSERT(std::filesystem::exists(filePath),
			"File " + filePath + " must exist.");
    	ASSERT(std::filesystem::is_regular_file(filePath),
			"File " + filePath + " must be a regular file.");
	    ASSERT(utils::string::equalsIgnoreCase(std::filesystem::path(filePath).extension().string(), ".OBJ"),
			"File " + filePath + " must have OBJ extension");
    	
    	Mesh newMesh{};

		LOG_INFO("Loading mesh: " + filePath);

		// Load the model (vertices and indices)
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warningMessage;
		std::string errorMessage;

		size_t lastSlash = filePath.find_last_of("/\\");
		std::string baseDir = filePath.substr(0, lastSlash + 1);

		ASSERT(tinyobj::LoadObj(
			&attrib, &shapes, &materials, &warningMessage, &errorMessage,
			filePath.c_str(),
			baseDir.c_str(),
			true),
			warningMessage + errorMessage);

		if (!errorMessage.empty())
		{
			LOG_ERROR(errorMessage);
		}
		
		if (!warningMessage.empty())
		{
			LOG_WARNING(warningMessage);
		}
		
		std::vector<std::shared_ptr<vulkan::Material>> modelMaterials;
    	modelMaterials.reserve(materials.size());

		for (const auto& material : materials)
		{
			std::shared_ptr<vulkan::Material> newModelMaterial
				= Services::get<World>()->getStorage()->getOrLoadMaterial(
					material.name,
					baseDir + material.diffuse_texname,
					baseDir + material.bump_texname,
					baseDir + material.metallic_texname,
					baseDir + material.roughness_texname,
					baseDir + material.ambient_texname);

			ASSERT(newModelMaterial, "Material should exist after its loading.");
			modelMaterials.push_back(newModelMaterial);
		}

		std::unordered_map<int, MeshPart> materialMeshes;
		std::unordered_map<math::Vertex, uint32_t> uniqueVertices;

		ASSERT(!modelMaterials.empty(),
			"Default material is missing for mesh " + filePath);
		
		for (const auto& shape : shapes) 
		{
			size_t indexOffset = 0;

			// Process each face in the shape.
			for (size_t faceIndex = 0; faceIndex < shape.mesh.num_face_vertices.size(); faceIndex++)
			{
				int materialId = shape.mesh.material_ids[faceIndex];
				
				if (!materialMeshes.contains(materialId))
				{
					MeshPart newMeshPart;
					ASSERT(modelMaterials.size() > static_cast<size_t>(materialId),
						"Material with index " + std::to_string(materialId) + " must exist for mesh " + filePath);

					if (materialId == -1)
					{
						newMeshPart.material = Services::get<World>()->getStorage()->getDefaultMaterial();
					}
					else
					{
						newMeshPart.material = modelMaterials[materialId];
					}
					materialMeshes[materialId] = newMeshPart;
				}

				MeshPart& currentMesh = materialMeshes[materialId];
				
				size_t faceVertices = 3;
				for (size_t v = 0; v < faceVertices; v++)
				{
					tinyobj::index_t index = shape.mesh.indices[indexOffset + v];

					math::Vertex vertex{};
			
					vertex.position = {
						attrib.vertices[3 * index.vertex_index + 0],
						attrib.vertices[3 * index.vertex_index + 1],
						attrib.vertices[3 * index.vertex_index + 2]
					};

					if (index.normal_index >= 0)
					{
						vertex.normal = {
							(index.normal_index >= 0) ? attrib.normals[3 * index.normal_index + 0] : 0.0f,
							(index.normal_index >= 0) ? attrib.normals[3 * index.normal_index + 1] : 0.0f,
							(index.normal_index >= 0) ? attrib.normals[3 * index.normal_index + 2] : 0.0f
						};
					}
					else
					{
						// LOG_ERROR("Model " + filePath + " has missing normals that require recalculation.");
						vertex.normal = { 0.0f, 0.0f, 0.0f };
					}
					
					if (index.texcoord_index >= 0)
					{
						vertex.textureCoordinates = math::Vector2(
							attrib.texcoords[2 * index.texcoord_index + 0],
							1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
						);
					}
					else
					{
						vertex.textureCoordinates = {0.0f, 0.0f};
					}

					// Will be calculated after loading.
					vertex.tangent = math::Vector3();
					
					if (!uniqueVertices.contains(vertex))
					{
						uniqueVertices[vertex] = static_cast<uint32_t>(currentMesh.vertices.size());
						currentMesh.vertices.push_back(vertex);
					}
			
					currentMesh.indices.push_back(uniqueVertices[vertex]);
				}
				indexOffset += faceVertices;
			}

			uniqueVertices.clear();
		}

		// Convert temporary meshes to final model meshes
		for (auto& [matId, mesh] : materialMeshes)
		{
			calculateTangents(mesh.vertices, mesh.indices);
			mesh.vertexCount = mesh.vertices.size();
			mesh.indexCount = mesh.indices.size();
			newMesh.meshParts.push_back(mesh);
		}

    	return newMesh;
    }

	
}
