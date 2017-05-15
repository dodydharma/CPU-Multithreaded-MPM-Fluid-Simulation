/*
 * By Dody Dharma
 * 6 May 2017
 */

#version 150
uniform     mat4    ciModelViewProjection;

in          vec4    ciPosition;
in          vec4    ciColor;
in          vec4    trailPosition;

out         vec4    vColor;
out         vec4    trailPos;

void main(void){
    gl_Position = ciModelViewProjection * ciPosition;
    trailPos    = ciModelViewProjection * trailPosition;
    vColor       = ciColor;
}
