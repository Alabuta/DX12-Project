#pragma once

#include <exception>

#include <string>
using namespace std::string_literals;

#include <wrl/client.h>
#include <dxgi1_5.h>
#include <d3d12.h>

#include <fmt/format.h>


namespace dx
{
    struct com_exception : public std::exception {
        explicit com_exception(std::string const &what_arg) : std::exception(what_arg.c_str()) { }
    };

    struct dxgi_factory : public std::exception {
        explicit dxgi_factory(std::string const &what_arg) : std::exception(what_arg.c_str()) { }
    };
}

