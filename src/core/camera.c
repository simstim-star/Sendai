#include "camera.h"
#include <math.h>
#include <stdlib.h>

/*****************************************************************
 Private functions
******************************************************************/

static void camera_reset(Sendai_Camera *camera)
{
	camera->position = camera->initial_position;
	camera->yaw = XM_PI;
	camera->pitch = 0.0f;
	camera->look_direction = (XMFLOAT3){0, 0, -1};
}

/*****************************************************************
 Public functions
******************************************************************/

Sendai_Camera Sendai_camera_spawn(XMFLOAT3 position)
{
	return (Sendai_Camera){
	  .initial_position = position,
	  .position = position,
	  .yaw = XM_PI,
	  .pitch = 0.0f,
	  .look_direction = {0, 0, -1},
	  .up_direction = {0, 1, 0},
	  .move_speed = {5.0f},
	  .turn_speed = XM_PIDIV4,
	};
}

void Sendai_camera_update(Sendai_Camera *camera, float elapsed_seconds)
{
	float rotate_delta = camera->turn_speed * elapsed_seconds;

	if (camera->keys_pressed.left) {
		camera->yaw += rotate_delta;
	}
	if (camera->keys_pressed.right) {
		camera->yaw -= rotate_delta;
	}
	if (camera->keys_pressed.up) {
		camera->pitch += rotate_delta;
	}
	if (camera->keys_pressed.down) {
		camera->pitch -= rotate_delta;
	}

	camera->pitch = min(camera->pitch, XM_PIDIV4);
	camera->pitch = max(-XM_PIDIV4, camera->pitch);

	/* 
		Determine the look direction (check notes/xyz_from_yaw_pitch.png)
		 x = cos(pitch) * sin(yaw)
         y = sin(pitch)
         z = cos(pitch) * cos(yaw)
	*/
	float look_b = cosf(camera->pitch);
	camera->look_direction.x = look_b * sinf(camera->yaw);
	camera->look_direction.y = sinf(camera->pitch);
	camera->look_direction.z = look_b * cosf(camera->yaw);

	XMVECTOR forward = XMLoadFloat3(&camera->look_direction);
	XMVECTOR up = XMLoadFloat3(&camera->up_direction);
	XMVECTOR right = XM_VEC3_CROSS(forward, up);


	XMVECTOR movement = XMVectorZero();

	// Process input, which can be [-1,0,1] and will be applied to the direction we want to move
	{
		float right_input = 0.f;
		if (camera->keys_pressed.a) {
			right_input -= 1.0f;
		}
		if (camera->keys_pressed.d) {
			right_input += 1.0f;
		}

		float forward_input = 0.f;
		if (camera->keys_pressed.w) {
			forward_input += 1.0f;
		}
		if (camera->keys_pressed.s) {
			forward_input -= 1.0f;
		}

		XMVECTOR right_scaled_by_input = XM_VEC_SCALE(right, right_input);
		XMVECTOR forward_scaled_by_input = XM_VEC_SCALE(forward, forward_input);
		movement = XM_VEC_ADD(movement, right_scaled_by_input);
		movement = XM_VEC_ADD(movement, forward_scaled_by_input);
	}

	XMVECTOR zero = XMVectorZero();
	if (!XM_VEC3_EQ(movement, zero)) {
		movement = XM_VEC3_NORM(movement);
		movement = XM_VEC_SCALE(movement, camera->move_speed * elapsed_seconds);

		XMFLOAT3 delta;
		XMStoreFloat3(&delta, &movement);

		camera->position.x += delta.x;
		camera->position.y += delta.y;
		camera->position.z += delta.z;
	}
}

XMMATRIX Sendai_camera_view_matrix(XMFLOAT3 pos, XMFLOAT3 look, XMFLOAT3 up)
{
	XMVECTOR eye_position = XMLoadFloat3(&pos);
	XMVECTOR eye_direction = XMLoadFloat3(&look);
	XMVECTOR up_direction = XMLoadFloat3(&up);
	return XM_MAT_LOOK_RH(eye_position, eye_direction, up_direction);
}

XMMATRIX Sendai_camera_projection_matrix(float fov, float aspect_ratio, float near_plane, float far_plane)
{
	return XMMatrixPerspectiveFovRH(fov, aspect_ratio, near_plane, far_plane);
}

void Sendai_camera_on_key_down(Sendai_Camera *camera, WPARAM key)
{
	switch (key) {
	case 'W':
		camera->keys_pressed.w = true;
		break;
	case 'A':
		camera->keys_pressed.a = true;
		break;
	case 'S':
		camera->keys_pressed.s = true;
		break;
	case 'D':
		camera->keys_pressed.d = true;
		break;
	case VK_LEFT:
		camera->keys_pressed.left = true;
		break;
	case VK_RIGHT:
		camera->keys_pressed.right = true;
		break;
	case VK_UP:
		camera->keys_pressed.up = true;
		break;
	case VK_DOWN:
		camera->keys_pressed.down = true;
		break;
	case VK_ESCAPE:
		camera_reset(camera);
		break;
	}
}

void Sendai_camera_on_key_up(Sendai_Camera *camera, WPARAM key)
{
	switch (key) {
	case 'W':
		camera->keys_pressed.w = false;
		break;
	case 'A':
		camera->keys_pressed.a = false;
		break;
	case 'S':
		camera->keys_pressed.s = false;
		break;
	case 'D':
		camera->keys_pressed.d = false;
		break;
	case VK_LEFT:
		camera->keys_pressed.left = false;
		break;
	case VK_RIGHT:
		camera->keys_pressed.right = false;
		break;
	case VK_UP:
		camera->keys_pressed.up = false;
		break;
	case VK_DOWN:
		camera->keys_pressed.down = false;
		break;
	}
}