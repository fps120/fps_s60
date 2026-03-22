#version 330 core
layout(location=0)in vec3 aPos;
layout(location=1)in vec3 aNormal;
layout(location=2)in vec3 aColor;
out vec3 Normal;out vec3 VertColor;
uniform mat4 uModel,uView,uProj;
void main(){Normal=mat3(transpose(inverse(uModel)))*aNormal;VertColor=aColor;gl_Position=uProj*uView*uModel*vec4(aPos,1.0);}
