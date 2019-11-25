#include <stdexcept>
using namespace std::string_literals;

#include <fmt/format.h>

#include "window.hxx"


namespace platform
{
    window::window(std::string_view name, std::int32_t width, std::int32_t height)
        : handle_{nullptr}, width_{width}, height_{height}, name_{name}
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        handle_ = glfwCreateWindow(width_, height_, name_.c_str(), nullptr, nullptr);

        if (handle_ == nullptr)
            throw std::runtime_error(fmt::format("failed to create '{}' window"s, name_));

        glfwSetWindowUserPointer(handle_, this);

        set_callbacks();
    }

    window::~window()
    {
        if (handle_)
            glfwDestroyWindow(handle_);
    }

    void window::connect_event_handler(std::shared_ptr<event_handler_interface> handler)
    {
        resize_callback_.connect(decltype(resize_callback_)::slot_type(
            &event_handler_interface::on_resize, handler.get(), _1, _2
        ).track_foreign(handler));
    }

    void window::update(std::function<void()> &&callback)
    {
        while (glfwWindowShouldClose(handle_) == GLFW_FALSE && glfwGetKey(handle_, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
            glfwPollEvents();

            callback();
        }
    }

    void window::set_callbacks()
    {
        glfwSetWindowSizeCallback(handle_, [] (auto handle, auto width, auto height)
        {
            auto instance = reinterpret_cast<window *>(glfwGetWindowUserPointer(handle));

            if (instance) {
                instance->width_ = width;
                instance->height_ = height;

                instance->resize_callback_(width, height);
            }
        });
    }
}
