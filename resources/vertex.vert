/*
 * By Dody Dharma
 * 6 May 2017
 */

#version 150
uniform     mat4    ciModelViewProjection;
uniform     bool    isUVMap;

in          vec4    ciPosition;
in          vec4    ciColor;
in          vec4    trailPosition;
in          float    isUVRed;

out         vec4    vColor;
out         vec4    trailPos;

void main(void){
    gl_Position = ciModelViewProjection * ciPosition;
    trailPos    = ciModelViewProjection * trailPosition;
    if (isUVMap) {
        if (isUVRed > 0.5) {
            vColor       = vec4(1, 0, 0, 0);
        } else {
            vColor       = vec4(0, 1, 0, 0);
        }
    } else {
        vColor       = ciColor;
    }
}
