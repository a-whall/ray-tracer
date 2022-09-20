#include "application.h"



std::stringstream console::GL_API_message_buffer;

std::string console::GL_Context_info;

#include <iomanip>
void console::print_API_messages()
{
  console::log("OpenGL API Messages""\n-------------------\n");
  if (console::GL_API_message_buffer.tellp() == std::streampos(0)) {
    console::log("No new messages.\n");
    return;
  }
  console::log('\r',std::setw(100),"\r\033[F\r",std::setw(100),'\r',console::GL_API_message_buffer.str());
  console::GL_API_message_buffer.str("");
}

#include <ctime>
std::string console::date_time()
{
  std::tm t;
  std::time_t now = std::time(nullptr);
  localtime_r(&now, &t);
  std::stringstream ss;
  ss << 1+t.tm_mon << '-' << t.tm_mday << '-' << t.tm_year-100 << ' '
    << t.tm_hour << '.' << t.tm_min << '.' << t.tm_sec;
  return ss.str();
}



#include <SDL_image.h>
void Application::init_SDL(const char* title, int x, int y, int w, int h, int flags)
{
  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)<0)
    console::error(SDL_GetError());
  if (IMG_Init(IMG_INIT_PNG)==0)
    console::error(IMG_GetError());
  sdl_app_data.p_window = SDL_CreateWindow(title, x, y, w, h, flags);
  sdl_app_data.p_key_states = SDL_GetKeyboardState(nullptr);
}

void Application::init_OGL() 
{
  int version_major(0), version_minor(0), profile(0), debug(0), double_buffer(0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
  sdl_app_data.context= SDL_GL_CreateContext(sdl_app_data.p_window);
  if (!sdl_app_data.context)
    console::error("Failed to create OpenGL context\n", SDL_GetError());
  if (!gladLoadGLLoader((GLADloadproc) SDL_GL_GetProcAddress))
    console::error("Unable to load OpenGL function pointers.");
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,  &profile);
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_FLAGS,         &debug);
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &version_major);
  SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &version_minor);
  SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER,          &double_buffer);
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback(callback, 0);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
  std::stringstream context_info;
  context_info
    << "GL Context Info""\n"
    << "---------------""\n"
    << "hardware vendor: " << glGetString(GL_VENDOR) << '\n'
    << "hardware renderer: " << glGetString(GL_RENDERER) << '\n'
    << "hardware driver version: " << glGetString(GL_VERSION) << '\n'
    << "current context version: " << version_major << '.' << version_minor
    << (profile&SDL_GL_CONTEXT_PROFILE_CORE?" (Core Profile)":profile&SDL_GL_CONTEXT_PROFILE_COMPATIBILITY?" (Compatibility Profile)":" (ES Profile)")
    << (debug&SDL_GL_CONTEXT_DEBUG_FLAG?"\n""debug output is enabled":"")
    << (double_buffer?"\n""default framebuffer is double-buffered":"") << '\n';
  console::GL_Context_info = context_info.str();
}

void Application::init(int argc, char* argv[], int w, int h)
{
  this->app_data.argc= argc;
  this->app_data.argv= argv;
  this->app_data.width= w;
  this->app_data.height= h;
  this->init_SDL(argv[0], SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
  this->init_OGL();
  this->on_init();
}

bool Application::is_running()
{
  return app_data.running;
}

void Application::step()
{
  Uint32 delta_time, frame_begin = SDL_GetTicks();
  SDL_Event event;
  while (SDL_PollEvent(&event))
    if (event.type == SDL_QUIT)
      app_data.running = false;
    else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
      app_data.width = event.window.data1;
      app_data.height = event.window.data2; }
    else
      this->on_event(event);
  this->on_update();
  delta_time = SDL_GetTicks() - frame_begin;
  if (delta_time < app_data.ms_per_frame)
    SDL_Delay(app_data.ms_per_frame - delta_time);
  app_data.cma_fdt += (delta_time - app_data.cma_fdt) / (++app_data.frame_count);
}

void Application::exit()
{
  this->on_exit();
  SDL_GL_DeleteContext(sdl_app_data.context);
  SDL_DestroyWindow(sdl_app_data.p_window);
  IMG_Quit();
  SDL_Quit();
}

void Application::on_init() {}

void Application::on_event(SDL_Event) {}

void Application::on_update() {}

void Application::on_exit() {}



void callback(GLenum debug_source, GLenum, GLuint, GLenum, GLsizei, const GLchar *msg, const void *) {
  if (debug_source == GL_DEBUG_SOURCE_API)
    console::GL_API_message_buffer << ' ' << msg;
}



