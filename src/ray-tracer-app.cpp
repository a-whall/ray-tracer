#include "ray-tracer-app.h"



#define NUM_BUFFERS 6
#define EBUF 0
#define VBUF 1
#define HEAP 2
#define GBUF 3
#define MBUF 4
#define LBUF 5

Shader ray_shader, render_shader;
GLuint render_tex, render_vao, bufferID[NUM_BUFFERS];

void Ray_Tracer_App::on_init()
{
  render_shader.handle = glCreateProgram();
  render_shader.create(GL_VERTEX_SHADER, GL_FRAGMENT_SHADER);
  render_shader.source("shader/window-quad.glsl");
  render_shader.compile();
  render_shader.link();
  ray_shader.handle = glCreateProgram();
  ray_shader.create(GL_COMPUTE_SHADER);
  ray_shader.source("shader/ray-compute.glsl");
  ray_shader.compile();
  ray_shader.link();
  glCreateVertexArrays(1, &render_vao);
  glCreateBuffers(NUM_BUFFERS, bufferID);
  GLuint window_quad_EB[6] = { 0u,1u,2u, 2u,3u,0u };
  glNamedBufferData(bufferID[EBUF], sizeof(window_quad_EB), window_quad_EB, GL_STATIC_DRAW);
  glVertexArrayElementBuffer(render_vao, bufferID[EBUF]);
  GLfloat window_quad_VB[16] = { -1.0f,+1.0f,0.0f,0.0f, +1.0f,+1.0f,1.0f,0.0f, +1.0f,-1.0f,1.0f,1.0f, -1.0f,-1.0f,0.0f,1.0f };
  glNamedBufferData(bufferID[VBUF], sizeof(window_quad_VB), window_quad_VB, GL_STATIC_DRAW);
  glVertexArrayVertexBuffer(render_vao, 0, bufferID[VBUF], 0, 16);
  glEnableVertexArrayAttrib(render_vao, 0);
  glVertexArrayAttribFormat(render_vao, 0, 4, GL_FLOAT, GL_FALSE, 0);
  glVertexArrayAttribBinding(render_vao, 0, 0);
  if (app_data.argc < 2)
    console::error("Expected 1 program argument: scene file missing");
  scene.translate_file(app_data.argv[1]);
  scene.regenerate_bufs();
  glNamedBufferData(bufferID[HEAP], sizeof(GLfloat)*scene.heap.size(), scene.heap.data(), GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufferID[HEAP]);
  glNamedBufferData(bufferID[GBUF], sizeof(GLint)*scene.gbuf.size(), scene.gbuf.data(), GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufferID[GBUF]);
  glNamedBufferData(bufferID[MBUF], sizeof(GLint)*scene.mbuf.size(), scene.mbuf.data(), GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bufferID[MBUF]);
  glNamedBufferData(bufferID[LBUF], sizeof(GLint)*scene.lbuf.size(), scene.lbuf.data(), GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, bufferID[LBUF]);
  glCreateTextures(GL_TEXTURE_2D, 1, &render_tex);
  glTextureParameteri(render_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(render_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTextureParameteri(render_tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTextureParameteri(render_tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTextureStorage2D(render_tex, 1, GL_RGBA32F, app_data.width, app_data.height);
  glBindImageTexture(0, render_tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
  glBindVertexArray(render_vao);
  cam = new PinholeCamera(glm::vec3(8.0f,5.0f,9.0f), glm::vec3(0.25f, 0.0f, 0.5f), 30.0, 0.66f);
  glUseProgram(ray_shader.handle);
  glUniform1i(ray_shader.loc("numShapes"), int(scene.geometry.size()));
  glUniform1i(ray_shader.loc("numLights"), int(scene.light.size()));
  glUniform3f(ray_shader.loc("cam.eye"), cam->eye.x, cam->eye.y, cam->eye.z);
  glUniform3f(ray_shader.loc("cam.across"), cam->across.x, cam->across.y, cam->across.z);
  glUniform3f(ray_shader.loc("cam.corner"), cam->corner.x, cam->corner.y, cam->corner.z);
  glUniform3f(ray_shader.loc("cam.up"), cam->up.x, cam->up.y, cam->up.z);
  glUseProgram(0);
  menu.build(&scene);
  console::log();
  menu.print(with_header);
}

void Ray_Tracer_App::on_event(SDL_Event e)
{
  if (e.type == SDL_KEYDOWN) {
    switch(e.key.keysym.sym) {
      // Menu Navigation
      case SDLK_RETURN:    menu.print(enter_input); break;
      case SDLK_RIGHT:     menu.print(right_input); break;
      case SDLK_LEFT:      menu.print(left_input);  break;
      case SDLK_BACKSPACE: menu.print(back_input);  break;
      case SDLK_UP:        menu.print(up_input);    break;
      case SDLK_DOWN:      menu.print(down_input);  break;
      // Other
      case SDLK_d: console::print_API_messages(); break;
      case SDLK_s: save_framebuffer_as_PNG();     break;
      default: break;
    }
  }
}

void Ray_Tracer_App::on_update()
{
  glUseProgram(ray_shader.handle);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, render_tex);
  glDispatchCompute(app_data.width, app_data.height, 1);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(render_shader.handle);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, render_tex);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
  SDL_GL_SwapWindow(sdl_app_data.p_window);
}

void Ray_Tracer_App::on_exit()
{
  glDeleteTextures(1, &render_tex);
  glDeleteVertexArrays(1, &render_vao);
  glDeleteBuffers(NUM_BUFFERS, bufferID);
  glDeleteProgram(render_shader.handle);
  glDeleteProgram(ray_shader.handle);
  console::log();
}

#include <algorithm>
#include "SDL_image.h"
void Ray_Tracer_App::save_framebuffer_as_PNG()
{
  int w = app_data.width, h = app_data.height;
  std::vector<GLubyte> raw_image(4*w*h);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, raw_image.data());
  for (int row = 0; row < h/2; ++row)
    std::swap_ranges(raw_image.begin()+4*w*row, raw_image.begin()+4*w*(row+1), raw_image.begin()+4*w*(h-row-1));
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(raw_image.data(), w, h, 32, w*4, SDL_PIXELFORMAT_RGBA32);
  if (!surface) console::log("Warning: failed to create SDL surface. ", SDL_GetError());
  std::string file = std::string("renders/") + console::date_time() + std::string(".png");
  if (IMG_SavePNG(surface, file.c_str())<0) console::log("\nWarning: failed to save png. ", IMG_GetError());
  SDL_FreeSurface(surface);
}



#include <sstream>
std::string Terminal_Menu::option_string()
{
  std::stringstream out;
  for (unsigned i = 0; i < context.current_state->c.size(); ++i)
    out << (context.current_state->cursor == i ? " > ":"   ") << context.states[context.current_state->c[i]]->name;
  return out.str();
}

std::string Terminal_Menu::directory_string()
{
  Menu_State *s = context.current_state;
  std::string out = s->name;
  while (s->pid != unsigned(-1))
    out = (s=context.states[s->pid])->name + std::string(" > ") + out;
  return out;
}

void Terminal_Menu::build(Scene_Interpreter *scene)
{
  context.create_state("Menu",    -1, { 1, 2, 3 });
  context.create_state("glContext",0, { });
  context.states[1]->description = console::GL_Context_info;
  context.create_state("scene",    0, { 4, 5, 6 });
  context.create_state("shader",   0, { 0 });
  context.create_state("geometry", 2, { });
  context.create_state("material", 2, { });
  context.create_state("light",    2, { });
  std::vector<std::vector<Scene_Object*>*> scene_object_containers = { &scene->geometry, &scene->material, &scene->light };
  int menu_pid = 4;
  for (auto container : scene_object_containers) {
    for (Scene_Object *o : *container) {
      auto o_id = context.create_state(o->name, menu_pid, {});
      for (Scene_Object_Variable *v : o->variable) {
        auto v_id = context.create_state(v->name, o_id, {});
        for (int i = 0; i < v->size; i++) {
          auto vc_id = context.create_state(std::to_string(scene->heap[v->index+i]), v_id, {});
          Menu_State *self = context.states[vc_id];
          this->context.states[vc_id]->modulate = [&, self, v, i, scene](MenuInputID e)
          {
            scene->heap[v->index+i] += (e==up_input)? +0.05f : (e==down_input)? -0.05f : 0.0f;
            glNamedBufferSubData(bufferID[HEAP], 0, sizeof(GLfloat)*scene->heap.size(), scene->heap.data());
            self->name = std::to_string(scene->heap[v->index+i]);
          };
        }
      }
    }
    menu_pid += 1;
  }
}

#include <iomanip>
void Terminal_Menu::print(MenuInputID e)
{
  context.input(e);
  if (e == enter_input || e == back_input || e == with_header)
    console::log("\033[F", std::setw(100), '\r', "$ ", directory_string());
  std::cout << '\r' << std::setw(100) << '\r' << option_string() << ' ' << std::flush;
}



void Scene_Interpreter::create_object(std::smatch &m)
{
  Scene_Object *o = new Scene_Object(m);
  switch(m[1].str()[0]) {
    case '$': o->material_index = std::stoi(m[4]);
              geometry.push_back(o); break;
    case '#': material.push_back(o); break;
    case '@': light.push_back(o);    break;
  }
  target = o;
}

void Scene_Interpreter::create_variable(std::smatch &m)
{
  unsigned idx, match_size;
  if (target == nullptr)
    return;
  for (idx = 3, match_size = 4; idx < m.size() && !m[idx].compare(""); idx += match_size--);
  target->variable.push_back(new Scene_Object_Variable(m[1].str(), heap.size(), match_size));
  for (unsigned i = 0; i < match_size; ++i)
    heap.push_back(std::stof(m[idx+i]));
}

#include <fstream>
void Scene_Interpreter::translate_file(std::string file_name)
{
  std::ifstream ifs(file_name);
  std::regex o_regex("([#$@]+)([A-z]+)\\s+(\\w+)\\s*([0-9]*)\\s*");
  std::regex v_regex("(\\w+)\\s+((-?[0-9.]+)\\s+(-?[0-9.]+)\\s+(-?[0-9.]+)\\s+(-?[0-9.]+)|(-?[0-9.]+)\\s+(-?[0-9.]+)\\s+(-?[0-9.]+)|\\s+(-?[0-9.]+)\\s+(-?[0-9.]+)|\\s+(-?[0-9.]+))\\s*");
  std::smatch o_match, v_match;
  for (std::string line; std::getline(ifs, line);) {
    if (std::regex_match(line, o_match, o_regex)) {
      this->create_object(o_match);
      while (std::getline(ifs, line)) {
        if (std::regex_match(line, v_match, v_regex))
          this->create_variable(v_match);
        else
          break;
      }
    }
  }
}

void Scene_Interpreter::regenerate_bufs()
{
  gbuf.clear();
  mbuf.clear();
  lbuf.clear();
  for (Scene_Object *o : geometry)
    gbuf.insert(gbuf.end(), { o->subtype, o->variable[0]->index, o->material_index, 0 });
  for (Scene_Object *o : material)
    mbuf.insert(mbuf.end(), { o->subtype, o->variable[0]->index });
  for (Scene_Object *o : light)
    lbuf.insert(lbuf.end(), { o->subtype, o->variable[0]->index });
}



int subtype_string_to_int(std::string s)
{
  return s == "plane"      ? 0
       : s == "sphere"     ? 1
       : s == "triangle"   ? 2
       : s == "diffuse"    ? 0
       : s == "phong"      ? 1
       : s == "glass"      ? 2
       : s == "mirror"     ? 3
       : s == "pointlight" ? 0
       : s == "spotlight"  ? 1
       : -1;
}

Scene_Object::Scene_Object(std::smatch& m)
: name(m[3].str()), subtype(subtype_string_to_int(m[2].str())), variable() {}

Scene_Object_Variable::Scene_Object_Variable(std::string s, unsigned i, unsigned z)
: name(s), index(i), size(z) {}



unsigned Menu_State_Context::create_state(std::string s, unsigned p, std::vector<unsigned> t)
{
  unsigned id = states.size();
  states.push_back(new Menu_State(s, id, p, t));
  states[id]->context = this;
  if (p < id && std::find(states[p]->c.begin(), states[p]->c.end(), id) == states[p]->c.end())
    states[p]->c.push_back(id);
  if (current_state == nullptr)
    current_state = states[0];
  return id;
}

void Menu_State_Context::input(MenuInputID e)
{
  if (current_state != nullptr)
    current_state = states[current_state->input(e)];
}



Menu_State::Menu_State(std::string s, MenuID mid, MenuID pid, std::vector<unsigned> &t)
: name(s), id(mid), pid(pid), cursor(0), c(t) {}

MenuID Menu_State::input(MenuInputID e)
{
  if (e == right_input && cursor < c.size()-1)
    cursor += 1;
  else if (e == left_input && cursor > 0)
    cursor -= 1;
  else if (e == down_input || e == up_input) {
    Menu_State *selected = context->states[c[cursor]];
    if (selected->modulate)
      selected->modulate.value()(e); 
  }
  else if (e == enter_input) {
    Menu_State *selected = context->states[c[cursor]];
    if (selected->c.empty()) {
      if (selected->description)
        console::log('\r',std::setw(100),"\r\033[F\r",std::setw(100),'\r',selected->description.value());
    }
    else return c[cursor];
  }
  else if (e == back_input)
    return pid;
  return id;
}


PinholeCamera::PinholeCamera(glm::vec3 eye, glm::vec3 target, float fov, float aspect)
{
  using namespace glm;
  this->eye = eye;
  this->target = target;
  top = tan(fov * .008726646f);
  right = aspect * top;
  vec3 W = normalize(eye - target);
  vec3 U = normalize(cross(vec3(0,1,0), W));
  vec3 V = 2.f * top * cross(W, U);
  across = 2.f * right * U;
  corner = eye - right * U - top * V - W;
  up = 2.f * top * V;
}