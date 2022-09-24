#shader compute
#version 460

layout (local_size_x = 1, local_size_y = 1) in;

layout (RGBA32F, binding = 0) uniform image2D render_image;

layout (std430, binding=0) buffer SceneData     { float heap[]; };
layout (std430, binding=1) buffer GeometryIndex { ivec4 gbuf[]; };
layout (std430, binding=2) buffer MaterialIndex { ivec2 mbuf[]; };
layout (std430, binding=3) buffer LightIndex    { ivec2 lbuf[]; };

// Geometry SubTypes
struct Plane  { ivec2 i; }; // { heap-address, material-index }
struct Sphere { ivec2 i; };

// Material SubTypes
struct Diffuse  { vec3 ka; vec3 kd; };
struct Specular { vec3 ka; vec3 kd; vec3 ks; float p; };
struct Mirror   { vec3 kr; };
struct Glass    { vec3 kr; vec3 kt; float ior; };

// Light SubTypes
struct Directional { int i; };
struct Point       { int i; };
struct Spot        { int i; };

// Miscellaneous Types
struct Ray { vec3 o; vec3 d; };
struct Light { vec3 position; vec3 color; vec3 direction; };
struct Isect { float t; vec3 position; vec3 normal; int material_idx; };
struct Camera { vec3 eye; vec3 across; vec3 corner; vec3 up; };

// uniforms and constants
const float pi = 3.14159;
const int maxDepth = 5;
const uniform float tmin = 0.05;
const uniform float tmax = 1e20;
uniform Camera cam;
uniform int numShapes;
uniform int numLights;
uniform vec3 ambient = vec3(0.1, 0.1, 0.1);

// jenkins one-at-a-time hash
uint hash(uint x) {
  x += (x << 10u);
  x ^= (x >> 6u);
  x += (x << 3u);
  x ^= (x >> 11u);
  x += (x << 15u);
  return x;
}
uint hash(uvec3 v) { return hash(v.x ^ hash(v.y) ^ hash(v.z)); }
float randomFloatBetween0and1(uint seed) { return uintBitsToFloat((seed&0x007FFFFFu)|0x3F800000u) - 1.0; }
float random(uvec3 v) { return randomFloatBetween0and1(hash(v)); }

Isect intersect(Plane plane, Ray ray, float current_tmax)
{
  int i = plane.index.x;
  int midx = plane.index.y;
  vec3 plane_point = vec3(heap[i], heap[i+1], heap[i+2]);
  vec3 plane_normal = normalize(vec3(heap[i+3], heap[i+4], heap[i+5]));
  float denom = dot(ray.d, plane_normal);
  if (denom != 0) {
    float t = dot(plane_point - ray.o, plane_normal) / denom;
    if (t > tmin && t < current_tmax) {
      vec3 position = pointAt(ray, t);
      return Isect(t, position, plane_normal, midx);
    }
  }
  return Isect(-1, vec3(0), vec3(0), -1);
}


Isect intersect(Sphere sphere, Ray ray, float current_tmax)
{
  int gidx = sphere.index.x;
  int midx = sphere.index.y;
  vec3 center = vec3(heap[gidx], heap[gidx+1], heap[gidx+2]);
  float radius = heap[gidx+3];
  float solution = -1.0;
  float B = 2 * dot(ray.o-center, ray.d);
  float C = pow(length(ray.o-center), 2) - radius * radius;
  float D = pow(B,2) - 4 * C;
  if (D < 0) // no solution
    return Isect(-1, vec3(0), vec3(0), -1);
  else { // find solution
    float t1 = (-B - sqrt(D)) / 2.0;
    if (t1 >= tmin && t1 <= current_tmax)
      solution = t1;
    else {
      float t2 = (-B + sqrt(D)) / 2.0;
      if (t2 >= tmin && t2 <= current_tmax)
        solution = t2;
    }
  }
  if (solution != -1.0) {
    vec3 position = pointAt(ray, solution);
    vec3 normal = normalize(position - center);
    return Isect(solution, position, normal, midx);
  }
  return Isect(-1, vec3(0), vec3(0), -1);
}


Isect checkIsect(Ray ray, int i, float current_tmax)
{
  int shapeType = gbuf[i].x;
  ivec2 geom_data = ivec2(gbuf[i].yz); // y: heap-index | z: mbuf-index
  switch (shapeType) {
    case 0: return intersect(Plane(geom_data), ray, current_tmax);
    case 1: return intersect(Sphere(geom_data), ray, current_tmax);
  }
  return Isect(-1, vec3(0), vec3(0), -1);
}


