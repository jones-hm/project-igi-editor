#version 450

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(std140, binding = 0) uniform ubo_mats {
	mat4 mvp_mat_follow_view;
	mat4 mvp_flat_sky_layer;
	mat4 mvp_objects;
};

uniform mat4 u_model_mat;

out vec3 v_normal;
out vec2 v_uv;

void main() {
	gl_Position = mvp_objects * u_model_mat * vec4(in_pos, 1.0);
	v_normal = in_normal;
	v_uv = in_uv;
}
