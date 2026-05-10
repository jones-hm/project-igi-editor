#version 410

in vec3 v_normal;
in vec2 v_uv;

out vec4 out_color;

void main() {
	vec3 light_dir = normalize(vec3(0.5, 1.0, 0.5));
	vec3 n = normalize(v_normal);
	float diffuse = max(dot(n, light_dir), 0.2);
	out_color = vec4(vec3(0.8) * diffuse, 1.0);
}
