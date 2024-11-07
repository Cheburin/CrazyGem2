#include "main.h"

#include "DXUTgui.h"
#include "SDKmisc.h"

extern bool right_mouse_buttom_pressed;

extern GraphicResources * G;

extern SwapChainGraphicResources * SCG;

extern SceneState scene_state;

extern CDXUTTextHelper*                    g_pTxtHelper;

ID3D11ShaderResourceView* null[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

inline void set_scene_constant_buffer(ID3D11DeviceContext* context){
	G->scene_constant_buffer->SetData(context, scene_state);
};
inline void set_ray_constant_buffer(ID3D11DeviceContext* context, SimpleMath::Vector3 s, SimpleMath::Vector3 v){
	RayState state;
	state.S = SimpleMath::Vector4(s.x, s.y, s.z, 1);
	state.V = SimpleMath::Vector4(v.x, v.y, v.z, 0);
	G->ray_constant_buffer->SetData(context, state);
};
//////////////////////////////////////////////////////////////////////////////////////////////////////
struct VectorToArray{
	float items[3];
	VectorToArray(const SimpleMath::Vector3& V){
		items[0] = V.x;
		items[1] = V.y;
		items[2] = V.z;
	}
	float operator [](int index){
		return items[index];
	}
};

SimpleMath::Vector3 GetMatrixRow(const SimpleMath::Matrix& matrix, int i){
	return XMMATRIX(matrix).r[i];
}

SimpleMath::Vector3 normalize(SimpleMath::Vector3 n){
	n.Normalize();
	return n;
}

float sign(const float& arg){
	return  arg < 0.f ? -1.f : 1.f;
}

SimpleMath::Vector3 projectNormal(const SimpleMath::Plane& plane, const SimpleMath::Vector3& normal){
	return normal - plane.DotNormal(normal)*plane.Normal();
}

bool rayIntersectWithBox(const SimpleMath::Vector3& _S, const SimpleMath::Vector3& _V, float& t, SimpleMath::Vector3& _P){
	VectorToArray v(_V);
	VectorToArray s(_S);
	for (int i = 0; i < 3; i++){
		if (abs(v[i]) > 0.0001f){
			float r = v[i] < .0f ? 1 : 0;
			t = (r - s[i]) / v[i];
			if (t > 0.0){
				_P = _S + t*_V;
				VectorToArray p(_P);
				int j = (i + 1) % 3;
				int k = (i + 2) % 3;
				if (0.0f <= p[j] && p[j] <= 1.0f){
					if (0.0f <= p[k] && p[k] <= 1.0f)
						return true;
				}
			}
		}
	}
	return false;
}

bool rayIntersectWithSphera(const SimpleMath::Vector3& _S, const SimpleMath::Vector3& _V, float& t, SimpleMath::Vector3& _P){
	t = _V.Dot(-_S) / _V.Dot(_V);
	_P = _S + t * _V;
	return _P.LengthSquared() <= 0.25f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

void RenderText()
{
	g_pTxtHelper->Begin();
	g_pTxtHelper->SetInsertionPos(2, 0);
	g_pTxtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 0.0f, 1.0f));
	g_pTxtHelper->DrawTextLine(DXUTGetFrameStats(true && DXUTIsVsyncEnabled()));
	g_pTxtHelper->DrawTextLine(DXUTGetDeviceStats());

	g_pTxtHelper->End();
}

void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
	Camera::OnFrameMove(fTime, fElapsedTime, pUserContext);
}

void set_scene_world_matrix(DirectX::XMFLOAT4X4 transformation);
void renderScene(ID3D11Device* pd3dDevice, ID3D11DeviceContext* context);

void clearAndSetRenderTarget(ID3D11DeviceContext* context, float ClearColor[], int n, ID3D11RenderTargetView** pRTV, ID3D11DepthStencilView* pDSV){
	for (int i = 0; i < n; i++)
		context->ClearRenderTargetView(pRTV[i], ClearColor);

	context->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	context->OMSetRenderTargets(n, pRTV, pDSV); //renderTargetViewToArray(pRTV) DXUTGetD3D11RenderTargetView
}

