#include "main.h"

extern GraphicResources * G;

extern SceneState scene_state;

using namespace SimpleMath;

void loadMatrix_VP(Matrix & v, Matrix & p){
	v = Matrix(scene_state.mView).Transpose();
	p = Matrix(scene_state.mProjection).Transpose();
}
void loadMatrix_InvV(Matrix & InvV){
	InvV = Matrix(scene_state.mInvView).Transpose();
}
void loadMatrix_W(Matrix & w){
	w = Matrix(scene_state.mWorld).Transpose();
}
void storeMatrix(Matrix & w, Matrix & wv, Matrix & wvp){
	scene_state.mWorld = w.Transpose();
	scene_state.mWorldView = wv.Transpose();
	scene_state.mWorldViewProjection = wvp.Transpose();
}

void set_scene_world_matrix(DirectX::XMFLOAT4X4 transformation){
	Matrix w, v, p;
	loadMatrix_VP(v, p);

	w = transformation;
	Matrix  wv = w * v;
	Matrix wvp = wv * p;

	storeMatrix(w, wv, wvp);
}

void set_scene_world_matrix_into_camera_origin(DirectX::XMFLOAT4X4 transformation){
	Matrix w, v, inv_v, p;
	loadMatrix_VP(v, p);
	loadMatrix_InvV(inv_v);

	w = transformation * Matrix::CreateTranslation(inv_v._41, inv_v._42, inv_v._43);
	Matrix  wv = w * v;
	Matrix wvp = wv * p;

	storeMatrix(w, wv, wvp);
}

void Draw_Single_Point(ID3D11DeviceContext* context, IEffect* effect, _In_opt_ std::function<void __cdecl()> setCustomState){
	effect->Apply(context);

	setCustomState();

	context->IASetInputLayout(nullptr);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	context->Draw(1, 0);
}