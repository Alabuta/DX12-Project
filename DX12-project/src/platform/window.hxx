#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <boost/signals2.hpp>

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>


namespace platform
{
    class window final {
    public:

        window(std::wstring_view name, std::int32_t width, std::int32_t height);

        ~window();

        HWND handle() noexcept { return handle_; }

        struct event_handler_interface {
            virtual ~event_handler_interface() = default;

            virtual void on_resize(std::int32_t width, std::int32_t height) = 0;
        };

        void connect_event_handler(std::shared_ptr<event_handler_interface> handler);

        void update(std::function<void()> &&callback);

    private:
        HWND handle_;

        std::int32_t width_{0}, height_{0};

        std::wstring name_;

        boost::signals2::signal<void(std::int32_t, std::int32_t)> resize_callback_;

        void set_callbacks();
    };
}
