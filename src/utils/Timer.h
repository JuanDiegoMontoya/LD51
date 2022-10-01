#pragma once
#include <chrono>

// this timer is what you would use if you're not insane
class Timer
{
  using microsecond_t = std::chrono::duration<double, std::ratio<1, 1'000'000>>;
  using millisecond_t = std::chrono::duration<double, std::ratio<1, 1'000>>;
  using second_t = std::chrono::duration<double, std::ratio<1>>;
  using myclock_t = std::chrono::steady_clock;
  using timepoint_t = std::chrono::time_point<myclock_t>;
public:
  Timer()
  {
    timepoint_ = myclock_t::now();
  }

  void Reset()
  {
    timepoint_ = myclock_t::now();
  }

  double Elapsed_us() const
  {
    timepoint_t beg_ = timepoint_;
    return std::chrono::duration_cast<microsecond_t>(myclock_t::now() - beg_).count();
  }

  double Elapsed_ms() const
  {
    timepoint_t beg_ = timepoint_;
    return std::chrono::duration_cast<millisecond_t>(myclock_t::now() - beg_).count();
  }

  double Elapsed_s() const
  {
    timepoint_t beg_ = timepoint_;
    return std::chrono::duration_cast<second_t>(myclock_t::now() - beg_).count();
  }

private:
  timepoint_t timepoint_;
};