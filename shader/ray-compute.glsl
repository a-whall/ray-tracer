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
  int i = plane.i.x;
  vec3 p = vec3(heap[i], heap[i+1], heap[i+2]);
  vec3 n = normalize(vec3(heap[i+3], heap[i+4], heap[i+5]));
  float denom = dot(ray.d, n);
  if (denom != 0) {
    float t = dot(p-ray.o, n) / denom;
    if (t > tmin && t < current_tmax)
      return Isect(t, ray.o+ray.d*t, n, plane.i.y);
  }
  return Isect(-1, vec3(0), vec3(0), -1);
}

Isect intersect(Sphere sphere, Ray ray, float current_tmax)
{
  int i = sphere.i.x;
  vec3 c = vec3(heap[i], heap[i+1], heap[i+2]);
  float r = heap[i+3];
  float t = -1.0;
  float B = 2 * dot(ray.o-c, ray.d);
  float C = pow(length(ray.o-c), 2) - r * r;
  float D = pow(B,2) - 4 * C;
  if (D >= -tmin) { // (D < 0) => no solution
    float sol = (-B - sqrt(D)) / 2.0;
    if (sol >= tmin && sol <= current_tmax)
      t = sol;
    else {
      sol = (-B + sqrt(D)) / 2.0;
      if (sol >= tmin && sol <= current_tmax)
        t = sol;
    }
  }
  if (t != -1.0)
    return Isect(t, ray.o+ray.d*t, normalize((ray.o+ray.d*t)-c), sphere.i.y);
  return Isect(-1, vec3(0), vec3(0), -1);
}

Isect checkIsect(Ray ray, int i, float current_tmax)
{
  switch (gbuf[i].x) {
    case 0: return intersect(Plane(gbuf[i].yz), ray, current_tmax);
    case 1: return intersect(Sphere(gbuf[i].yz), ray, current_tmax);
    case 2: break;
  }
  return Isect(-1, vec3(0), vec3(0), -1);
}

Isect castRay(Ray ray)
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

Light _sample(Directional light, vec3 shadingPoint)
{
  int i = light.i;
  vec3 lp = vec3(1./0., 0., 0.);
  vec3 ld = vec3(heap[i+0], heap[i+1], heap[i+2]);
  vec3 lc = vec3(heap[i+3], heap[i+4], heap[i+5]);
  return Light(lp, lc, normalize(ld));
}

Light _sample(Point light, vec3 sp)
{
  int i = light.i;
  vec3 lp = vec3(heap[i+0], heap[i+1], heap[i+2]);
  vec3 lc = vec3(heap[i+3], heap[i+4], heap[i+5]);
  float li = heap[i+6];
  vec3 ld = lp - sp;
  return Light(lp, li*lc/pow(length(ld),2), normalize(ld));
}

Light _sample(Spot light, vec3 sp)
{
  int i = light.i;
  vec3 lp = vec3(heap[i+0], heap[i+1], heap[i+2]);
  vec3 ld = vec3(heap[i+3], heap[i+4], heap[i+5]);
  vec3 lc = vec3(heap[i+6], heap[i+7], heap[i+8]);
  float li = heap[i+9];
  float lr = heap[i+10];
  float la = heap[i+11];
  const float le = 50.0;
  vec3 lightToPoint = sp - lp;
  float cos_ld = dot(normalize(lightToPoint), normalize(ld));
  if (cos_ld > cos(la*pi/360.))
    lc *= li / pow(length(lightToPoint),2) * pow(cos_ld, le);
  else
    lc = ambient;
  return Light(lp, lc, normalize(lightToPoint));
}

Light getLightSample(int i, vec3 shadingPoint)
{
  switch (lbuf[i].x) { // switch l_type: pass l_index
    case 0: return _sample(Directional(lbuf[i].y), shadingPoint);
    case 1: return _sample(Point(lbuf[i].y), shadingPoint);
    case 2: return _sample(Spot(lbuf[i].y), shadingPoint);
  }
  return Light(vec3(0), vec3(0), vec3(0));
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