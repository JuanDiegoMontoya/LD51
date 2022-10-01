#include "System.h"
#include "ecs/Scene.h"

namespace ecs
{
  System::System(Scene* scene, EventBus* eventBus)
    : _scene(scene),
      _eventBus(eventBus)
  {
  }

  System::~System()
  {
  }
}