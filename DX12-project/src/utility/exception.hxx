#pragma once

#include <exception>
#include <string>


namespace dx
{
    struct com_exception : public std::exception {
        explicit com_exception(std::string const &what_arg) : std::exception(what_arg.c_str()) { }
    };

    struct dxgi_factory : public std::exception {
        explicit dxgi_factory(std::string const &what_arg) : std::exception(what_arg.c_str()) { }
    };

    struct device_error : public std::exception {
        explicit device_error(std::string const &what_arg) : std::exception(what_arg.c_str()) { }
    };

    struct swapchain : public std::exception {
        explicit swapchain(std::string const &what_arg) : std::exception(what_arg.c_str()) { }
    };
}

