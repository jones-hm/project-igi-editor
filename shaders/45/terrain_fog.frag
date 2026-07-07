/******************************************************************************
 * @file    terrain_fog.frag
 * @brief   fragment shader to apply fog
 *****************************************************************************/

#version 450 core

// linear fog
layout (std140, binding = 1) uniform ub_fog
{
	vec4	g_fog_color;
	float	g_fog_far;
	float	g_fog_pad0;
	float	g_fog_pad1;
	float	g_fog_pad2;
	float	g_fog_intensity;
};

layout (location = 0) in  vec3  vs_out_pos_in_view;

layout (location = 0) out vec4 ps_out_color;

const float FOG_NEAR_FACTOR = 0.333333;	// tune this

float calc_fog_blend_alpha() {
	float distance = length(vs_out_pos_in_view);
	float eff_far = g_fog_far / max(g_fog_intensity, 0.0001);
	float fog_near = eff_far * FOG_NEAR_FACTOR;

	float visibility = (eff_far - distance) / (eff_far - fog_near);
	
	visibility = clamp(visibility, 0.0, 1.0);
	float fog_a =  (1.0 - visibility);

	return fog_a;
}

void main() {
	float fog_a = calc_fog_blend_alpha();
	ps_out_color = vec4(g_fog_color.xyz, fog_a);
}
