#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec3 VertColor;

out vec4 FragColor;

uniform vec3  uLightDir;    // normalised direction TO light
uniform vec3  uLightColor;
uniform vec3  uAmbient;
uniform vec3  uCamPos;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec3  uFogColor;

void main() {
    vec3 norm  = normalize(Normal);
    vec3 ldir  = normalize(uLightDir);

    // Diffuse
    float diff  = max(dot(norm, ldir), 0.0);
    vec3  diffC = diff * uLightColor;

    // Specular (Blinn-Phong)
    vec3  viewDir  = normalize(uCamPos - FragPos);
    vec3  halfDir  = normalize(ldir + viewDir);
    float spec     = pow(max(dot(norm, halfDir), 0.0), 32.0);
    vec3  specC    = spec * uLightColor * 0.15;

    vec3 lighting = (uAmbient + diffC + specC) * VertColor;

    // Distance fog
    float dist = length(uCamPos - FragPos);
    float fog  = clamp((dist - uFogStart) / (uFogEnd - uFogStart), 0.0, 1.0);
    vec3  col  = mix(lighting, uFogColor, fog);

    FragColor = vec4(col, 1.0);
}
