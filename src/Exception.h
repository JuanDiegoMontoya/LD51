#pragma once
#include <exception>
#include <string>
#include <utility>

class Exception : public std::exception
{
public:
  Exception() {}
  Exception(std::string message)
    : message_(std::move(message))
  {
  }

  const char* what() const noexcept override
  {
    return message_.c_str();
  }

protected:
  std::string message_;
};

class LoadFileException : public Exception
{
public:
  LoadFileException(std::string path)
    : Exception("Failed to load file: " + path)
  {
  }
};