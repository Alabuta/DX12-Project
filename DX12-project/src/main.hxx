#pragma once

#define _SCL_SECURE_NO_WARNINGS

#if defined(_DEBUG) || defined(DEBUG)
    #define _CRTDBG_MAP_ALLOC
    #include <crtdbg.h>
#endif

#include <execution>
#include <exception>

#include <fmt/format.h>

#include <wrl.h>
