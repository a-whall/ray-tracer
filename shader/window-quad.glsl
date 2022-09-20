#shader vertex
#version 460

layout (location = 0) in vec4 vbuf;
out vec2 tex_coords;

uniform bool flip = false;

void main()
{
  tex_coords = vbuf.zw;
  if (flip) {
    if (tex_coords.t == 1.0)
      tex_coords.t = -1.0;
    else if (tex_coords.t == -1.0)
      tex_coords.t = 1.0;
  }
  gl_Position = vec4(vbuf.xy, 0, 1);
}

#shader fragment
#version 460

in vec2 tex_coords;
out vec4 frag_color;

layout (RGBA32F, binding = 0) uniform image2D render_image;

void main()
{
  frag_color = imageLoad(render_image, ivec2(tex_coords.s*960, tex_coords.t*640)).rgba;
}
#end