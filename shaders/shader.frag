#version 450

layout(set = 0, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 proj;
    vec4 lightDirVS_ambient;
} uFrame;

layout(set = 1, binding = 0) uniform sampler2D uAlbedo;

layout(push_constant) uniform ObjectPC {
    mat4 model;
    uint materialFlags;
    float _pad0;
    float _pad1;
    float _pad2;
} uObj;

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormalVS;

layout(location = 0) out vec4 outColor;

const uint TEXTURE_NONE = 0u;
const uint TEXTURE_MODULATE = 1u;
const uint TEXTURE_REPLACE = 2u;

void main() {
    vec4 tex = texture(uAlbedo, inUV);
    vec4 base = inColor;

    if (uObj.materialFlags == TEXTURE_REPLACE) {
        base = tex;
    } else if (uObj.materialFlags == TEXTURE_MODULATE) {
        base = tex * inColor;
    }

    vec3 n = normalize(inNormalVS);
    vec3 l = normalize(uFrame.lightDirVS_ambient.xyz);
    float ambient = uFrame.lightDirVS_ambient.w;
    float ndotl = max(dot(n, l), 0.0);
    float light = ambient + (1.0 - ambient) * ndotl;

    outColor = vec4(base.rgb * light, base.a);
}
