shader_type canvas_item;

uniform sampler2D blue_noise : filter_nearest, repeat_enable;
uniform float noise_scale = 1.0;  // Controls tiling density

void fragment() {
    float gray = dot(COLOR.rgb, vec3(0.299, 0.587, 0.114));
    vec2 noise_uv = FRAGCOORD.xy / (vec2(textureSize(blue_noise, 0)) * noise_scale);
    float threshold = texture(blue_noise, noise_uv).r;
    float dithered = step(threshold, gray);
    COLOR = vec4(vec3(dithered), COLOR.a);
}
