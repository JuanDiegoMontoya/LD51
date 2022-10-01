#pragma once

namespace ecs
{
  struct Lifetime
  {
    int microsecondsLeft;
  };

  struct DeleteNextTick {};

  struct DeleteInNTicks
  {
    int ticks;
  };
}