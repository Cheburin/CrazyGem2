#include "GeometricPrimitive.h"
#include "Effects.h"
#include "DirectXHelpers.h"
#include "Model.h"
#include "CommonStates.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "SimpleMath.h"

#include "DirectXMath.h"

#include "DXUT.h"

#include <wrl.h>

#include <map>
#include <algorithm>
#include <array>
#include <memory>
#include <assert.h>
#include <malloc.h>
#include <Exception>

#include "ConstantBuffer.h"

#include "AppBuffers.h"

using namespace DirectX;

struct EffectShaderFileDef{
	WCHAR * name;
	WCHAR * entry_point;
	WCHAR * shader_ver;
};
class IPostProcess
{
public:
	virtual ~IPostProcess() { }

	virtual void __cdecl Process(_In_ ID3D11DeviceContext* deviceContext, _In_opt_ std::function<void __cdecl()> setCustomState = nullptr) = 0;
};

inline ID3D11RenderTargetView** renderTargetViewToArray(ID3D11RenderTargetView* rtv1, ID3D11RenderTargetView* rtv2 = 0, ID3D11RenderTargetView* rtv3 = 0){
	static ID3D11RenderTargetView* rtvs[10];
	rtvs[0] = rtv1;
	rtvs[1] = rtv2;
	rtvs[2] = rtv3;
	return rtvs;
};
inline ID3D11ShaderResourceView** shaderResourceViewToArray(ID3D11ShaderResourceView* rtv1, ID3D11ShaderResourceView* rtv2 = 0, ID3D11ShaderResourceView* rtv3 = 0, ID3D11ShaderResourceView* rtv4 = 0, ID3D11ShaderResourceView* rtv5 = 0){
	static ID3D11ShaderResourceView* srvs[10];
	srvs[0] = rtv1;
	srvs[1] = rtv2;
	srvs[2] = rtv3;
	srvs[3] = rtv4;
	srvs[4] = rtv5;
	return srvs;
};
inline ID3D11Buffer** constantBuffersToArray(ID3D11Buffer* c1, ID3D11Buffer* c2){
	static ID3D11Buffer* cbs[10];
	cbs[0] = c1;
	cbs[1] = c2;
	return cbs;
};
inline ID3D11Buffer** constantBuffersToArray(DirectX::ConstantBuffer<SceneState> &cb){
	static ID3D11Buffer* cbs[10];
	cbs[0] = cb.GetBuffer();
	return cbs;
};
inline ID3D11Buffer** constantBuffersToArray(DirectX::ConstantBuffer<BlurHandling> &cb){
	static ID3D11Buffer* cbs[10];
	cbs[0] = cb.GetBuffer();
	return cbs;
};
inline ID3D11Buffer** constantBuffersToArray(DirectX::ConstantBuffer<SceneState> &c1, DirectX::ConstantBuffer<RayState> &c2){
	static ID3D11Buffer* cbs[10];
	cbs[0] = c1.GetBuffer();
	cbs[1] = c2.GetBuffer();
	return cbs;
};
inline ID3D11SamplerState** samplerStateToArray(ID3D11SamplerState* ss1, ID3D11SamplerState* ss2 = 0){
	static ID3D11SamplerState* sss[10];
	sss[0] = ss1;
	sss[1] = ss2;
	return sss;
};

namespace Camera{
	void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext);
}

std::unique_ptr<DirectX::IEffect> createHlslEffect(ID3D11Device* device, std::map<const WCHAR*, EffectShaderFileDef>& fileDef);

std::unique_ptr<DirectX::ModelMeshPart> CreateModelMeshPart(ID3D11Device* device, std::function<void(std::vector<VertexPositionNormalTangentColorTexture> & _vertices, std::vector<UINT> & _indices)> createGeometry);

void Draw_Single_Point(ID3D11DeviceContext* context, IEffect* effect, _In_opt_ std::function<void __cdecl()> setCustomState);

void set_scene_world_matrix(DirectX::XMFLOAT4X4 transformation = SimpleMath::Matrix::Identity);
void set_scene_world_matrix_into_camera_origin(DirectX::XMFLOAT4X4 transformation = SimpleMath::Matrix::Identity);
void loadModelFromFile(char * file_name, std::vector<VertexPositionNormalTangentColorTexture> & _vertices, std::vector<UINT> & _indices, SimpleMath::Matrix transformation = SimpleMath::Matrix::Identity);
struct BoundingVolumes;
void CreateBoundingVolumes(std::vector<VertexPositionNormalTangentColorTexture> & _vertices, BoundingVolumes* boundingVolumes);

struct BoundingVolumes{
	SimpleMath::Vector3 _min;
	SimpleMath::Vector3 _max;
	SimpleMath::Vector3 _modelCenter;
	SimpleMath::Vector3 _size;
	float _d;
};
class GraphicResources {
public:
	std::unique_ptr<CommonStates> render_states;

	std::unique_ptr<DirectX::IEffect> ground_effect;
	
	std::unique_ptr<DirectX::IEffect> sky_effect;

	std::unique_ptr<DirectX::IEffect> box_effect;

	std::unique_ptr<DirectX::IEffect> figure_effect;

	std::unique_ptr<DirectX::IEffect> bench_effect;

	std::unique_ptr<DirectX::IEffect> ray_effect;

	std::unique_ptr<DirectX::IEffect> sphere_hit_effect;

	std::unique_ptr<DirectX::IEffect> sphere_effect;

	std::unique_ptr<DirectX::IEffect> tbn_effect;

	std::unique_ptr<DirectX::ConstantBuffer<SceneState> > scene_constant_buffer;

	std::unique_ptr<DirectX::ConstantBuffer<RayState> > ray_constant_buffer;

	std::unique_ptr<DirectX::ModelMeshPart> ground_model;

	std::unique_ptr<DirectX::ModelMeshPart> figure_model;

	std::unique_ptr<DirectX::ModelMeshPart> bench_model;

	Microsoft::WRL::ComPtr<ID3D11InputLayout> model_input_layout;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sky_cube_texture;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> figure_normal_texture;

	std::unique_ptr<GeometricPrimitive> sphere_model;

	std::map<std::string, BoundingVolumes> boundingVolumes;

	Microsoft::WRL::ComPtr<ID3D11BlendState> alfaBlend;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> envDepth_T2D;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> envDepth_DSVs[6];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> envDepth_SRV;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> envColor_T2D;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> envColor_RTVs[6];
};

class SwapChainGraphicResources {
public:
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthStencilSRV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilV;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilT;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> colorSRV;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> colorV;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> colorT;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalSRV;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> normalV;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> normalT;
};