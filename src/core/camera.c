#include <math.h>
#include <stdlib.h>
#include "camera.h"

/*****************************************************************
 Private functions
******************************************************************/

static void camera_reset(Sendai_Camera *camera) {
	camera->position = camera->initial_position;
	camera->yaw = XM_PI;
	camera->pitch = 0.0f;
	camera->look_direction = (XMFLOAT3){0, 0, -1};
}

/*****************************************************************
 Public functions
******************************************************************/

Sendai_Camera Sendai_camera_spawn(XMFLOAT3 position) {
	return (Sendai_Camera){
		.initial_position = position,
		.position = position,
		.yaw = XM_PI,
		.pitch = 0.0f,
		.look_direction = {0, 0, -1},
		.up_direction = {0, 1, 0},
		.move_speed = {20.0f},
		.turn_speed = XM_PIDIV2,
	};
}

void Sendai_camera_update(Sendai_Camera *camera, float elapsedSeconds) {
	// Calculate the move vector in camera space.
	XMFLOAT3 move = (XMFLOAT3){0, 0, 0};

	if (camera->keys_pressed.a) move.x -= 1.0f;
	if (camera->keys_pressed.d) move.x += 1.0f;
	if (camera->keys_pressed.w) move.z -= 1.0f;
	if (camera->keys_pressed.s) move.z += 1.0f;

	// Avoid normalizing zero vector (which can't be normalized)
	if (fabs(move.x) > 0.1f && fabs(move.z) > 0.1f) {
		XMVECTOR moveAsXMVECTOR = XMLoadFloat3(&move);
		XMVECTOR vector = XM_VEC3_NORM(moveAsXMVECTOR);
		move.x = XM_VECX(vector);
		move.z = XM_VECZ(vector);
	}

	float moveInterval = camera->move_speed * elapsedSeconds;
	float rotateInterval = camera->turn_speed * elapsedSeconds;

	if (camera->keys_pressed.left) camera->yaw += rotateInterval;
	if (camera->keys_pressed.right) camera->yaw -= rotateInterval;
	if (camera->keys_pressed.up) camera->pitch += rotateInterval;
	if (camera->keys_pressed.down) camera->pitch -= rotateInterval;

	// Prevent looking too far up or down.
	camera->pitch = min(camera->pitch, XM_PIDIV4);
	camera->pitch = max(-XM_PIDIV4, camera->pitch);

	// Move the camera in model space.
	float x = move.x * -cosf(camera->yaw) - move.z * sinf(camera->yaw);
	float z = move.x * sinf(camera->yaw) - move.z * cosf(camera->yaw);
	camera->position.x += x * moveInterval;
	camera->position.z += z * moveInterval;

	// Determine the look direction.
	float r = cosf(camera->pitch);
	camera->look_direction.x = r * sinf(camera->yaw);
	camera->look_direction.y = sinf(camera->pitch);
	camera->look_direction.z = r * cosf(camera->yaw);
}

XMMATRIX Sendai_camera_view_matrix(XMFLOAT3 pos, XMFLOAT3 lookDirection, XMFLOAT3 upDirection) {
	XMVECTOR EyePosition = XMLoadFloat3(&pos);
	XMVECTOR EyeDirection = XMLoadFloat3(&lookDirection);
	XMVECTOR UpDirection = XMLoadFloat3(&upDirection);
	return XM_MAT_LOOK_RH(EyePosition, EyeDirection, UpDirection);
}

XMMATRIX Sendai_camera_projection_matrix(float fov, float aspectRatio, float nearPlane, float farPlane) {
	return XMMatrixPerspectiveFovRH(fov, aspectRatio, nearPlane, farPlane);
}

void Sendai_camera_on_key_down(Sendai_Camera *camera, WPARAM key) {
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

void Sendai_camera_on_key_up(Sendai_Camera *camera, WPARAM key) {
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