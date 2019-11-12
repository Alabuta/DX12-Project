#include <stdexcept>

using namespace std::string_literals;

#include <fmt/format.h>

#include "window.hxx"


namespace
{
    auto constexpr kCLASS_NAME = L"vulkan-island-window-class";

    LRESULT CALLBACK static_callback(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        /*if (auto window = windowsTable[hWnd]; window)
            return window->Process(hWnd, msg, wParam, lParam);

        else */return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

namespace platform
{
    window::window(std::wstring_view name, std::int32_t width, std::int32_t height)
        : handle_{nullptr}, width_{width}, height_{height}, name_{name}
    {
        auto hInstance = GetModuleHandleW(nullptr);

        WNDCLASSEXW const window_class{
            sizeof(WNDCLASSEXW),
            CS_OWNDC,
            static_cast<WNDPROC>(static_callback),
            0, 0,
            hInstance,
            nullptr,
            LoadCursorW(hInstance, IDC_ARROW),
            CreateSolidBrush(RGB(41, 34, 37)), nullptr,
            kCLASS_NAME,
            nullptr
        };

        if (RegisterClassExW(&window_class) == 0)
            throw std::runtime_error("failed to register window class"s);

        handle_ = CreateWindowExW(0, window_class.lpszClassName, name_.c_str(), WS_POPUP | WS_SYSMENU,
                                  0, 0, width, height, nullptr, nullptr, hInstance, nullptr);

        if (handle_ == nullptr)
            throw std::runtime_error("failed to create window"s);

        set_callbacks();

        SetWindowLongPtrW(handle_, GWL_EXSTYLE, WS_EX_WINDOWEDGE);
        SetWindowLongPtrW(handle_, GWL_STYLE, WS_TILEDWINDOW /*^ (WS_THICKFRAME | WS_CAPTION)*/);

        RECT rc;

        {
            GetWindowRect(handle_, &rc);

            // Primary desktop work area rectangle.
            RECT rcArea;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &rcArea, 0);

            // Adjust rectangle of the window to new desktop rectangle.
            rc.right = (rc.right -= rc.left) > (rcArea.right -= rcArea.left) ? rcArea.right : rc.right;
            rc.bottom = (rc.bottom -= rc.top) > (rcArea.bottom -= rcArea.top) ? rcArea.bottom : rc.bottom;

            // The left-top frame corner of the window.
            if (rc.left == 0) rc.left = ((rcArea.right - rc.right) >> 1) + rcArea.left;
            if (rc.top == 0) rc.top = ((rcArea.bottom - rc.bottom) >> 1) + rcArea.top;
        }

        SetWindowPos(handle_, nullptr, rc.left, rc.top, rc.right, rc.bottom, SWP_DRAWFRAME | SWP_NOZORDER);

        ShowWindow(handle_, SW_SHOW);
        SetForegroundWindow(handle_);
    }

    window::~window()
    {
        if (handle_) {
            DestroyWindow(handle_);

            UnregisterClassW(kCLASS_NAME, GetModuleHandleW(nullptr));

            handle_ = nullptr;
        }
    }

    void window::connect_event_handler(std::shared_ptr<event_handler_interface> handler)
    {
        resize_callback_.connect(decltype(resize_callback_)::slot_type(
            &event_handler_interface::on_resize, handler.get(), _1, _2
        ).track_foreign(handler));
    }

    void window::update(std::function<void()> &&callback)
    {
        /*while (glfwWindowShouldClose(handle_) == GLFW_FALSE && glfwGetKey(handle_, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
            callback();
        }*/
    }

    void window::set_callbacks()
    {
        /*glfwSetWindowSizeCallback(handle_, [] (auto handle, auto width, auto height)
        {
            auto instance = reinterpret_cast<window *>(glfwGetWindowUserPointer(handle));

            if (instance) {
                instance->width_ = width;
                instance->height_ = height;

                instance->resize_callback_(width, height);
            }
        });*/
    }
}
