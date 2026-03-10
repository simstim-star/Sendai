#pragma once

#include "pch.h"
#include "DirectXMathC.h"

typedef struct R_Camera {
	XMFLOAT3 InitialPosition;
	XMFLOAT3 Position;
	float Yaw;	 // Relative to the +z axis.
	float Pitch; // Relative to the xz plane.
	XMFLOAT3 LookDirection;
	XMFLOAT3 UpDirection;
	float MoveSpeed; // Speed at which the camera moves, in units per second.
	float TurnSpeed; // Speed at which the camera turns, in radians per second.

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

R_Camera R_CameraSpawn(XMFLOAT3 position);
void R_CameraUpdate(R_Camera *camera, float elapsed_seconds);
void R_CameraOnKeyDown(R_Camera *camera, WPARAM key);
void R_CameraOnKeyUp(R_Camera *camera, WPARAM key);

XMMATRIX R_CameraViewMatrix(XMFLOAT3 pos, XMFLOAT3 look, XMFLOAT3 up);
XMMATRIX R_CameraProjectionMatrix(float fov, float aspect_ratio, float near_plane, float far_plane);
