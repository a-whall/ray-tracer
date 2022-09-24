#shader compute
#version 460

layout (local_size_x = 1, local_size_y = 1) in;

layout (RGBA32F, binding = 0) uniform image2D render_image;

layout (std430, binding=0) buffer SceneData     { float heap[]; };
layout (std430, binding=1) buffer GeometryIndex { ivec4 gbuf[]; };
layout (std430, binding=2) buffer MaterialIndex { ivec2 mbuf[]; };
layout (std430, binding=3) buffer LightIndex    { ivec2 lbuf[]; };

// Geometry SubTypes
struct Plane  { ivec2 i; }; // { heap-index, mbuf-index }
struct Sphere { ivec2 i; };

// Material SubTypes
struct Diffuse    { vec3 ka; vec3 kd; };
struct Specular   { vec3 ka; vec3 kd; vec3 ks; float p; };
struct Reflective { vec3 kr; };
struct Glass      { vec3 kr; vec3 kt; float ior; };

// Light SubTypes
struct Directional { int i; }; // { heap-index }
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
uniform vec3 ambient = vec3(0.05, 0.05, 0.05);

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
  ivec2 m = mbuf[isect.material_idx];
  for (int li = 0; li < numLights; li++) {
    Light light = getLightSample(li, isect.position);
    vec3 pointToLight = light.position - isect.position;
    vec3 l = normalize(pointToLight);
    Ray shadowRay = Ray(isect.position, l);
    Isect shadowIsect = castRay(shadowRay);
    if (shadowIsect.t > tmin && shadowIsect.t < length(pointToLight))
      continue;
    vec3 n = isect.normal;
    vec3 v = ray.d;
    vec3 r = reflect(l, n);
    vec3 diffuse = vec3(heap[m.y+3],heap[m.y+4],heap[m.y+5]) * max(dot(n,l), 0.0);
    vec3 specular = (m.x == 1) ? vec3(heap[m.y+6],heap[m.y+7],heap[m.y+8]) * pow(max(dot(r,v), 0.0),heap[m.y+9]) : vec3(0);
    color += light.color * (specular + diffuse);
  }
  return ambient * vec3(heap[m.y],heap[m.y+1],heap[m.y+2]) + color;
}

vec3 uniformSampleHemisphere(float r1, float r2)
{
  float sin_theta = sqrt(1. - r1*r1);
  float phi = 2. * pi * r2;
  return vec3(sin_theta*cos(phi), r1, sin_theta*sin(phi));
}

vec3 computeIndirectDiffuse(uvec2 pixel, Isect isect)
{
  const uint N = 100u;
  vec3 indirectDiffuse = vec3(0);
  vec3 nb, nt, n = isect.normal;
  nt = abs(n.x) > abs(n.y) ? vec3(n.z,0,-n.x)/sqrt(n.x*n.x+n.z*n.z) : vec3(0,-n.z,n.y)/sqrt(n.y*n.y+n.z*n.z);
  nb = cross(n, nt);
  uint hitCount = 0;
  for (uint i = 0u; i < N; i++) {
    float r1 = random(uvec3(pixel.xy, i));
    float r2 = random(uvec3(pixel.xy, i));
    vec3 sRay = uniformSampleHemisphere(r1, r2);
    vec3 sampleWorld = vec3(
      sRay.x*nb.x + sRay.y*n.x + sRay.z*nt.x,
      sRay.x*nb.y + sRay.y*n.y + sRay.z*nt.y,
      sRay.x*nb.z + sRay.y*n.z + sRay.z*nt.z
    );
    Ray sampleRay = Ray(isect.position+sampleWorld, sampleWorld);
    Isect tsect = castRay(sampleRay);
    if (tsect.t > 0) {
      ivec2 mi = mbuf[tsect.material_idx];
      if (mi.x == 0 || mi.x == 1) {
        indirectDiffuse += r1 * shading(sampleRay, tsect);
        hitCount += 1;
      }
    }
  }
  return indirectDiffuse / N;
}


void main()
{
  float x = float(gl_GlobalInvocationID.x) / 960.0;
  float y = (639.0 - float(gl_GlobalInvocationID.y)) / 640.0;
  Ray ray[maxDepth+1];
  ivec2 m[maxDepth+1];
  ray[0] = Ray(cam.eye, normalize((cam.corner+cam.across*x+cam.up*y)-cam.eye));
  vec3 pixel_color = vec3(0);
  for (int i = 0; i < maxDepth; i++) {
    Isect isect = castRay(ray[i]);
    if (isect.t > 0) {
      m[i] = mbuf[isect.material_idx].xy;
      if (m[i].x == 0 || m[i].x == 1) {
        if (i > 0 && m[i-1].x == 2) {
          pixel_color += vec3(heap[m[i-1].y], heap[m[i-1].y+1], heap[m[i-1].y+2]) * shading(ray[i], isect);
        }
        else if (i > 0 && m[i-1].x == 3) {
          pixel_color += vec3(heap[m[i-1].y], heap[m[i-1].y+1], heap[m[i-1].y+2]) * shading(ray[i], isect);
        }
        else {
          pixel_color += shading(ray[i], isect);
          // pixel_color += computeIndirectDiffuse(gl_GlobalInvocationID.xy, isect);
        }
        break;
      }
      if (m[i].x == 2)
        ray[i+1] = Ray(isect.position, reflect(ray[i].d, isect.normal));
      if (m[i].x == 3)
        ray[i+1] = Ray(isect.position, refract(-ray[i].d, isect.normal, heap[m[i].y+3]));
    }
    else break;
  }
  pixel_color = clamp(pixel_color, 0.0, 1.0);
  imageStore(render_image, ivec2(gl_GlobalInvocationID.xy), vec4(pixel_color,1));
}
#end