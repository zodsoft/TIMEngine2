#version 430

precision highp float;

in vec3 vertex;  
in vec2 texCoord;

smooth out vec2 coord;

void main()  
{
	coord = texCoord;	
	gl_Position=vec4(vertex,1);
}