Isect rayIntersectScene(Ray ray)
{
  float current_min_t = tmax;
  Isect result = Isect(-1, vec3(0), vec3(0), -1);
  for (int i = 0; i < numShapes; i++) {
    Isect hit = checkIsect(ray, i, current_min_t);
    if (hit.t > 0) {
      current_min_t = hit.t;
      result = Isect(hit.t, hit.position, hit.normal, hit.material_idx);
    }
  }
  return result;
}


LightSample lsample(PointLight light, vec3 shadingPoint)
{
  int l = light.addr;
  vec3 position  = vec3(heap[l+0], heap[l+1], heap[l+2]);
  vec3 intensity = vec3(heap[l+3], heap[l+4], heap[l+5]);
  vec3 ambient   = vec3(heap[l+6], heap[l+7], heap[l+8]);
  vec3 direction = position - shadingPoint;
  return LightSample(
    position,
    intensity * 100.0 / pow(length(direction), 2),
    ambient,
    normalize(direction)
  );
}


LightSample lsample(SpotLight light, vec3 shadingPoint)
{
  return LightSample(vec3(0), vec3(0), vec3(0), vec3(0));
}


LightSample getSample(int i, vec3 shadingPoint)
{
  int lightType = lbuf[i].x;
  int lightAddr = lbuf[i].y;
  switch(lightType) {
    case 0: return lsample(PointLight(lightAddr), shadingPoint);
    case 1: return lsample(SpotLight(lightAddr), shadingPoint);
  }
  return LightSample(vec3(0), vec3(0), vec3(0), vec3(0));
}


vec3 shading(Ray ray, Isect isect)
{
  vec3 color = vec3(0);
  int mtype = mbuf[isect.material_idx].x;
  int idx = mbuf[isect.material_idx].y;
  vec3 ka = vec3(heap[idx], heap[idx+1], heap[idx+2]);
  for (int i = 0; i < numLights; i++) {
    LightSample light = getSample(i, isect.position);
    vec3 ambient = light.ambient * ka;
    vec3 pos_to_light = light.position - isect.position;
    vec3 l = normalize(pos_to_light);
    float dist_to_light = length(pos_to_light);
    Ray shadowRay = Ray(isect.position, l);
    Isect shadowIsect = rayIntersectScene(shadowRay);
    if (shadowIsect.t > tmin && shadowIsect.t < dist_to_light) {
      color += ambient;
      continue;
    }
    vec3 n = isect.normal;
    vec3 v = ray.d;
    vec3 r = reflect(l, n);
    vec3 kd = vec3(heap[idx+3], heap[idx+4], heap[idx+5]); // for now, don't need to worry about materials having (or not having) kd since only supports diffuse and phong
    vec3 diffuse = kd * max(dot(n,l), 0.0);
    vec3 specular = vec3(0);
    if (mtype == 1) { // is a phong material (has a ks)
      vec3 ks = vec3(heap[idx+6], heap[idx+7], heap[idx+8]);
      float p = heap[idx+9];
      specular = ks * pow( max(dot(r,v), 0.0), p );
    }
    color += ambient + light.intensity * (specular + diffuse);
  }
  return color;
}


vec4 rayTracing(Ray ray) { // currently only checks the camera ray. no bouncing rays
  vec3 color = vec3(0);
  for (int i = 0; i < 1; i++) {
    Isect isect = rayIntersectScene(ray);
    color += (isect.t > 0) ? shading(ray, isect) : vec3(0);
  }
  clamp(color, 0.0, 1.0);
  return vec4(color, 1);
}


void main() {
  vec4 color = vec4(0);
  // Create the original ray
  float x = float(gl_GlobalInvocationID.x) / 960.0;
  float y = (639.0 - float(gl_GlobalInvocationID.y)) / 640.0;
  float top = tan(camera.fov * pi / 360.0);
  float bottom = -top;
  float right = camera.aspect * top;
  float left = -right;
  vec3 W = normalize(camera.eye - camera.target);
  vec3 U = normalize(cross(vec3(0,1,0), W));
  vec3 V = cross(W, U) * 2 * top;
  vec3 across = U * 2 * right;
  vec3 corner = camera.eye + U * left + V * bottom + W * -1.0;
  vec3 up = V * 2 * top;
  Ray ray = Ray(
    camera.eye,
    normalize((corner + across * x + up * y) - camera.eye)
  );
  // Cast the ray to get a pixel color
  color = rayTracing(ray);
  // Save the result to a HDR texture
  imageStore(render_image, ivec2(gl_GlobalInvocationID.xy), color);
}
#end