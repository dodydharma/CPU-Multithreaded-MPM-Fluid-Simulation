#version 150

uniform sampler2D	tex0;

in vec2 vTexCoord0;

out vec4 oColor;

vec3 rgb2hsv(vec3 c)
{
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}


void main() {
    vec4 sum = texture( tex0, vTexCoord0 ).rgba;
    float threshold1 = 0.2;
    float threshold2 = 0.1;
    vec3 sum_hsv = rgb2hsv(sum.rgb);
    if (sum_hsv.z > threshold1) {
        //        sum = texture( tex0, vTexCoord0 +   0.0 * sample_offset ).rgba;
        sum_hsv.z = 1;
        sum.rgb = hsv2rgb(sum_hsv);
        sum.a = 0.7;
    } else {
        if (sum_hsv.z > threshold2) {
            //                sum = texture( tex0, vTexCoord0 +   0.0 * sample_offset ).rgba;
            sum_hsv.z = 1.0;
            sum.rgb = hsv2rgb(sum_hsv);
            sum.a = 1;
        } else {
            sum.a = 0;
        }
    }
    
    oColor.rgba = sum;
}
