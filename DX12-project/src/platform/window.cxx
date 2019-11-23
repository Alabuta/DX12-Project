#include <stdexcept>

using namespace std::string_literals;

#include <fmt/format.h>

#include "window.hxx"

namespace platform
{
    window::window(std::wstring_view name, std::int32_t width, std::int32_t height)
        : handle_{nullptr}, width_{width}, height_{height}, name_{name}
    {
        auto hInstance = GetModuleHandleW(nullptr);

        auto class_name = L"CLASS_NAME_"s + std::wstring{name};

        WNDCLASSEXW const window_class{
            sizeof(WNDCLASSEXW),
            CS_OWNDC,
            static_cast<WNDPROC>(static_callback),
            0, 0,
            hInstance,
            nullptr,
            LoadCursorW(hInstance, IDC_ARROW),
            CreateSolidBrush(RGB(41, 34, 37)), nullptr,
            class_name.c_str(),
            nullptr
        };

        if (RegisterClassExW(&window_class) == 0)
            throw std::runtime_error("failed to register window class"s);

        handle_ = CreateWindowExW(0, window_class.lpszClassName, name_.c_str(), WS_POPUP | WS_SYSMENU,
                                  0, 0, width, height, nullptr, nullptr, hInstance, reinterpret_cast<LPVOID>(this));

        if (handle_ == nullptr)
            throw std::runtime_error("failed to create window"s);

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

            UnregisterClassW((L"CLASS_NAME_"s + name_).c_str(), GetModuleHandleW(nullptr));

            handle_ = nullptr;
        }
    }

    LRESULT CALLBACK window::static_callback(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg) {
            case WM_NCCREATE:
                auto create_info = reinterpret_cast<LPCREATESTRUCT>(lParam);
                auto instance = reinterpret_cast<window *>(create_info->lpCreateParams);

                window_tables_.insert_or_assign(hWnd, instance);
                return TRUE;
        }

        if (auto window = window_tables_[hWnd]; window)
            return window->process(hWnd, msg, wParam, lParam);

        else return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK window::process(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg) {
            case WM_ERASEBKGND:
                return 1L;

            /*case WM_NCPAINT:
                if (!IsWindowed())
                    return 0L;
                break;*/

            case WM_SIZE:
                resize_callback_(LOWORD(lParam), HIWORD(lParam));
                return 0L;

            /*case WM_CLOSE:
                Window::Destroy();
                return 0L;*/

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0L;
        }

        return DefWindowProcW(hWnd, msg, wParam, lParam);
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
        MSG msg;

    #pragma warning(push, 3)
    #pragma warning(disable: 4127)
        while (true) {
    #pragma warning(pop)
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE /*| PM_NOYIELD*/) != 0) {
                if (msg.message == WM_QUIT)
                    return;// static_cast<std::int32_t>(msg.wParam);

                //TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            //if (Window::main().InFocus())
                callback();

            //else WaitMessage();
        }
    }
}
