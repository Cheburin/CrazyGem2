#include "main.h"
#include <fstream>

// #include <assimp/Importer.hpp>      // C++ importer interface
// #include <assimp/scene.h>           // Output data structure
// #include <assimp/postprocess.h>     // Post processing flags

#include <locale>
#include <codecvt>
#include <string>
#include <array>
#include <locale> 

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

template<typename T>
void CreateBuffer(_In_ ID3D11Device* device, T const& data, D3D11_BIND_FLAG bindFlags, _Outptr_ ID3D11Buffer** pBuffer)
{
	assert(pBuffer != 0);

	D3D11_BUFFER_DESC bufferDesc = { 0 };

	bufferDesc.ByteWidth = (UINT)data.size() * sizeof(T::value_type);
	bufferDesc.BindFlags = bindFlags;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA dataDesc = { 0 };

	dataDesc.pSysMem = data.data();

	device->CreateBuffer(&bufferDesc, &dataDesc, pBuffer);

	//SetDebugObjectName(*pBuffer, "DirectXTK:GeometricPrimitive");
}

std::unique_ptr<DirectX::ModelMeshPart> CreateModelMeshPart(ID3D11Device* device, std::function<void(std::vector<VertexPositionNormalTangentColorTexture> & _vertices, std::vector<UINT> & _indices)> createGeometry){
	std::vector<VertexPositionNormalTangentColorTexture> vertices;
	std::vector<UINT> indices;

	createGeometry(vertices, indices);

	size_t nVerts = vertices.size();

	std::unique_ptr<DirectX::ModelMeshPart> modelMeshPArt(new DirectX::ModelMeshPart());

	modelMeshPArt->indexCount = indices.size();
	modelMeshPArt->startIndex = 0;
	modelMeshPArt->vertexOffset = 0;
	modelMeshPArt->vertexStride = sizeof(VertexPositionNormalTangentColorTexture);
	modelMeshPArt->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	modelMeshPArt->indexFormat = DXGI_FORMAT_R32_UINT;
	modelMeshPArt->vbDecl = std::shared_ptr<std::vector<D3D11_INPUT_ELEMENT_DESC>>(
		new std::vector<D3D11_INPUT_ELEMENT_DESC>(
		&VertexPositionNormalTangentColorTexture::InputElements[0],
		&VertexPositionNormalTangentColorTexture::InputElements[VertexPositionNormalTangentColorTexture::InputElementCount]
		)
		);

	CreateBuffer(device, vertices, D3D11_BIND_VERTEX_BUFFER, modelMeshPArt->vertexBuffer.ReleaseAndGetAddressOf());

	CreateBuffer(device, indices, D3D11_BIND_INDEX_BUFFER, modelMeshPArt->indexBuffer.ReleaseAndGetAddressOf());

	return modelMeshPArt;
}

XMFLOAT4X4& assign(XMFLOAT4X4& output, const aiMatrix4x4& aiMe){
	output._11 = aiMe.a1;
	output._12 = aiMe.a2;
	output._13 = aiMe.a3;
	output._14 = aiMe.a4;

	output._21 = aiMe.b1;
	output._22 = aiMe.b2;
	output._23 = aiMe.b3;
	output._24 = aiMe.b4;

	output._31 = aiMe.c1;
	output._32 = aiMe.c2;
	output._33 = aiMe.c3;
	output._34 = aiMe.c4;

	output._41 = aiMe.d1;
	output._42 = aiMe.d2;
	output._43 = aiMe.d3;
	output._44 = aiMe.d4;

	return output;
}

