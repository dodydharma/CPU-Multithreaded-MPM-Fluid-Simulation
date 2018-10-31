#version 150

uniform sampler2D	tex0;
uniform vec2		sample_offset;
uniform float       kernel[30];
uniform int         kernelSize;

in vec2 vTexCoord0;

out vec4 oColor;

void main()
{
    vec4 sum = vec4( 0.0, 0.0, 0.0, 0.0 );
    for (int i = 0; i < kernelSize; ++i) {
        sum += texture( tex0, vTexCoord0 + (i - kernelSize/2) * sample_offset ).rgba * kernel[i];
    }
    
    oColor.rgba = sum;

    
}
