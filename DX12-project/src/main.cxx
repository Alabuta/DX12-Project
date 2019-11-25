#include "main.hxx"
#include "utility/exception.hxx"
#include "platform/window.hxx"

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D12.lib")


namespace graphics
{
    struct extent final {
        std::uint32_t width{0}, height{0};
    };
}


winrt::com_ptr<IDXGIAdapter1> pick_hardware_adapter(IDXGIFactory4 *const dxgi_factory)
{
    std::vector<winrt::com_ptr<IDXGIAdapter1>> adapters(16);

    std::generate(std::begin(adapters), std::end(adapters), [dxgi_factory, i = 0u] () mutable
    {
        winrt::com_ptr<IDXGIAdapter1> adapter;

        dxgi_factory->EnumAdapters1(i++, adapter.put());

        return adapter;
    });

    auto it = std::remove_if(std::begin(adapters), std::end(adapters), [] (auto &&adapter) { return adapter == nullptr; });

    adapters.erase(it, std::end(adapters));

    for (auto &&adapter : adapters) {
        DXGI_ADAPTER_DESC1 description;
        adapter->GetDesc1(&description);

        std::wcout << L"Adapter "s + description.Description << std::endl;
    }

    it = std::remove_if(std::begin(adapters), std::end(adapters), [] (auto &&adapter)
    {
        DXGI_ADAPTER_DESC1 description;
        adapter->GetDesc1(&description);

        if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
            return true;

        if (auto result = D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr); FAILED(result))
            return true;

        return description.DedicatedVideoMemory == 0;
    });

    adapters.erase(it, std::end(adapters));

    std::sort(std::begin(adapters), std::end(adapters), [] (auto &&lhs, auto &&rhs)
    {
        DXGI_ADAPTER_DESC1 lhs_description;
        lhs->GetDesc1(&lhs_description);

        DXGI_ADAPTER_DESC1 rhs_description;
        rhs->GetDesc1(&rhs_description);

        if (lhs_description.DedicatedVideoMemory != rhs_description.DedicatedVideoMemory)
            return lhs_description.DedicatedVideoMemory > rhs_description.DedicatedVideoMemory;

        if (lhs_description.DedicatedSystemMemory != rhs_description.DedicatedSystemMemory)
            return lhs_description.DedicatedSystemMemory > rhs_description.DedicatedSystemMemory;

        return lhs_description.SharedSystemMemory > rhs_description.SharedSystemMemory;
    });

    if (adapters.empty())
        throw dx::dxgi_factory("failed to pick hardware adapter"s);

    return adapters.front();
}

winrt::com_ptr<ID3D12Device1> create_device(IDXGIAdapter1 *const hardware_adapter)
{
    winrt::com_ptr<ID3D12Device1> device;

    if (auto result = D3D12CreateDevice(hardware_adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device1), device.put_void()); FAILED(result))
        throw dx::dxgi_factory(fmt::format("failed to create logical device: {0:#x}"s, result));

    {
        auto const requested_feature_levels = std::array{
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0
        };

        D3D12_FEATURE_DATA_FEATURE_LEVELS feature_levels{
            static_cast<UINT>(std::size(requested_feature_levels)), std::data(requested_feature_levels)
        };

        if (auto result = device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &feature_levels, sizeof(feature_levels)); FAILED(result))
            throw dx::device_error(fmt::format("failed to check device D3D12 feature support: {0:#x}"s, result));
    }

    return device;
}

winrt::com_ptr<ID3D12CommandQueue> create_command_queue(ID3D12Device1 *const device, D3D12_COMMAND_LIST_TYPE type)
{
    D3D12_COMMAND_QUEUE_DESC const description{
        type,
        D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        D3D12_COMMAND_QUEUE_FLAG_NONE,
        0
    };

    winrt::com_ptr<ID3D12CommandQueue> queue;

    if (auto result = device->CreateCommandQueue(&description, __uuidof(queue), queue.put_void()); FAILED(result))
        throw dx::device_error(fmt::format("failed to create a command queue: {0:#x}"s, result));

    return queue;
}

winrt::com_ptr<ID3D12CommandAllocator> create_command_allocator(ID3D12Device1 *const device, D3D12_COMMAND_LIST_TYPE type)
{
    winrt::com_ptr<ID3D12CommandAllocator> allocator;

    if (auto result = device->CreateCommandAllocator(type, __uuidof(allocator), allocator.put_void()); FAILED(result))
        throw dx::device_error(fmt::format("failed to create a command allocator: {0:#x}"s, result));

    return allocator;
}