void collectMeshes(const aiScene* scene, int level, aiNode * node, std::vector<VertexPositionNormalTangentColorTexture> * _vertices, std::vector<UINT> * _indices, DirectX::XMFLOAT4X4 parentTransformation, unsigned int & vertex_size){

	SimpleMath::Matrix transformation = SimpleMath::Matrix(assign(XMFLOAT4X4(), node->mTransformation));

	transformation = transformation.Transpose();

	auto nodeTransformation = transformation * parentTransformation;

	char buffer[1024];
	sprintf(buffer, "%d %s \n", level, node->mName.C_Str());// , delta.y);
	OutputDebugStringA(buffer);

	if (std::string(node->mName.C_Str()).find("Meterial") == -1)
	{
		for (int i = 0; i < node->mNumMeshes; i++){
			auto nodeMesh = scene->mMeshes[node->mMeshes[i]];

			for (int k = 0; k < nodeMesh->mNumVertices; k++){
				VertexPositionNormalTangentColorTexture v;

				v.position = SimpleMath::Vector3::Transform(SimpleMath::Vector3(nodeMesh->mVertices[k].x, nodeMesh->mVertices[k].y, nodeMesh->mVertices[k].z), nodeTransformation);

				auto normal = SimpleMath::Vector3::TransformNormal(SimpleMath::Vector3(nodeMesh->mNormals[k].x, nodeMesh->mNormals[k].y, nodeMesh->mNormals[k].z), nodeTransformation.Invert().Transpose());
				normal.Normalize();
				v.normal = normal;

				if (nodeMesh->mTangents){
					auto tangent = SimpleMath::Vector3::TransformNormal(SimpleMath::Vector3(nodeMesh->mTangents[k].x, nodeMesh->mTangents[k].y, nodeMesh->mTangents[k].z), nodeTransformation);
					tangent.Normalize();
					v.tangent = SimpleMath::Vector4(tangent);

					auto bitangent = SimpleMath::Vector3::TransformNormal(SimpleMath::Vector3(nodeMesh->mBitangents[k].x, nodeMesh->mBitangents[k].y, nodeMesh->mBitangents[k].z), nodeTransformation);
					bitangent.Normalize();
					v.tangent.w = (normal.Cross(tangent).Dot(bitangent) < 0.0F) ? -1.0F : 1.0F;
				}

				v.textureCoordinate = SimpleMath::Vector2((nodeMesh->mTextureCoords)[0][k].x, (nodeMesh->mTextureCoords)[0][k].y);

				_vertices->push_back(v);
			}

			for (int m = 0; m < nodeMesh->mNumFaces; m++){
				if (nodeMesh->mFaces[m].mNumIndices != 3) throw "";
				for (int n = 0; n < 3; n++){
					_indices->push_back(vertex_size + nodeMesh->mFaces[m].mIndices[n]);
				};
			}

			vertex_size += nodeMesh->mNumVertices;
		}
	}

	for (int i = 0; i < node->mNumChildren; i++){

		collectMeshes(scene, ++level, node->mChildren[i], _vertices, _indices, nodeTransformation, vertex_size);
	}
}

float degToRad(float d){
	return (d * 3.141592654f) / 180.0f;
}

void loadModelFromFile(char * file_name, std::vector<VertexPositionNormalTangentColorTexture> & _vertices, std::vector<UINT> & _indices, SimpleMath::Matrix transformation){

	Assimp::Importer importer;

	// And have it read the given file with some example postprocessing
	// Usually - if speed is not the most important aspect for you - you'll 
	// propably to request more postprocessing than we do in this example.
	//aiProcess_Triangulate
	const aiScene* scene = importer.ReadFile(file_name, aiProcess_ConvertToLeftHanded | aiProcess_FlipUVs | aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace);// | aiProcess_MakeLeftHanded);

	auto node = scene->mRootNode;

	unsigned int vertex_size = 0;

	collectMeshes(scene, 0, node, &_vertices, &_indices, transformation, vertex_size);
}

///
void CreateBoundingVolumes(std::vector<VertexPositionNormalTangentColorTexture> & _vertices, BoundingVolumes* boundingVolumes)
{
	SimpleMath::Vector3& _min = boundingVolumes->_min;
	SimpleMath::Vector3& _max = boundingVolumes->_max;
	SimpleMath::Vector3& _modelCenter = boundingVolumes->_modelCenter;
	SimpleMath::Vector3& _size = boundingVolumes->_size;
	float& _d = boundingVolumes->_d;

	_min = SimpleMath::Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
	_max = SimpleMath::Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (UINT i = 0; i < _vertices.size(); i++)
	{
		// The minVertex and maxVertex will most likely not be actual vertices in the model, but vertices
		// that use the smallest and largest x, y, and z values from the model to be sure ALL vertices are
		// covered by the bounding volume

		//Get the smallest vertex 
		_min.x = min(_min.x, _vertices[i].position.x);    // Find smallest x value in model
		_min.y = min(_min.y, _vertices[i].position.y);    // Find smallest y value in model
		_min.z = min(_min.z, _vertices[i].position.z);    // Find smallest z value in model

		//Get the largest vertex 
		_max.x = max(_max.x, _vertices[i].position.x);    // Find largest x value in model
		_max.y = max(_max.y, _vertices[i].position.y);    // Find largest y value in model
		_max.z = max(_max.z, _vertices[i].position.z);    // Find largest z value in model
	}

	_size = _max - _min;

	SimpleMath::Vector3 halfSize = 0.5f * _size;

	_modelCenter = _max - halfSize;

	_d = _size.Length();
}