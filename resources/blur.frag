#version 150

uniform sampler2D	tex0;
uniform vec2		sample_offset;
uniform float		attenuation;
uniform bool        use_threshold;

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

void main()
{
    vec4 sum = vec4( 0.0, 0.0, 0.0, 0.0 );
    sum += texture( tex0, vTexCoord0 + -10.0 * sample_offset ).rgba * 0.009167927656011385;
    sum += texture( tex0, vTexCoord0 +  -9.0 * sample_offset ).rgba * 0.014053461291849008;
    sum += texture( tex0, vTexCoord0 +  -8.0 * sample_offset ).rgba * 0.020595286319257878;
    sum += texture( tex0, vTexCoord0 +  -7.0 * sample_offset ).rgba * 0.028855245532226279;
    sum += texture( tex0, vTexCoord0 +  -6.0 * sample_offset ).rgba * 0.038650411513543079;
    sum += texture( tex0, vTexCoord0 +  -5.0 * sample_offset ).rgba * 0.049494378859311142;
    sum += texture( tex0, vTexCoord0 +  -4.0 * sample_offset ).rgba * 0.060594058578763078;
    sum += texture( tex0, vTexCoord0 +  -3.0 * sample_offset ).rgba * 0.070921288047096992;
    sum += texture( tex0, vTexCoord0 +  -2.0 * sample_offset ).rgba * 0.079358891804948081;
    sum += texture( tex0, vTexCoord0 +  -1.0 * sample_offset ).rgba * 0.084895951965930902;
    sum += texture( tex0, vTexCoord0 +   0.0 * sample_offset ).rgba * 0.086826196862124602;
    sum += texture( tex0, vTexCoord0 +  +1.0 * sample_offset ).rgba * 0.084895951965930902;
    sum += texture( tex0, vTexCoord0 +  +2.0 * sample_offset ).rgba * 0.079358891804948081;
    sum += texture( tex0, vTexCoord0 +  +3.0 * sample_offset ).rgba * 0.070921288047096992;
    sum += texture( tex0, vTexCoord0 +  +4.0 * sample_offset ).rgba * 0.060594058578763078;
    sum += texture( tex0, vTexCoord0 +  +5.0 * sample_offset ).rgba * 0.049494378859311142;
    sum += texture( tex0, vTexCoord0 +  +6.0 * sample_offset ).rgba * 0.038650411513543079;
    sum += texture( tex0, vTexCoord0 +  +7.0 * sample_offset ).rgba * 0.028855245532226279;
    sum += texture( tex0, vTexCoord0 +  +8.0 * sample_offset ).rgba * 0.020595286319257878;
    sum += texture( tex0, vTexCoord0 +  +9.0 * sample_offset ).rgba * 0.014053461291849008;
    sum += texture( tex0, vTexCoord0 + +10.0 * sample_offset ).rgba * 0.009167927656011385;
    
    if (use_threshold) {
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
    }
    
    
    oColor.rgba = attenuation * sum;
    
}