winrt::com_ptr<ID3D12GraphicsCommandList1> create_command_list(ID3D12Device1 *const device, ID3D12CommandAllocator *const allocator, D3D12_COMMAND_LIST_TYPE type)
{
    winrt::com_ptr<ID3D12GraphicsCommandList1> list;

    if (auto result = device->CreateCommandList(0, type, allocator, nullptr, __uuidof(list), list.put_void()); FAILED(result))
        throw dx::device_error(fmt::format("failed to create a command list: {0:#x}"s, result));

    return list;
}

winrt::com_ptr<IDXGISwapChain>
create_swapchain(IDXGIFactory4 *const factory, ID3D12Device1 *const device, platform::window const &window, graphics::extent extent, DXGI_FORMAT format)
{
    auto [width, height] = extent;

    DXGI_MODE_DESC const display_mode_description{
        width, height,
        DXGI_RATIONAL{60, 1},
        format,
        DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
        DXGI_MODE_SCALING_UNSPECIFIED
    };

    DXGI_SWAP_CHAIN_DESC description{
        display_mode_description,
        DXGI_SAMPLE_DESC{4, 4},
        DXGI_USAGE_RENDER_TARGET_OUTPUT,
        3,
        window.handle(),
        TRUE,
        DXGI_SWAP_EFFECT_FLIP_DISCARD,
        0
    };

    winrt::com_ptr<IDXGISwapChain> swap_chain;

    if (auto result = factory->CreateSwapChain(device, &description, swap_chain.put()); FAILED(result))
        throw dx::dxgi_factory(fmt::format("failed to create a swap chain: {0:#x}"s, result));

    return swap_chain;
}

int main()
{
    if (auto result = glfwInit(); result != GLFW_TRUE)
        throw std::runtime_error(fmt::format("failed to init GLFW: {0:#x}\n"s, result));

#if defined(_DEBUG) || defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    {
        winrt::com_ptr<ID3D12Debug3> debug_controller;

        if (auto result = D3D12GetDebugInterface(__uuidof(ID3D12Debug3), debug_controller.put_void()); FAILED(result))
            throw dx::com_exception("failed get debug interface {0:#x}"s);

        debug_controller->EnableDebugLayer();
    }
#endif

    winrt::com_ptr<IDXGIFactory4> dxgi_factory;

    if (auto result = CreateDXGIFactory1(__uuidof(IDXGIFactory4), dxgi_factory.put_void()); FAILED(result))
        throw dx::dxgi_factory(fmt::format("failed to create DXGI factory instance: {0:#x}"s, result));

    auto hardware_adapter = pick_hardware_adapter(dxgi_factory.get());

    auto device = create_device(hardware_adapter.get());

    winrt::com_ptr<ID3D12Fence> fence;

    if (auto result = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), fence.put_void()); FAILED(result))
        throw dx::device_error(fmt::format("failed to create a fence: {0:#x}"s, result));

    auto const RTV_heap_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto const DSV_heap_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    auto const CBV_SRV_UAV_heap_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    auto constexpr back_buffer_format = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaa_levels{
            back_buffer_format,
            4,
            D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,
            0
        };

        auto constexpr feature = D3D12_FEATURE::D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS;

        if (auto result = device->CheckFeatureSupport(feature, &msaa_levels, sizeof(msaa_levels)); FAILED(result))
            throw dx::device_error(fmt::format("failed to check MSAA quality feature support: {0:#x}"s, result));

        if (msaa_levels.NumQualityLevels < 4)
            throw dx::device_error("MSAA quality level lower than required level"s);
    }

    auto command_queue = create_command_queue(device.get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto command_allocator = create_command_allocator(device.get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto command_list = create_command_list(device.get(), command_allocator.get(), D3D12_COMMAND_LIST_TYPE_DIRECT);

    command_list->Close();

    graphics::extent extent{800, 600};

    platform::window window{"DX12 Project"sv, static_cast<std::int32_t>(extent.width), static_cast<std::int32_t>(extent.height)};

    window.update([]
    {
        ;
    });

    command_list = nullptr;
    command_allocator = nullptr;
    command_queue = nullptr;

    fence = nullptr;

    device = nullptr;
    hardware_adapter = nullptr;

    dxgi_factory = nullptr;

    glfwTerminate();
}
