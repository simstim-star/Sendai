#pragma once

#include "DirectXMathC.h"
#include <windows.h>

typedef struct Sendai_Camera {
	XMFLOAT3 initial_position;
	XMFLOAT3 position;
	float yaw;	 // Relative to the +z axis.
	float pitch; // Relative to the xz plane.
	XMFLOAT3 look_direction;
	XMFLOAT3 up_direction;
	float move_speed; // Speed at which the camera moves, in units per second.
	float turn_speed; // Speed at which the camera turns, in radians per second.

	struct {
		bool w;
		bool a;
		bool s;
		bool d;

		bool left;
		bool right;
		bool up;
		bool down;
	} keys_pressed;
} Sendai_Camera;

Sendai_Camera Sendai_camera_spawn(XMFLOAT3 position);
void Sendai_camera_update(Sendai_Camera *camera, float elapsed_seconds);
void Sendai_camera_on_key_down(Sendai_Camera *camera, WPARAM key);
void Sendai_camera_on_key_up(Sendai_Camera *camera, WPARAM key);

XMMATRIX Sendai_camera_view_matrix(XMFLOAT3 pos, XMFLOAT3 look, XMFLOAT3 up);
XMMATRIX Sendai_camera_projection_matrix(float fov, float aspect_ratio, float near_plane, float far_plane);
