#version 450

in vec3 v_normal;
in vec2 v_uv;

out vec4 out_color;

void main() {
	vec3 light_dir = normalize(vec3(0.5, 1.0, 0.5));
	float diffuse = max(dot(v_normal, light_dir), 0.2);
	out_color = vec4(vec3(0.8) * diffuse, 1.0);
}
