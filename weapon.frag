#version 330 core
in vec3 Normal;in vec3 VertColor;
out vec4 FragColor;
uniform vec3 uLightDir;
void main(){float d=max(dot(normalize(Normal),normalize(uLightDir)),0.0)*0.7+0.3;FragColor=vec4(VertColor*d,1.0);}
