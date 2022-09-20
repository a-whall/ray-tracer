#include "ray-tracer-app.h"

int main(int argc, char* argv[])
{
  Ray_Tracer_App app;
  app.init(argc, argv, 960, 640);
  while (app.is_running())
    app.step();
  app.exit();
}