void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* context,
	double fTime, float fElapsedTime, void* pUserContext)
{
	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = scene_state.vFrustumParams.x;
	vp.Height = scene_state.vFrustumParams.y;
	vp.MinDepth = 0;
	vp.MaxDepth = 1;
	context->RSSetViewports(1, &vp);

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	{
		clearAndSetRenderTarget(context, clearColor, 1, renderTargetViewToArray(DXUTGetD3D11RenderTargetView()), DXUTGetD3D11DepthStencilView());

		renderScene(pd3dDevice, context);
	}

	RenderText();
}
///collision
float getEffectiveRadius(const SimpleMath::Vector3& N, const SimpleMath::Matrix& orientation, const SimpleMath::Vector3& size){
	SimpleMath::Vector3 R = XMMATRIX(orientation).r[0];
	SimpleMath::Vector3 S = XMMATRIX(orientation).r[1];
	SimpleMath::Vector3 T = XMMATRIX(orientation).r[2];
	return 0.5f*(abs(N.Dot(size.x*R)) + abs(N.Dot(size.y*S)) + abs(N.Dot(size.z*T)));
}
bool IsObjectNotCollisionWithBiPlane(const SimpleMath::Vector3& normal, const float& d, const SimpleMath::Vector3& q, const float& effectr){
	auto t1 = SimpleMath::Plane(normal, -d).DotCoordinate(q);
	auto t2 = SimpleMath::Plane(-normal, -d).DotCoordinate(q);
	return t1 > (effectr) || t2 > (effectr);
}
bool IsCollision(const SimpleMath::Vector3& A_Origin, const SimpleMath::Vector3& A_Size, const SimpleMath::Matrix& A_Orientation, const SimpleMath::Vector3& B_Origin, const SimpleMath::Vector3& B_Size, const SimpleMath::Matrix& B_Orientation, SimpleMath::Vector3& slide_vector, bool& slide, const SimpleMath::Matrix& Prev_A_Orientation, const SimpleMath::Vector3& Prev_A_Origin){
	for (int i = 0; i < 3; i++){
		SimpleMath::Vector3 A_Normal = GetMatrixRow(A_Orientation, i);
		if (IsObjectNotCollisionWithBiPlane(A_Normal, 0.5f*VectorToArray(A_Size)[i], B_Origin - A_Origin, getEffectiveRadius(A_Normal, B_Orientation, B_Size)))
			return false;
		SimpleMath::Vector3 B_Normal = GetMatrixRow(B_Orientation, i);
		if (IsObjectNotCollisionWithBiPlane(B_Normal, 0.5f*VectorToArray(B_Size)[i], A_Origin - B_Origin, getEffectiveRadius(B_Normal, A_Orientation, A_Size)))
			return false;
	}
	////
	slide = false;
	SimpleMath::Vector3 save_slide_vector = slide_vector;
	int signX[4] = { 1, -1, -1, 1 };
	int signZ[4] = { 1, -1, 1, -1 };
	//1
	for (int i = 0; i < 4; i++){
		SimpleMath::Vector3 edgeS = (A_Origin + 0.5f * A_Size.x * signX[i] * GetMatrixRow(A_Orientation, 0) + 0.5f * A_Size.z * signZ[i] * GetMatrixRow(A_Orientation, 2)) - B_Origin;
		SimpleMath::Vector3 _m = SimpleMath::Vector3::Transform(edgeS, B_Orientation.Transpose()*SimpleMath::Matrix::CreateTranslation(0.5f*B_Size));
		if (!(0.0f <= _m.x&&_m.x <= B_Size.x && 0.0f <= _m.z&&_m.z <= B_Size.z))
			continue;
		SimpleMath::Vector3 _p = SimpleMath::Vector3::Transform((Prev_A_Origin + 0.5f * A_Size.x * signX[i] * GetMatrixRow(Prev_A_Orientation, 0) + 0.5f * A_Size.z * signZ[i] * GetMatrixRow(Prev_A_Orientation, 2)) - B_Origin, B_Orientation.Transpose());
		bool slideZ = abs(_p.x) > (0.5f*B_Size.x - 0.0001);
		bool slideX = abs(_p.z) > (0.5f*B_Size.z - 0.0001);
		{
			slide_vector = save_slide_vector;
			SimpleMath::Vector3 _v = SimpleMath::Vector3::TransformNormal(slideX ? SimpleMath::Vector3(1, 0, 0) : SimpleMath::Vector3(0, 0, 1), B_Orientation);
			_v.Normalize();
			slide_vector = (_v.Dot(slide_vector) < .0 ? -1 : 1)*slide_vector.Length()*_v;
			//slide_vector += SimpleMath::Vector3::TransformNormal(slideX ? SimpleMath::Vector3(0, 0, 0.0001*sign(_p.z)) : SimpleMath::Vector3(0.0001*sign(_p.x), 0, 0), B_Orientation);
		}
		if (true){
			for (int k = 0; k < 2; k++){
				for (int i = 0; i < 3; i++){
					SimpleMath::Vector3 A_Normal = GetMatrixRow(Prev_A_Orientation, i);
					if (IsObjectNotCollisionWithBiPlane(A_Normal, 0.5f*VectorToArray(A_Size)[i], B_Origin - (Prev_A_Origin + slide_vector), getEffectiveRadius(A_Normal, B_Orientation, B_Size)))
					{
						slide = true;
						return true;
					}
					SimpleMath::Vector3 B_Normal = GetMatrixRow(B_Orientation, i);
					if (IsObjectNotCollisionWithBiPlane(B_Normal, 0.5f*VectorToArray(B_Size)[i], Prev_A_Origin + slide_vector - B_Origin, getEffectiveRadius(B_Normal, Prev_A_Orientation, A_Size)))
					{
						slide = true;
						return true;
					}
				}
				slide_vector = save_slide_vector;
				SimpleMath::Vector3 _v = SimpleMath::Vector3::TransformNormal(SimpleMath::Vector3(1, 0, 1), B_Orientation);
				_v.Normalize();
				slide_vector = (_v.Dot(slide_vector) < .0 ? -1 : 1)*slide_vector.Length()*_v;
				slide_vector += SimpleMath::Vector3::TransformNormal(SimpleMath::Vector3(0.1*sign(_p.x), 0, 0.1*sign(_p.z)), B_Orientation);
			}
			//return true;
		}
	}
	//2
	for (int i = 0; i < 4; i++){
		SimpleMath::Vector3 edgeS = (B_Origin + 0.5f * B_Size.x * signX[i] * GetMatrixRow(B_Orientation, 0) + 0.5f * B_Size.z * signZ[i] * GetMatrixRow(B_Orientation, 2)) - A_Origin;
		SimpleMath::Vector3 _m = SimpleMath::Vector3::Transform(edgeS, A_Orientation.Transpose()*SimpleMath::Matrix::CreateTranslation(0.5f*A_Size));
		if (!(0.0f <= _m.x&&_m.x <= A_Size.x && 0.0f <= _m.z&&_m.z <= A_Size.z))
			continue;
		SimpleMath::Vector3 _p = SimpleMath::Vector3::Transform((B_Origin + 0.5f * B_Size.x * signX[i] * GetMatrixRow(B_Orientation, 0) + 0.5f * B_Size.z * signZ[i] * GetMatrixRow(B_Orientation, 2)) - Prev_A_Origin, Prev_A_Orientation.Transpose());
		bool slideZ = abs(_p.x) > (0.5f*A_Size.x - 0.0001);
		bool slideX = abs(_p.z) > (0.5f*A_Size.z - 0.0001);
		{
			slide_vector = save_slide_vector;
			SimpleMath::Vector3 _v = SimpleMath::Vector3::TransformNormal(slideX ? SimpleMath::Vector3(1, 0, 0) : SimpleMath::Vector3(0, 0, 1), Prev_A_Orientation);
			_v.Normalize();
			slide_vector = (_v.Dot(slide_vector) < .0 ? -1 : 1)*slide_vector.Length()*_v;
			//slide_vector += SimpleMath::Vector3::TransformNormal(slideX ? SimpleMath::Vector3(0, 0, -0.0001*sign(_p.z)) : SimpleMath::Vector3(-0.0001*sign(_p.x), 0, 0), Prev_A_Orientation);
		}
		if (true){
			for (int k = 0; k < 2; k++){
				for (int i = 0; i < 3; i++){
					SimpleMath::Vector3 A_Normal = GetMatrixRow(Prev_A_Orientation, i);
					if (IsObjectNotCollisionWithBiPlane(A_Normal, 0.5f*VectorToArray(A_Size)[i], B_Origin - (Prev_A_Origin + slide_vector), getEffectiveRadius(A_Normal, B_Orientation, B_Size)))
					{
						slide = true;
						return true;
					}
					SimpleMath::Vector3 B_Normal = GetMatrixRow(B_Orientation, i);
					if (IsObjectNotCollisionWithBiPlane(B_Normal, 0.5f*VectorToArray(B_Size)[i], Prev_A_Origin + slide_vector - B_Origin, getEffectiveRadius(B_Normal, Prev_A_Orientation, A_Size)))
					{
						slide = true;
						return true;
					}
				}
				slide_vector = save_slide_vector;
				SimpleMath::Vector3 _v = SimpleMath::Vector3::TransformNormal(SimpleMath::Vector3(1, 0, 1), Prev_A_Orientation);
				_v.Normalize();
				slide_vector = (_v.Dot(slide_vector) < .0 ? -1 : 1)*slide_vector.Length()*_v;
				slide_vector += SimpleMath::Vector3::TransformNormal(SimpleMath::Vector3(-0.1*sign(_p.x), 0, -0.1*sign(_p.z)), Prev_A_Orientation);
			}
			//return true;
		}
	}
	////
	return true;
}
bool IsObjectCollisionWithBoundPlane(const SimpleMath::Plane& plane, const SimpleMath::Vector3& q, const SimpleMath::Matrix& intoBox, const SimpleMath::Vector3& size, const float& effectr){
	SimpleMath::Vector3 _q = SimpleMath::Vector3::Transform(q - effectr*plane.Normal(), intoBox);
	if (0.0f <= _q.x&&_q.x <= size.x && 0.0f <= _q.y&&_q.y <= size.y && 0.0f <= _q.z&&_q.z <= size.z)
		return true;
	else
		return false;
}
bool IsCollision(const SimpleMath::Vector3& A_Origin, const SimpleMath::Vector3& A_Size, const SimpleMath::Matrix& A_Orientation, const SimpleMath::Vector3& B_Origin, const float& effectr, SimpleMath::Vector3& slide_vector, bool& slide, const SimpleMath::Matrix& Prev_A_Orientation, const SimpleMath::Vector3& Prev_A_Origin){
	int sign[2] = {1,-1};
	SimpleMath::Vector3 q = B_Origin - A_Origin;
	SimpleMath::Matrix intoBox = A_Orientation.Transpose() * SimpleMath::Matrix::CreateTranslation(0.5f*A_Size);
	for (int i = 0; i < 3; i++){
		for (int j = 0; j<2; j++){
			SimpleMath::Plane plane(sign[j] * GetMatrixRow(A_Orientation, i), -0.5f*VectorToArray(A_Size)[i]);
			if (plane.DotCoordinate(q) > effectr){
				return false;
			}
			else if (plane.DotCoordinate(q) > .0  && IsObjectCollisionWithBoundPlane(plane, q, intoBox, A_Size, effectr)){
				plane = SimpleMath::Plane(sign[j] * GetMatrixRow(Prev_A_Orientation, i), -0.5f*VectorToArray(A_Size)[i]);

				SimpleMath::Vector3 newSlideDir = normalize(SimpleMath::Vector3(-plane.Normal().z, 0, plane.Normal().x));
				slide_vector = (newSlideDir.Dot(slide_vector) < .0 ? -1 : 1)*slide_vector.Length()*newSlideDir;
				slide = true;
				return true;
			}
		}
	}
	SimpleMath::Vector3 edgeV(0, 1, 0);
	int signX[4] = { 1, -1, -1,  1 };
	int signZ[4] = { 1, -1,  1, -1 };
	for (int i = 0; i < 4; i++){
		SimpleMath::Vector3 edgeS = -q +  0.5f * A_Size.x * signX[i] * GetMatrixRow(A_Orientation, 0) + 0.5f * A_Size.z * signZ[i] * GetMatrixRow(A_Orientation, 2);
		float t = edgeV.Dot(-edgeS) / edgeV.Dot(edgeV);
		SimpleMath::Vector3 _P = edgeS + t * edgeV;
		if (_P.LengthSquared() <= effectr*effectr){
			SimpleMath::Vector3 newSlideDir = Prev_A_Origin + 0.5f * A_Size.x * signX[i] * GetMatrixRow(Prev_A_Orientation, 0) + 0.5f * A_Size.z * signZ[i] * GetMatrixRow(Prev_A_Orientation, 2) - B_Origin;
			newSlideDir = normalize(SimpleMath::Vector3(-newSlideDir.z, 0, newSlideDir.x));
			slide_vector = (newSlideDir.Dot(slide_vector) < .0 ? -1 : 1)*slide_vector.Length()*newSlideDir;
			slide = true;
			return true;
		}
	}
	return false;
}
///
SimpleMath::Vector3 wordForward(0,0,1);

