#pragma once
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <glad.h>
#include <glm.hpp>

struct App_Data
{
  int argc;
  char** argv;
  int width;
  int height;
  int fps = 60;
  float ms_per_frame = 1000.0f/fps;
  unsigned frame_count = 0;
  float cma_fdt = 0.0f; // cumulative-moving-average of frame-delta-time
  bool running = true;
};


struct SDL_App_Data
{
  SDL_Window *p_window = nullptr;
  SDL_GLContext context = nullptr;
  const Uint8 *p_key_states = nullptr;
};


class Application
{
  void init_SDL(const char *, int, int, int, int, int);
  void init_OGL();
public:
  void init(int, char**, int, int);
  bool is_running();
  void step();
  void exit();
protected:
  App_Data app_data;
  SDL_App_Data sdl_app_data;

  virtual void on_init();
  virtual void on_event(SDL_Event);
  virtual void on_update();
  virtual void on_exit();
};


#include <sstream>
#include <iostream>
namespace console
{
  template <std::ostream& stream = std::cout, typename ... Args>
  void log(Args&&... args)
  {
    (stream << ... << args) << '\n';
  }

  template <typename ... Args>
  void error(Args&&... args)
  {
    log<std::cerr>("Error: ", std::forward<Args>(args)...);
  }

  extern std::stringstream GL_API_message_buffer;
  extern std::string GL_Context_info;

  void print_API_messages();
  std::string date_time();
}


std::string GLenum_string(GLenum e);

extern void GLAPIENTRY callback(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *, const void *);


#include <set>
#include <vector>
#include <unordered_map>
class Shader
{
  std::string file_path;
  std::vector<GLint> shaderTypes;
  std::vector<GLuint> shaderHandles;
  std::unordered_map<const char *, int> uniform_location_map;
  std::set<const char *> nonexistent_uniform_set;
  void log_resource(GLenum);
  const char * stages();
public:
  GLuint handle;

  template<typename ... Args>
  void create(Args&&... shader_types) {
    shaderHandles.clear();
    shaderTypes = { shader_types... };
    for (auto shaderT : shaderTypes) {
      shaderHandles.push_back(glCreateShader(shaderT));
    }
  }
  void source(const char *, bool = false);
  void compile();
  void link();
  void log_program_resources();
  int loc(const char *);
};



void log(const int shader_id, GLenum requested_property);



void parseOBJ(const char*);