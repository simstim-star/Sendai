#pragma once

#include "DirectXMathC.h"

typedef struct R_Camera {
	XMFLOAT3 InitialPosition;
	XMFLOAT3 Position;
	float Yaw; 
	float Pitch;
	XMFLOAT3 LookDirection;
	XMFLOAT3 UpDirection;
	float MoveSpeed; // units per second
	float TurnSpeed; // radians per second

	struct {
		bool W;
		bool A;
		bool S;
		bool D;

		bool LeftArrow;
		bool RightArrow;
		bool UpArrow;
		bool DownArrow;
	} KeysPressed;
} R_Camera;

R_Camera R_CameraSpawn(XMFLOAT3 Position);
void R_CameraUpdate(R_Camera *Camera, float ElapsedSeconds);
void R_CameraOnKeyDown(R_Camera *Camera, WPARAM Key);
void R_CameraOnKeyUp(R_Camera *Camera, WPARAM Key);

XMMATRIX R_CameraViewMatrix(XMFLOAT3 Pos, XMFLOAT3 Look, XMFLOAT3 Up);
XMMATRIX R_CameraProjectionMatrix(float Fov, float AspectRatio, float NearPlane, float FarPlane);
