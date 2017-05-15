/*
 * By Dody Dharma
 * 6 May 2017
 */

#version 150
layout(points) in;
layout(line_strip, max_vertices = 3) out;

in vec4 trailPos[];
in vec4 vColor[]; // Output from vertex shader for each vertex

out vec4 gColor; // Output to fragment shader

void main()
{
    gColor = vColor[0];
    
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    
    gl_Position = trailPos[0];
    EmitVertex();
    
    EndPrimitive();
}
