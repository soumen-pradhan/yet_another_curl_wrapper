#version 450

void main() {
    // Generate a full-screen triangle using gl_VertexIndex
    // This technique covers the entire screen space (-1 to +1 in X and Y).
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
