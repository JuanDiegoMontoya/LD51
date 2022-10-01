#include "utils/LoadFile.h"
#include "Exception.h"
#include <fstream>

std::string LoadFile(std::string_view path)
{
  std::ifstream file{ path.data() };
  if (file.fail()) // empty files are acceptable, missing/bad files are not
  {
    throw LoadFileException(path.data());
  }
  return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}