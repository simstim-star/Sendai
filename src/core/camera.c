#include "pch.h"
#include "camera.h"

/*****************************************************************
 Private functions
******************************************************************/

static void
CameraReset(R_Camera *camera)
{
	camera->Position = camera->InitialPosition;
	camera->Yaw = XM_PI;
	camera->Pitch = 0.0f;
	camera->LookDirection = (XMFLOAT3){0, 0, -1};
}

/*****************************************************************
 Public functions
******************************************************************/

R_Camera
R_CameraSpawn(XMFLOAT3 Position)
{
	return (R_Camera){
	  .InitialPosition = Position,
	  .Position = Position,
	  .Yaw = XM_PI,
	  .Pitch = 0.0f,
	  .LookDirection = {0, 0, -1},
	  .UpDirection = {0, 1, 0},
	  .MoveSpeed = {5.0f},
	  .TurnSpeed = XM_PIDIV4,
	};
}

void
R_CameraUpdate(R_Camera *Camera, float ElapsedSeconds)
{
	float RotateDelta = Camera->TurnSpeed * ElapsedSeconds;

	if (Camera->KeysPressed.LeftArrow) {
		Camera->Yaw -= RotateDelta;
	}
	if (Camera->KeysPressed.RightArrow) {
		Camera->Yaw += RotateDelta;
	}
	if (Camera->KeysPressed.UpArrow) {
		Camera->Pitch += RotateDelta;
	}
	if (Camera->KeysPressed.DownArrow) {
		Camera->Pitch -= RotateDelta;
	}

	Camera->Pitch = min(Camera->Pitch, XM_PIDIV4);
	Camera->Pitch = max(-XM_PIDIV4, Camera->Pitch);

	/*
		Determine the look direction (check notes/xyz_from_yaw_pitch.png)
		 x = cos(pitch) * sin(yaw)
		 y = sin(pitch)
		 z = cos(pitch) * cos(yaw)
	*/
	float look_b = cosf(Camera->Pitch);
	Camera->LookDirection.x = look_b * sinf(Camera->Yaw);
	Camera->LookDirection.y = sinf(Camera->Pitch);
	Camera->LookDirection.z = look_b * cosf(Camera->Yaw);

	XMVECTOR Forward = XMLoadFloat3(&Camera->LookDirection);
	XMVECTOR Up = XMLoadFloat3(&Camera->UpDirection);
	XMVECTOR Right = XM_VEC3_CROSS(Forward, Up);

	XMVECTOR Movement = XMVectorZero();

	// Process input, which can be [-1,0,1] and will be applied to the direction we want to move
	{
		float RightInput = 0.f;
		if (Camera->KeysPressed.A) {
			RightInput += 1.0f;
		}
		if (Camera->KeysPressed.D) {
			RightInput -= 1.0f;
		}

		float ForwardInput = 0.f;
		if (Camera->KeysPressed.W) {
			ForwardInput += 1.0f;
		}
		if (Camera->KeysPressed.S) {
			ForwardInput -= 1.0f;
		}

		XMVECTOR RightScaledByInput = XM_VEC_SCALE(Right, RightInput);
		XMVECTOR ForwardScaledByInput = XM_VEC_SCALE(Forward, ForwardInput);
		Movement = XM_VEC_ADD(Movement, RightScaledByInput);
		Movement = XM_VEC_ADD(Movement, ForwardScaledByInput);
	}

	XMVECTOR Zero = XMVectorZero();
	if (!XM_VEC3_EQ(Movement, Zero)) {
		Movement = XM_VEC3_NORM(Movement);
		Movement = XM_VEC_SCALE(Movement, Camera->MoveSpeed * ElapsedSeconds);

		XMFLOAT3 Delta;
		XM_STORE_FLOAT3(&Delta, Movement);

		Camera->Position.x += Delta.x;
		Camera->Position.y += Delta.y;
		Camera->Position.z += Delta.z;
	}
}

XMMATRIX
R_CameraViewMatrix(XMFLOAT3 pos, XMFLOAT3 look, XMFLOAT3 up)
{
	XMVECTOR EyePosition = XMLoadFloat3(&pos);
	XMVECTOR EyeDirection = XMLoadFloat3(&look);
	XMVECTOR UpDirection = XMLoadFloat3(&up);
	return XM_MAT_LOOK_LH(EyePosition, EyeDirection, UpDirection);
}

XMMATRIX
R_CameraProjectionMatrix(float FOV, float AspectRatio, float NearPlane, float FarPlane)
{
	return XMMatrixPerspectiveFovLH(FOV, AspectRatio, NearPlane, FarPlane);
}

void
R_CameraOnKeyDown(R_Camera *Camera, WPARAM Key)
{
	switch (Key) {
	case 'W':
		Camera->KeysPressed.W = TRUE;
		break;
	case 'A':
		Camera->KeysPressed.A = TRUE;
		break;
	case 'S':
		Camera->KeysPressed.S = TRUE;
		break;
	case 'D':
		Camera->KeysPressed.D = TRUE;
		break;
	case VK_LEFT:
		Camera->KeysPressed.LeftArrow = TRUE;
		break;
	case VK_RIGHT:
		Camera->KeysPressed.RightArrow = TRUE;
		break;
	case VK_UP:
		Camera->KeysPressed.UpArrow = TRUE;
		break;
	case VK_DOWN:
		Camera->KeysPressed.DownArrow = TRUE;
		break;
	case VK_ESCAPE:
		CameraReset(Camera);
		break;
	}
}

void
R_CameraOnKeyUp(R_Camera *Camera, WPARAM Key)
{
	switch (Key) {
	case 'W':
		Camera->KeysPressed.W = FALSE;
		break;
	case 'A':
		Camera->KeysPressed.A = FALSE;
		break;
	case 'S':
		Camera->KeysPressed.S = FALSE;
		break;
	case 'D':
		Camera->KeysPressed.D = FALSE;
		break;
	case VK_LEFT:
		Camera->KeysPressed.LeftArrow = FALSE;
		break;
	case VK_RIGHT:
		Camera->KeysPressed.RightArrow = FALSE;
		break;
	case VK_UP:
		Camera->KeysPressed.UpArrow = FALSE;
		break;
	case VK_DOWN:
		Camera->KeysPressed.DownArrow = FALSE;
		break;
	}
}