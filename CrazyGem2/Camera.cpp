#include "main.h"

bool right_mouse_buttom_pressed = false;

float m_pitch = 0;
float m_yaw = XM_PI / 2.0f;// -XM_PI / 2.0f;

float m_pitch2 = 0;
float m_yaw2 = XM_PI / 2.0f;// -XM_PI / 2.0f;

const float ROTATION_GAIN = 0.004f;
const float MOVEMENT_GAIN = 0.02f;

DirectX::XMFLOAT3 m_cameraPos = XMFLOAT3(0, 10, -25.0/1.0);

extern std::unique_ptr<Keyboard> _keyboard;
extern std::unique_ptr<Mouse> _mouse;

extern SceneState scene_state;

void updateCameraOrientation(SimpleMath::Vector3 cameraDirection, SimpleMath::Vector3& _cameraPos, SimpleMath::Vector3& _cameraTarget);
void updatePosition(float fElapsedTime, SimpleMath::Vector2 move);
void updateOrientation2(SimpleMath::Vector3 direction);

namespace Camera{
	void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
	{
		// Update the camera's position based on user input 
		auto mouse = _mouse->GetState();

		right_mouse_buttom_pressed = mouse.rightButton;

		if (mouse.leftButton && mouse.positionMode == Mouse::MODE_RELATIVE)
		{
			SimpleMath::Vector3 delta = SimpleMath::Vector3(float(mouse.x), float(mouse.y), 0.f)
				* ROTATION_GAIN;

			m_pitch -= delta.y;
			m_yaw -= delta.x;

			// limit pitch to straight up or straight down
			// with a little fudge-factor to avoid gimbal lock
			float limit = XM_PI / 2.0f - 0.01f;
			m_pitch = max(-limit, m_pitch);
			m_pitch = min(+limit, m_pitch);

			// keep longitude in sane range by wrapping
			if (m_yaw > XM_PI)
			{
				m_yaw -= XM_PI * 2.0f;
			}
			else if (m_yaw < -XM_PI)
			{
				m_yaw += XM_PI * 2.0f;
			}
		}
		if (mouse.rightButton && mouse.positionMode == Mouse::MODE_RELATIVE)
		{
			SimpleMath::Vector3 delta = SimpleMath::Vector3(float(mouse.x), float(mouse.y), 0.f)
				* ROTATION_GAIN;

			m_pitch2 -= delta.y;
			m_yaw2 -= delta.x;

			// limit pitch to straight up or straight down
			// with a little fudge-factor to avoid gimbal lock
			float limit = XM_PI / 2.0f - 0.01f;
			m_pitch2 = max(-limit, m_pitch2);
			m_pitch2 = min(+limit, m_pitch2);

			// keep longitude in sane range by wrapping
			if (m_yaw2 > XM_PI)
			{
				m_yaw2 -= XM_PI * 2.0f;
			}
			else if (m_yaw2 < -XM_PI)
			{
				m_yaw2 += XM_PI * 2.0f;
			}
		}
		XMMATRIX view;
		XMMATRIX invView;
		_mouse->SetMode(mouse.leftButton || mouse.rightButton ? Mouse::MODE_RELATIVE : Mouse::MODE_ABSOLUTE);
		{
			float y = sinf(m_pitch);
			float r = cosf(m_pitch);
			float x = r*cosf(m_yaw);
			float z = r*sinf(m_yaw);
			
			SimpleMath::Vector3 _cameraPos, _cameraTarget, _up = SimpleMath::Vector3(0,1,0);
			updateCameraOrientation(XMFLOAT3(x, y, z), _cameraPos, _cameraTarget);

			view = XMMatrixLookAtLH(_cameraPos, _cameraTarget, _up);

			invView = DirectX::XMMatrixInverse(0, view);

			DirectX::XMStoreFloat4x4(&scene_state.mView, DirectX::XMMatrixTranspose(view));
			DirectX::XMStoreFloat4x4(&scene_state.mInvView, DirectX::XMMatrixTranspose(invView));
		}
		{
			float y = sinf(m_pitch2);
			float r = cosf(m_pitch2);
			float x = r*cosf(m_yaw2);
			float z = r*sinf(m_yaw2);

			updateOrientation2(XMFLOAT3(x, y, z));
		}
		auto kb = _keyboard->GetState();
		{
			SimpleMath::Vector2 move = SimpleMath::Vector3::Zero;

			if (kb.Up || kb.W)
				move.x += 1.f;

			if (kb.Down || kb.S)
				move.x -= 1.f;

			if (kb.Left || kb.A)
				move.y -= 1.f;

			if (kb.Right || kb.D)
				move.y += 1.f;

			move *= MOVEMENT_GAIN;

			updatePosition(fElapsedTime, move);
		}
	}
}