#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <boost/signals2.hpp>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>


namespace platform
{
    class window final {
    public:

        window(std::string_view name, std::int32_t width, std::int32_t height);

        ~window();

        void update(std::function<void()> &&callback);

        HWND handle() const noexcept { return glfwGetWin32Window(handle_); }

        struct event_handler_interface {
            virtual ~event_handler_interface() = default;

            virtual void on_resize(std::int32_t width, std::int32_t height) = 0;
        };

        void connect_event_handler(std::shared_ptr<event_handler_interface> handler);

    private:

        GLFWwindow *handle_;

        std::int32_t width_{0}, height_{0};

        std::string name_;

        boost::signals2::signal<void(std::int32_t, std::int32_t)> resize_callback_;

        void set_callbacks();
    };
}
