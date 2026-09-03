#version 450

layout(push_constant) uniform Square {
    vec2 center;
    vec2 half_extent;
    vec4 color;
} square;

layout(location = 0) flat out vec4 v_color;

const vec2 corners[6] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);

void main() {
    const vec2 local = corners[gl_VertexIndex];
    const vec2 position = square.center + local * square.half_extent;
    gl_Position = vec4(position, 0.0, 1.0);
    v_color = square.color;
}
