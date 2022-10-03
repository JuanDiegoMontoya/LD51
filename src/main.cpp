#include "Application.h"
#include "ecs/Scene.h"
#include "utils/EventBus.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int main(int, const char* const*)
{
  EventBus eventBus;
  auto scene = ecs::Scene(&eventBus);
  auto app = Application("Flocker", &scene, &eventBus);
  app.Run();

  return 0;
}