SimpleMath::Vector3 actorDir(0,0,1);
SimpleMath::Vector3 actorPos(0,0,-25);
float cameraBoom = 15.f;

SimpleMath::Matrix actorOrientation;
SimpleMath::Matrix actorTranslation;
SimpleMath::Vector3 actorForward;
SimpleMath::Vector3 actorRight;
///

///
SimpleMath::Matrix subActorOrientation;
///

void moveActor(float fElapsedTime, SimpleMath::Vector3 destDir){
	float speedForOrientation = 10.f*fElapsedTime, speeForTranslation = 15.f*fElapsedTime;

	destDir = normalize(destDir);

	SimpleMath::Vector3 _actorDir = normalize(actorDir + ((actorDir.Dot(destDir) == -1) ? SimpleMath::Vector3(0.02f, 0.0f, -0.02f) : SimpleMath::Vector3(0.0f, 0.0f, 0.0f)) + speedForOrientation * destDir);

	SimpleMath::Matrix _actorOrientation = SimpleMath::Matrix::CreateRotationY(-sign(_actorDir.Cross(wordForward).y)*acos(_actorDir.Dot(wordForward)));;

	/// test with collision
	for (int h = 0; h < 2; h++){
		bool slideF, slideB, withSlide;
		bool _collisionF, _collisionB;
		SimpleMath::Vector3 slide_vectorF, slide_vectorB, slide_vector;

		slideB = slideF = withSlide = false;
		slide_vectorF = slide_vectorB = slide_vector = speeForTranslation * destDir;

		_collisionF = IsCollision(actorPos + slide_vector + SimpleMath::Vector3(0, 2.5, 0), SimpleMath::Vector3(5.f, 5.f, 10.f), _actorOrientation, G->boundingVolumes["figure"]._modelCenter, G->boundingVolumes["figure"]._size, SimpleMath::Matrix::Identity, slide_vectorF, slideF, actorOrientation, actorPos);
		_collisionB = IsCollision(actorPos + slide_vector + SimpleMath::Vector3(0, 2.5, 0), SimpleMath::Vector3(5.f, 5.f, 10.f), _actorOrientation, SimpleMath::Vector3(.0f, .0f, 25.0f) + G->boundingVolumes["bench"]._modelCenter, 0.5f * G->boundingVolumes["bench"]._d, slide_vectorB, slideB, actorOrientation, actorPos);

		if (slideF){
			slideB = false;
			slide_vector = slide_vectorB = slide_vectorF;
			withSlide = !IsCollision(actorPos + slide_vectorB + SimpleMath::Vector3(0, 2.5, 0), SimpleMath::Vector3(5.f, 5.f, 10.f), actorOrientation, SimpleMath::Vector3(.0f, .0f, 25.0f) + G->boundingVolumes["bench"]._modelCenter, 0.5f * G->boundingVolumes["bench"]._d, slide_vectorB, slideB, actorOrientation, actorPos);
		}
		else if (slideB){
			slideF = false;
			slide_vector = slide_vectorF = slide_vectorB;
			withSlide = !IsCollision(actorPos + slide_vectorF + SimpleMath::Vector3(0, 2.5, 0), SimpleMath::Vector3(5.f, 5.f, 10.f), actorOrientation, G->boundingVolumes["figure"]._modelCenter, G->boundingVolumes["figure"]._size, SimpleMath::Matrix::Identity, slide_vectorF, slideF, actorOrientation, actorPos);
		}

		if (!_collisionF && !_collisionB)
		{
			actorDir = _actorDir;
			actorOrientation = _actorOrientation;
			actorPos = actorPos + slide_vector;
			actorTranslation = SimpleMath::Matrix::CreateTranslation(actorPos);
			return;
		}
		else if (withSlide){
			actorPos = actorPos + slide_vector;
			actorTranslation = SimpleMath::Matrix::CreateTranslation(actorPos);
			return;
		}

		_actorOrientation = actorOrientation;
	}
}
///
void updateCameraOrientation(SimpleMath::Vector3 cameraDirection, SimpleMath::Vector3& _cameraPos, SimpleMath::Vector3& _cameraTarget){
	auto cameraTarget = actorPos + SimpleMath::Vector3(0.f,4.f,0.f);

	auto cameraPos = cameraTarget - cameraBoom*cameraDirection;

	actorForward = normalize(projectNormal(SimpleMath::Plane(0, 1, 0, 0), cameraTarget - cameraPos));

	actorRight = SimpleMath::Vector3(actorForward.z, 0.0, -actorForward.x);

	_cameraPos = cameraPos;
	_cameraTarget = cameraTarget;
}