#include <fstream>
void Shader::source(const char *path, bool) // bool remake
{
  int shader_index(0);
  std::ifstream ifs(path);
  std::stringstream ss[5];
  std::string line;
  this->file_path = path;
  auto line_contains = [&line](std::string phrase) { return line.find(phrase) != std::string::npos; };
  getline(ifs, line);
  if (!line_contains("#shader"))
    console::error("shader files must start with a #shader directive");
  while(getline(ifs, line))
    if (line_contains("#shader") || line_contains("#end")) {
      auto source_str = ss[shader_index].str();
      const char* source_code = source_str.c_str();
      glShaderSource(shaderHandles[shader_index++], 1, &source_code, nullptr); }
    else
      ss[shader_index] << line << '\n';
}

void Shader::compile()
{
  int idx(0);
  for (GLuint handle : shaderHandles) {
    glCompileShader(handle);
    GLint status_errno(0), log_length(0);
    glGetShaderiv(handle, GL_COMPILE_STATUS, &status_errno);
    if (status_errno == 0) {
      glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
      GLchar *error_log = new GLchar[log_length];
      glGetShaderInfoLog(handle, log_length, nullptr, error_log);
      error_log[strcspn(error_log,"\n")] = 0;
      console::log(GLenum_string(shaderTypes[idx++]), " unit compilation failed""\n", this->file_path,'\n', error_log);
      delete[] error_log; }
    glAttachShader(this->handle, handle); }
}

void Shader::link()
{
  glLinkProgram(this->handle);
  GLint status_errno(0);
  glGetProgramiv(this->handle, GL_LINK_STATUS, &status_errno);
  if (status_errno == 0) {
    GLint log_length(0);
    glGetProgramiv(this->handle, GL_INFO_LOG_LENGTH, &log_length);
    GLchar *error_log = new GLchar[log_length];
    glGetProgramInfoLog(this->handle, log_length, nullptr, error_log);
    error_log[strcspn(error_log,"\0") - 1] = 0;
    console::error("Failed to link program ",this->handle," (",this->file_path,")\n\n",error_log,'\n');
    delete[] error_log; }
  for (GLuint handle : shaderHandles) {
    glDetachShader(this->handle, handle);
    glDeleteShader(handle); }
}

int Shader::loc(const char *name)
{
  if (uniform_location_map.find(name) != uniform_location_map.end())
    return uniform_location_map[name];
  else if (nonexistent_uniform_set.find(name) != nonexistent_uniform_set.end())
    return -1;
  int location = glGetUniformLocation(handle, name);
  if (location == -1) {
    nonexistent_uniform_set.insert(name);
    console::log("\nWarning: \"", name, "\" is not an active uniform of program ", handle, ", it doesn't exist or the OpenGL Compiler has optimized it out."); }
  else
    uniform_location_map[name] = location;
  return location;
}

#include <iomanip>
void Shader::log_resource(GLenum requested_property)
{
  auto print_active = [&](std::vector<GLenum> properties)
  {
    int num_tokens;
    glGetProgramInterfaceiv(this->handle, requested_property, GL_ACTIVE_RESOURCES, &num_tokens);
    console::log(
      GLenum_string(requested_property), ":\n",
      std::setw(5), " index ", std::setw(9), " name ", std::setw(40), " type ");
    for(int i = 0; i < num_tokens; ++i) {
      int *results = new int[properties.size()];
      glGetProgramResourceiv(this->handle, requested_property, i, properties.size(), properties.data(), properties.size(), NULL, results);
      int size = results[0] + 1;
      char *name = new char[size];
      glGetProgramResourceName(this->handle, requested_property, i, size, NULL, name);
      console::log(' ', std::setw(4), results[2], "       ", std::left, std::setw(40), name, GLenum_string(results[1]), std::right);
      delete[] results;
      delete[] name;
    }
  };
  switch(requested_property) {
    case GL_BUFFER_VARIABLE: console::log(" TODO: implement gl buffer variable debug output"); break;
    case GL_PROGRAM_INPUT: print_active({GL_NAME_LENGTH,GL_TYPE,GL_LOCATION}); break;
    case GL_UNIFORM: print_active({GL_NAME_LENGTH,GL_TYPE,GL_LOCATION,GL_BLOCK_INDEX}); break;
  }
}

