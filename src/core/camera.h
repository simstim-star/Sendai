#pragma once

#include "DirectXMathC.h"

typedef struct R_Camera {
	XMFLOAT3 InitialPosition;
	XMFLOAT3 Position;
	FLOAT Yaw; 
	FLOAT Pitch;
	XMFLOAT3 LookDirection;
	XMFLOAT3 UpDirection;
	FLOAT MoveSpeed; // units per second
	FLOAT TurnSpeed; // radians per second
	FLOAT SpeedEnhance;

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
VOID R_CameraUpdate(R_Camera *Camera, FLOAT ElapsedSeconds);
VOID R_CameraOnKeyDown(R_Camera *Camera, WPARAM Key);
VOID R_CameraOnKeyUp(R_Camera *Camera, WPARAM Key);

XMMATRIX R_CameraViewMatrix(XMFLOAT3 Pos, XMFLOAT3 Look, XMFLOAT3 Up);
XMMATRIX R_CameraProjectionMatrix(FLOAT Fov, FLOAT AspectRatio, FLOAT NearPlane, FLOAT FarPlane);
