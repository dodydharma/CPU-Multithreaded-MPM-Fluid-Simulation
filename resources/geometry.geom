/*
 * By Dody Dharma
 * 6 May 2017
 */

#version 150
layout(points) in;
layout(triangle_strip, max_vertices = 100) out;

#define PI 3.14

uniform float circleRadius;

in vec4 trailPos[];
in vec4 vColor[]; // Output from vertex shader for each vertex

out vec4 gColor; // Output to fragment shader

float atan2(in float y, in float x)
{
    bool s = (abs(x) > abs(y));
    float ret = mix(PI/2.0 - atan(x,y), atan(y,x), s);
    if (ret < 0) {
        ret += 2*PI;
    }
    return ret;
}

void create_pill(vec4 pos, vec4 trail_pos) {
    float circleRadius = circleRadius;
    
    int n = 6;
    float step = 2*PI/n;
    
    
    
    float delta_x = pos.x - trail_pos.x;
    float delta_y = pos.y - trail_pos.y;
    float angle = atan2(delta_y, delta_x);
    angle -= PI/2;
    int i = 0;
    
    float reduced_alpha = 0.5;
    
    vec4 start1 = pos + vec4(circleRadius * cos(angle) * 0.2, circleRadius * sin(angle) * 0.4, 0.0, 0.0);
    for(; i < (n/2) ; ++i)
    {
        
        gl_Position = pos;
        gColor.a = 1.0;
        EmitVertex();
        
        gl_Position = pos + vec4(circleRadius * cos(angle) * 0.2, circleRadius * sin(angle) * 0.4, 0.0, 0.0);
        gColor.a = reduced_alpha;
        EmitVertex();
        
        angle += step;
        if (angle >= 2*PI) {
            angle -= 2*PI;
        }
        gl_Position = pos + vec4(circleRadius * cos(angle) * 0.2, circleRadius * sin(angle) * 0.4, 0.0, 0.0);
        gColor.a = reduced_alpha;
        EmitVertex();
    }
    EndPrimitive();
    vec4 end1 = pos + vec4(circleRadius * cos(angle) * 0.2, circleRadius * sin(angle) * 0.4, 0.0, 0.0);
    
    vec4 start2 = trail_pos + vec4(circleRadius * cos(angle) * 0.2, circleRadius * sin(angle) * 0.4, 0.0, 0.0);
    for(; i < n ; ++i)
    {
        
        gl_Position = trail_pos;
        gColor.a = 1.0;
        EmitVertex();
        
        gl_Position = trail_pos + vec4(circleRadius * cos(angle) * 0.2, circleRadius * sin(angle) * 0.4, 0.0, 0.0);
        gColor.a = reduced_alpha;
        EmitVertex();
        
        angle += step;
        if (angle >= 2*PI) {
            angle -= 2*PI;
        }
        gl_Position = trail_pos + vec4(circleRadius * cos(angle) * 0.2, circleRadius * sin(angle) * 0.4, 0.0, 0.0);
        gColor.a = reduced_alpha;
        EmitVertex();
    }
    EndPrimitive();
    vec4 end2 = trail_pos + vec4(circleRadius * cos(angle) * 0.2, circleRadius * sin(angle) * 0.4, 0.0, 0.0);
    
    gl_Position = start1;
    EmitVertex();
    
    gl_Position = end1;
    EmitVertex();
    
    gl_Position = start2;
    EmitVertex();
    EndPrimitive();
    
    gl_Position = start2;
    EmitVertex();
    
    gl_Position = end2;
    EmitVertex();
    
    gl_Position = start1;
    EmitVertex();
    EndPrimitive();
    
}

void create_circle(vec4 pos) {
    float circleRadius = 5.0;
    
    int n = 6;
    float step = 2*PI/n;
    
    float angle = 0;
    
    for(int i = 0; i < n ; ++i)
    {
        
        gl_Position = pos;
        EmitVertex();
        
        gl_Position = pos + vec4(circleRadius * cos(angle) * 0.2, circleRadius * sin(angle) * 0.4, 0.0, 0.0);
        EmitVertex();
        
        angle += step;
        gl_Position = pos + vec4(circleRadius * cos(angle) * 0.2, circleRadius * sin(angle) * 0.4, 0.0, 0.0);
        EmitVertex();
    }
    EndPrimitive();
}


void main()
{
    gColor = vColor[0];
    
    //    gl_Position = gl_in[0].gl_Position;
    //    EmitVertex();
    //
    
    
    //    create_circle(gl_in[0].gl_Position);
    create_pill(gl_in[0].gl_Position, trailPos[0]);
    //    gl_Position = pos + vec4(circleRadius, circleRadius, 0.0, 0.0);   //circle parametric equation
    //    EmitVertex();
    //    gl_Position = pos + vec4(-circleRadius, circleRadius, 0.0, 0.0);   //circle parametric equation
    //    EmitVertex();
    //    gl_Position = pos + vec4(circleRadius, -circleRadius, 0.0, 0.0);   //circle parametric equation
    //    EmitVertex();
    //    gl_Position = pos + vec4(-circleRadius, -circleRadius, 0.0, 0.0);   //circle parametric equation
    //    EmitVertex();
    ////
    //    gl_Position = trailPos[0];
    //    EmitVertex();
    
    //    gl_Position = trailPos[0] + vec4(10.0, 10.0, 0.0, 0.0);
    //    EmitVertex();
    //
    //    gl_Position = trailPos[0] + vec4(10.0, -10.0, 0.0, 0.0);
    //    EmitVertex();
    
    EndPrimitive();
}