void Shader::log_program_resources()
{
  std::string stages_string= "";
  for (GLint i : shaderTypes)
    stages_string.append(GLenum_string(i).append("  "));
  console::log("-- shader resource report --\n"
    " ID: ", this->handle, "\n"
    " source: ", this->file_path, "\n"
    " stages: ", stages_string);
  this->log_resource(GL_PROGRAM_INPUT);
  this->log_resource(GL_UNIFORM);
}



std::string GLenum_string(GLenum e) {
  switch (e) {
    // types
    case GL_INT:               return "int";
    case GL_INT_VEC2:          return "ivec2";
    case GL_INT_VEC3:          return "ivec3";
    case GL_INT_VEC4:          return "ivec4";
    case GL_FLOAT:             return "float";
    case GL_FLOAT_VEC2:        return "vec2";
    case GL_FLOAT_VEC3:        return "vec3";
    case GL_FLOAT_VEC4:        return "vec4";
    case GL_FLOAT_MAT2:        return "mat2";
    case GL_FLOAT_MAT3:        return "mat3";
    case GL_FLOAT_MAT4:        return "mat4";
    case GL_DOUBLE:            return "double";
    case GL_DOUBLE_MAT2:       return "dmat2";
    case GL_DOUBLE_MAT3:       return "dmat3";
    case GL_DOUBLE_MAT4:       return "dmat4";
    case GL_UNSIGNED_INT:      return "uint";
    case GL_UNSIGNED_INT_VEC2: return "uvec2";
    case GL_UNSIGNED_INT_VEC3: return "uvec3";
    case GL_UNSIGNED_INT_VEC4: return "uvec4";
    case GL_BOOL:              return "bool";
    case GL_BOOL_VEC2:         return "bvec2";
    case GL_BOOL_VEC3:         return "bvec3";
    case GL_BOOL_VEC4:         return "bvec4";
    case GL_SAMPLER_2D:        return "sampler2D";
    case GL_SAMPLER_2D_ARRAY:  return "sampler2DArray";
    case GL_IMAGE_2D:          return "image2D";
    // program resources
    case GL_PROGRAM_INPUT: return"program input";
    case GL_UNIFORM:       return"uniform";
    // debug
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:     return "OpenGL Window System Message";
    case GL_DEBUG_SOURCE_API:               return "OpenGL API Message";
    case GL_DEBUG_SOURCE_SHADER_COMPILER:   return "OpenGL Shader Compiler Message";
    case GL_DEBUG_SOURCE_OTHER:             return "OpenGL Message";
    case GL_DEBUG_TYPE_ERROR:               return "error: ";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "warning: ";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  return "warning: ";
    case GL_DEBUG_TYPE_PORTABILITY:         return "warning: ";
    case GL_DEBUG_TYPE_PERFORMANCE:         return "warning: ";
    case GL_DEBUG_TYPE_MARKER:              return "warning: ";
    case GL_DEBUG_TYPE_PUSH_GROUP:          return "warning: ";
    case GL_DEBUG_TYPE_POP_GROUP:           return "warning: ";
    case GL_DEBUG_TYPE_OTHER:               return "";
    // shader types
    case GL_FRAGMENT_SHADER:        return "fragment";
    case GL_VERTEX_SHADER:          return "vertex";
    case GL_GEOMETRY_SHADER:        return "geometry";
    case GL_TESS_CONTROL_SHADER:    return "tessellation-control";
    case GL_TESS_EVALUATION_SHADER: return "tessellation-evaluation";
    case GL_COMPUTE_SHADER:         return "compute";
    default: return "???";
  }
}



#include <regex>
void parseOBJ(const char* file) {
  std::ifstream ifs(file);
  std::stringstream ss;
  std::string line;
  std::regex v ("v ([-]?[0-9.]*) ([-]?[0-9.]*) ([-]?[0-9.]*)");
  std::regex f ("f ([0-9]*) ([0-9]*) ([0-9]*)");
  std::smatch sm;
  std::vector<glm::vec3> vertex_data;
  std::vector<glm::uvec3> face_data;
  getline(ifs, line); // consume top line
  while(getline(ifs, line)) {
    if (std::regex_match(line, sm, v))
      vertex_data.push_back(glm::vec3(std::stof(sm[1]), std::stof(sm[2]), std::stof(sm[3])));
    else if (std::regex_match(line, sm, f))
      face_data.push_back(glm::uvec3(std::stoi(sm[1]), std::stoi(sm[2]), std::stoi(sm[3])));
  }
  console::log("vertex data size: ", vertex_data.size());
  console::log("face data size: ", face_data.size());
}