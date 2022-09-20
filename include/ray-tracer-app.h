#pragma once
#include "application.h"


typedef unsigned MenuID;


enum MenuInputID
{
  null_input,
  enter_input,
  right_input,
  left_input,
  back_input,
  down_input,
  up_input,
  with_header
};


#include <optional>
#include <functional>
struct Menu_State_Context;
struct Menu_State
{
  Menu_State_Context *context;
  std::string name;
  MenuID id, pid;
  unsigned cursor;
  std::vector<MenuID> c;
  std::optional<std::string> description;
  std::optional<std::function<void(MenuInputID)>> modulate;

  MenuID input(MenuInputID);
  Menu_State(std::string, MenuID, MenuID, std::vector<MenuID> &t);
};


struct Menu_State_Context
{
  std::vector<Menu_State *> states;
  Menu_State *current_state = nullptr;

  MenuID create_state(std::string, unsigned, std::vector<MenuID>);
  void input(MenuInputID);
};


struct Scene_Object_Variable
{
  std::string name;
  int index, size;

  Scene_Object_Variable(std::string, unsigned, unsigned);
};


#include <regex>
struct Scene_Object
{
  std::string name;
  int subtype, material_index;
  std::vector<Scene_Object_Variable*> variable;

  Scene_Object(std::smatch&);
};


class Scene_Interpreter
{
  void create_object(std::smatch&);
  void create_variable(std::smatch&);
public:
  std::vector<Scene_Object*> geometry;
  std::vector<Scene_Object*> material;
  std::vector<Scene_Object*> light;
  std::vector<float> heap;
  std::vector<int> gbuf;
  std::vector<int> mbuf;
  std::vector<int> lbuf;
  Scene_Object *target = nullptr;

  void translate_file(std::string);
  void regenerate_bufs();
};


class Terminal_Menu
{
  Menu_State_Context context;
  std::string option_string();
  std::string directory_string();
public:
  void build(Scene_Interpreter*);
  void print(MenuInputID=null_input);
};


class Ray_Tracer_App : public Application
{
  Scene_Interpreter scene;
  Terminal_Menu menu;
protected:
  void on_init() override;
  void on_event(SDL_Event) override;
  void on_update() override;
  void on_exit() override;
public:
  void save_framebuffer_as_PNG();
};