#pragma once

#include "DirectXMathC.h"
#include <windows.h>

typedef struct snc_camera_t {
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
} snc_camera_t;

snc_camera_t snc_camera_spawn(XMFLOAT3 position);
void snc_camera_update(snc_camera_t *camera, float elapsedSeconds);
void snc_camera_on_key_down(snc_camera_t *camera, WPARAM key);
void snc_camera_on_key_up(snc_camera_t *camera, WPARAM key);

XMMATRIX snc_camera_view_matrix(XMFLOAT3 pos, XMFLOAT3 look_direction, XMFLOAT3 up_direction);
XMMATRIX snc_camera_projection_matrix(float fov, float aspect_ratio, float near_plane, float far_plane);
