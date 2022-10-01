#pragma once

namespace ecs
{
  // It is required to derive from this struct in order to create a new axis binding.
  // This struct must be visible to the server.
  struct AxisBindingBase
  {
    float magnitude = 0;
  };
}