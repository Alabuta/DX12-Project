#pragma once

#define _SCL_SECURE_NO_WARNINGS

#if defined(_DEBUG) || defined(DEBUG)
    #define _CRTDBG_MAP_ALLOC
    #include <crtdbg.h>
#endif

#include <algorithm>
#include <execution>
#include <exception>
#include <iostream>
#include <vector>

#include <string>
using namespace std::string_literals;

#include <string_view>
using namespace std::string_view_literals;

#include <fmt/format.h>

#include <wrl.h>
#include <dxgi.h>
#include <d3d12.h>
