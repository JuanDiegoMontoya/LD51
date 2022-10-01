#include "Input.h"
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>

namespace input
{
  class InputManager::InputAccess
  {
  public:

    static void WindowFocusCallback(GLFWwindow* window, int focused)
    {
      ImGui_ImplGlfw_WindowFocusCallback(window, focused);
    }

    static void CursorEnterCallback(GLFWwindow* window, int entered)
    {
      ImGui_ImplGlfw_CursorEnterCallback(window, entered);
    }

    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
      ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    }

    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
      ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

      auto* pInput = reinterpret_cast<InputManager*>(glfwGetWindowUserPointer(window));
      pInput->scrollOffset = yoffset;

      // dispatch all action events for scrolling in this direction
      for (auto&& [actionInput, dispatcher] : pInput->_actionBindings)
      {
        if (const auto* pScroll = std::get_if<MouseScroll>(&actionInput.type))
        {
          if ((yoffset >= 0 && !pScroll->down) || (yoffset < 0 && pScroll->down))
          {
            dispatcher->DispatchEvent(pInput->_eventBus);
          }
        }
      }
    }

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
      ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);

      auto* pInput = reinterpret_cast<InputManager*>(glfwGetWindowUserPointer(window));

      // TODO: assert that key is not GLFW_KEY_UNKNOWN
      switch (action)
      {
      case GLFW_RELEASE: pInput->_buttonStates[key] = ButtonState::RELEASED; break;
      case GLFW_PRESS: pInput->_buttonStates[key] = ButtonState::PRESSED; break;
      case GLFW_REPEAT: pInput->_buttonStates[key] = ButtonState::DOWN; break;
      default: break;
      }

      // dispatch all action events for this key
      if (action == GLFW_PRESS)
      {
        for (auto&& [actionInput, dispatcher] : pInput->_actionBindings)
        {
          if (const auto* pButton = std::get_if<Button>(&actionInput.type); pButton && *pButton == static_cast<Button>(key))
          {
            dispatcher->DispatchEvent(pInput->_eventBus);
          }
        }
      }
    }

    static void CharCallback(GLFWwindow* window, unsigned c)
    {
      ImGui_ImplGlfw_CharCallback(window, c);
    }

    static void MonitorCallback(GLFWmonitor* monitor, int event)
    {
      ImGui_ImplGlfw_MonitorCallback(monitor, event);
    }

    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
    {
      auto* pInput = reinterpret_cast<InputManager*>(glfwGetWindowUserPointer(window));
      pInput->cursorPosX = xpos;
      pInput->cursorPosY = ypos;
    }
  };

  InputManager::InputManager(GLFWwindow* window, EventBus* eventBus)
    : _window(window),
      _eventBus(eventBus)
  {
    for (auto& state : _buttonStates)
    {
      state = ButtonState::UP;
    }

    glfwSetWindowFocusCallback(window, InputAccess::WindowFocusCallback);
    glfwSetCursorEnterCallback(window, InputAccess::CursorEnterCallback);
    glfwSetMouseButtonCallback(window, InputAccess::MouseButtonCallback);
    glfwSetScrollCallback(window, InputAccess::ScrollCallback);
    glfwSetKeyCallback(window, InputAccess::KeyCallback);
    glfwSetCharCallback(window, InputAccess::CharCallback);
    glfwSetMonitorCallback(InputAccess::MonitorCallback);

    glfwSetWindowUserPointer(window, this);
  }

  void InputManager::PollEvents([[maybe_unused]] double dt)
  {
    for (auto& state : _buttonStates)
    {
      if (state == ButtonState::RELEASED) { state = ButtonState::UP; }
      if (state == ButtonState::PRESSED) { state = ButtonState::DOWN; }
    }
    scrollOffset = 0;
    
    // this is where action events would get dispatched, if there are any
    glfwPollEvents();

    // dispatch axis events
    for (auto&& [axis, dispatcher] : _axisBindings)
    {
      if (const auto* pButton = std::get_if<Button>(&axis.type))
      {
        auto state = _buttonStates[static_cast<std::size_t>(*pButton)];
        if (state == ButtonState::DOWN || state == ButtonState::PRESSED)
        {
          dispatcher->DispatchEvent(_eventBus, axis.scale);
        }
      }

      if (scrollOffset != 0)
      {
        if (const auto* pScroll = std::get_if<MouseScroll>(&axis.type))
        {
          dispatcher->DispatchEvent(_eventBus, axis.scale * static_cast<float>(scrollOffset) * (pScroll->down ? -1.0f : 1.0f));
        }
      }

      // TODO: add mouse position axis event
    }
  }
}