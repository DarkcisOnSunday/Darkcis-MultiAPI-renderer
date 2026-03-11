#version 450

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    vec4 lightDirVS_ambient;
} uFrame;

layout(push_constant) uniform ObjectPC {
    mat4 model;
    uint materialFlags;
    float _pad0;
    float _pad1;
    float _pad2;
} uObj;

layout(location = 0) in vec3 inPos;
layout(location = 1) in uint inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outNormalVS;

vec4 UnpackColor(uint c) {
    float a = float((c >> 24) & 255u) / 255.0;
    float r = float((c >> 16) & 255u) / 255.0;
    float g = float((c >> 8) & 255u) / 255.0;
    float b = float(c & 255u) / 255.0;
    return vec4(r, g, b, a);
}

void main() {
    mat4 modelView = uFrame.view * uObj.model;
    vec4 viewPos = modelView * vec4(inPos, 1.0);
    gl_Position = uFrame.proj * viewPos;

    outColor = UnpackColor(inColor);
    outUV = inUV;
    outNormalVS = mat3(modelView) * inNormal;
}
