#pragma once
//#include <source_location>
#include <cassert>

#ifndef NDEBUG
#define G_ASSERT(x) assert(x)
#define G_ASSERT_MSG(x, msg) assert(x)
#define G_UNREACHABLE assert(false)
#else
#define G_ASSERT(x)
#define G_ASSERT_MSG(x, msg)
#define G_UNREACHABLE
#endif

//namespace g
//{
//  void Assert(bool condition, std::source_location = std::source_location::current());
//}