void updatePosition(float fElapsedTime, SimpleMath::Vector2 move){
	moveActor(fElapsedTime, move.x*actorForward + move.y*actorRight);
}

void updateOrientation2(SimpleMath::Vector3 direction){
	//subActorOrientation = SimpleMath::Matrix::Identity;

	auto right = SimpleMath::Vector3(0, 1, 0).Cross(direction);
	right.Normalize();
	auto up = direction.Cross(right);

	subActorOrientation = SimpleMath::Matrix(
		SimpleMath::Vector4(right.x, right.y, right.z, 0),
		SimpleMath::Vector4(up.x, up.y, up.z, 0),
		SimpleMath::Vector4(direction.x, direction.y, direction.z, 0),
		SimpleMath::Vector4(0, 0, 0, 1)
	);
}

extern DirectX::XMFLOAT4X4    envViews[6];
extern DirectX::XMFLOAT4X4    envProj;

void _render(ID3D11Device* pd3dDevice, ID3D11DeviceContext* context, bool renderDepth);

void renderScene(ID3D11Device* pd3dDevice, ID3D11DeviceContext* context)
{
	float ClearColor[4] = { 0.0, 0.0, 0.0, 0.0 };

	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.MinDepth = 0;
	vp.MaxDepth = 1;

	auto prev_scene_state = scene_state;
	
	{
		vp.Height = vp.Width = 2048;
		context->RSSetViewports(1, &vp);

		scene_state.mProjection = SimpleMath::Matrix(envProj).Transpose();

		for (int j = 0; j < 6; ++j){
			context->ClearRenderTargetView(G->envColor_RTVs[j].Get(), ClearColor);
			context->ClearDepthStencilView(G->envDepth_DSVs[j].Get(), D3D10_CLEAR_DEPTH, 1.0, 0);
			context->OMSetRenderTargets(1, renderTargetViewToArray(G->envColor_RTVs[j].Get()), G->envDepth_DSVs[j].Get());

			auto view = (SimpleMath::Matrix::CreateTranslation(0, 10, 20).Invert() * envViews[j]);
			scene_state.mView = view.Transpose();
			scene_state.mInvView = (view.Invert()).Transpose();

			_render(pd3dDevice, context, true);
		}

		//just for test
		//if (right_mouse_buttom_pressed){
		//	D3DX11SaveTextureToFile(context, G->envColor_T2D.Get(), D3DX11_IFF_DDS, L"EnvCubeMap_Color.dds");
		//	D3DX11SaveTextureToFile(context, G->envDepth_T2D.Get(), D3DX11_IFF_DDS, L"EnvCubeMap_Depth.dds");
		//}

		vp.Width = scene_state.vFrustumParams.x;
		vp.Height = scene_state.vFrustumParams.y;
		context->RSSetViewports(1, &vp);
	}

	scene_state = prev_scene_state;
	
	context->ClearRenderTargetView(DXUTGetD3D11RenderTargetView(), ClearColor);
	context->ClearDepthStencilView(DXUTGetD3D11DepthStencilView(), D3D10_CLEAR_DEPTH, 1.0, 0);
	context->OMSetRenderTargets(1, renderTargetViewToArray(DXUTGetD3D11RenderTargetView()), DXUTGetD3D11DepthStencilView());

	_render(pd3dDevice, context, false);
}

