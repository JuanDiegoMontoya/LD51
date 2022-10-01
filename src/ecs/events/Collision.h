#pragma once
#include <utils/EventBus.h>
#include <ecs/Entity.h>

namespace ecs
{
  struct Collision
  {
    Entity entity0;
    Entity entity1;
  };
}