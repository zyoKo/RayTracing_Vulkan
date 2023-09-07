
#version 450

// A triangle of points that covers the [-1,1]X[-1,1] NDC space.
// These will be indexed with gl_VertexIndex = 0, 1, 2.
vec3 points[3] = vec3[]( vec3(-1,-1,0), vec3(-1,3,0), vec3(3,-1,0) );

void main()
{
  gl_Position.xyz = vec3(points[gl_VertexIndex]);
}