void _render(ID3D11Device* pd3dDevice, ID3D11DeviceContext* context, bool renderDepth)
{
	///// Options
	scene_state.renderDepthPass.x = renderDepth ? 1.0f : 0.0f;
	/////

	///// collision shapes
	SimpleMath::Matrix boxTransform = SimpleMath::Matrix::CreateScale(G->boundingVolumes["figure"]._size)*SimpleMath::Matrix::CreateTranslation(G->boundingVolumes["figure"]._min);

	SimpleMath::Matrix sphereTransform = SimpleMath::Matrix::CreateScale(G->boundingVolumes["bench"]._d)*SimpleMath::Matrix::CreateTranslation(G->boundingVolumes["bench"]._modelCenter);
	/////

	///// main constant buffer and samplers
	context->GSSetConstantBuffers(0, 2, constantBuffersToArray(*(G->scene_constant_buffer), *(G->ray_constant_buffer)));

	context->VSSetConstantBuffers(0, 2, constantBuffersToArray(*(G->scene_constant_buffer), *(G->ray_constant_buffer)));
	
	context->PSSetSamplers(0, 1, samplerStateToArray(G->render_states->AnisotropicWrap()));
	/////

	context->OMSetBlendState(G->render_states->Opaque(), Colors::Black, 0xFFFFFFFF);
	context->OMSetDepthStencilState(G->render_states->DepthDefault(), 0);

	set_scene_world_matrix(SimpleMath::Matrix::CreateTranslation(-0.5, 0, -0.5)*SimpleMath::Matrix::CreateScale(5000, 5000, 5000));
	set_scene_constant_buffer(context);
	G->ground_model->Draw(context, G->ground_effect.get(), G->model_input_layout.Get(), [=]{
		context->RSSetState(G->render_states->CullCounterClockwise());
	});

	SimpleMath::Matrix body;
	set_scene_world_matrix(body = SimpleMath::Matrix::CreateTranslation(-0.5, 0.0, -0.5)*SimpleMath::Matrix::CreateScale(5, 5, 10)*actorOrientation*actorTranslation);
	set_scene_constant_buffer(context);
	Draw_Single_Point(context, G->box_effect.get(), [=]{
		context->RSSetState(G->render_states->Wireframe());
	});

	/////нарисуем дуло
	auto subBodyOrigin = SimpleMath::Matrix::CreateScale(1/5.0, 1/5.0, 1/10.0)*SimpleMath::Matrix::CreateTranslation(0.5, 1, 0.5)*body;
	set_scene_world_matrix(SimpleMath::Matrix::CreateTranslation(-0.5, 0, 0)*SimpleMath::Matrix::CreateScale(1, 1, 10)*subActorOrientation*subBodyOrigin);
	set_scene_constant_buffer(context);
	Draw_Single_Point(context, G->box_effect.get(), [=]{
		context->RSSetState(G->render_states->Wireframe());
	});
	/////

	////нарисуем луч из дула
	if (right_mouse_buttom_pressed){
		SimpleMath::Matrix rayTransform = SimpleMath::Matrix::CreateTranslation(0, 0.5, 0)*subActorOrientation*subBodyOrigin;
		set_scene_world_matrix(rayTransform);
		set_ray_constant_buffer(context, SimpleMath::Vector3(0, 0, 0), SimpleMath::Vector3(0, 0, 10));
		set_scene_constant_buffer(context);
		Draw_Single_Point(context, G->ray_effect.get(), [=]{
			context->RSSetState(G->render_states->Wireframe());
		});

		//найдем пересечение луча с ограничивающими объемами
		{
			SimpleMath::Vector3 s = SimpleMath::Vector3::Transform(SimpleMath::Vector3(0, 0, 0), rayTransform * boxTransform.Invert());
			SimpleMath::Vector3 v = SimpleMath::Vector3::TransformNormal(SimpleMath::Vector3(0, 0, 10000), rayTransform * boxTransform.Invert());
			float t;
			SimpleMath::Vector3 p;
			if (rayIntersectWithBox(s, v, t, p)){
				SimpleMath::Vector3 target = SimpleMath::Vector3::Transform(p, boxTransform);
				set_scene_world_matrix(SimpleMath::Matrix::CreateScale(0.5, 0.5, 0.5)*SimpleMath::Matrix::CreateTranslation(target));
				set_scene_constant_buffer(context);
				G->sphere_model->Draw(G->sphere_hit_effect.get(), G->model_input_layout.Get(), false, false, [=]{
					context->RSSetState(G->render_states->Wireframe());
				});
			}
		}
		{
			SimpleMath::Vector3 s = SimpleMath::Vector3::Transform(SimpleMath::Vector3(0, 0, 0), rayTransform * SimpleMath::Matrix::CreateTranslation(.0f, .0f, -25.0f) * sphereTransform.Invert());
			SimpleMath::Vector3 v = SimpleMath::Vector3::TransformNormal(SimpleMath::Vector3(0, 0, 10000), rayTransform * SimpleMath::Matrix::CreateTranslation(.0f, .0f, -25.0f) * sphereTransform.Invert());
			float t;
			SimpleMath::Vector3 p;
			if (rayIntersectWithSphera(s, v, t, p)){
				SimpleMath::Vector3 target = SimpleMath::Vector3::Transform(p, sphereTransform * SimpleMath::Matrix::CreateTranslation(.0f, .0f, 25.0f));
				set_scene_world_matrix(SimpleMath::Matrix::CreateScale(0.5, 0.5, 0.5)*SimpleMath::Matrix::CreateTranslation(target));
				set_scene_constant_buffer(context);
				G->sphere_model->Draw(G->sphere_hit_effect.get(), G->model_input_layout.Get(), false, false, [=]{
					context->RSSetState(G->render_states->Wireframe());
				});
			}
		}
	}

	set_scene_world_matrix_into_camera_origin();
	set_scene_constant_buffer(context);
	G->sphere_model->Draw(G->sky_effect.get(), G->model_input_layout.Get(), false, false, [=]{
		context->PSSetShaderResources(2, 1, shaderResourceViewToArray(G->sky_cube_texture.Get()));

		context->RSSetState(G->render_states->CullClockwise());
	});

	set_scene_world_matrix(SimpleMath::Matrix::Identity);
	set_scene_constant_buffer(context);
	G->figure_model->Draw(context, G->figure_effect.get(), G->model_input_layout.Get(), [=]{
		context->PSSetShaderResources(1, 1, shaderResourceViewToArray(G->figure_normal_texture.Get()));
		if(!renderDepth)context->PSSetShaderResources(2, 1, shaderResourceViewToArray(G->envDepth_SRV.Get()));

		context->RSSetState(G->render_states->CullCounterClockwise());
	});

	set_scene_world_matrix(SimpleMath::Matrix::CreateTranslation(.0f, .0f, 25.0f));
	set_scene_constant_buffer(context);
	G->bench_model->Draw(context, G->bench_effect.get(), G->model_input_layout.Get(), [=]{
		if (!renderDepth)context->PSSetShaderResources(2, 1, shaderResourceViewToArray(G->envDepth_SRV.Get()));

		context->RSSetState(G->render_states->CullCounterClockwise());
	});

	//G->bench_model->primitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	//G->bench_model->Draw(context, G->tbn_effect.get(), G->model_input_layout.Get(), [=]{
	//	context->RSSetState(G->render_states->Wireframe());
	//});
	//G->bench_model->primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	///boundingVolumes
	if (!renderDepth){
		set_scene_world_matrix(boxTransform);
		set_scene_constant_buffer(context);
		Draw_Single_Point(context, G->box_effect.get(), [=]{
			context->OMSetBlendState(G->alfaBlend.Get(), Colors::Black, 0xFFFFFFFF);
			context->RSSetState(G->render_states->CullClockwise());
		});
		Draw_Single_Point(context, G->box_effect.get(), [=]{
			context->OMSetBlendState(G->alfaBlend.Get(), Colors::Black, 0xFFFFFFFF);
			context->RSSetState(G->render_states->CullCounterClockwise());
		});

		set_scene_world_matrix(sphereTransform * SimpleMath::Matrix::CreateTranslation(.0f, .0f, 25.0f));
		set_scene_constant_buffer(context);
		G->sphere_model->Draw(G->sphere_effect.get(), G->model_input_layout.Get(), false, false, [=]{
			context->OMSetBlendState(G->alfaBlend.Get(), Colors::Black, 0xFFFFFFFF);
			context->RSSetState(G->render_states->CullClockwise());
		});
		G->sphere_model->Draw(G->sphere_effect.get(), G->model_input_layout.Get(), false, false, [=]{
			context->OMSetBlendState(G->alfaBlend.Get(), Colors::Black, 0xFFFFFFFF);
			context->RSSetState(G->render_states->CullCounterClockwise());
		});
	}

	context->PSSetShaderResources(1, 2, shaderResourceViewToArray(0, 0));
}