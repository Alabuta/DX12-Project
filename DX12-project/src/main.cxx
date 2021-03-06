#include "main.hxx"
#include "utility/exception.hxx"
#include "platform/window.hxx"

#include "graphics/command.hxx"
#include "graphics/descriptor.hxx"

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "RuntimeObject.lib")
//#pragma comment(lib, "ComBase.lib")


namespace graphics
{
    struct extent final {
        std::uint32_t width{0}, height{0};
    };

    DXGI_FORMAT constexpr kDEPTH_FORMAT{DXGI_FORMAT::DXGI_FORMAT_D32_FLOAT};
}

namespace app
{
#if defined(_DEBUG) || defined(DEBUG)
    auto constexpr kDEBUG_D3D{true};
#else
    auto constexpr kDEBUG_D3D{false};
#endif

    auto constexpr kSWAPCHAIN_BUFFER_COUNT = 3u;

    struct D3D final {
        winrt::com_ptr<IDXGIFactory7> dxgi_factory;

        winrt::com_ptr<IDXGIAdapter4> hardware_adapter;
        winrt::com_ptr<ID3D12Device6> device;

        winrt::com_ptr<IDXGISwapChain4> swapchain;

        std::vector<winrt::com_ptr<ID3D12Resource>> swapchain_buffers;
        winrt::com_ptr<ID3D12Resource> depth_stencil_buffer;

        winrt::com_ptr<ID3D12GraphicsCommandList5> command_list;
        winrt::com_ptr<ID3D12CommandAllocator> command_allocator;
        winrt::com_ptr<ID3D12CommandQueue> command_queue;

        winrt::com_ptr<ID3D12Fence1> fence;

        winrt::com_ptr<ID3D12DescriptorHeap> rtv_descriptor_heaps;
        winrt::com_ptr<ID3D12DescriptorHeap> dsv_descriptor_heap;
    };
}


winrt::com_ptr<IDXGIAdapter4> pick_hardware_adapter(IDXGIFactory7 *const dxgi_factory)
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

        if (auto result = D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_1, winrt::guid_of<ID3D12Device6>(), nullptr); FAILED(result))
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

    if (auto adapter = adapters.front().try_as<IDXGIAdapter4>(); adapter == nullptr)
        throw dx::dxgi_factory("failed to query 'IDXGIAdapter4' interface from adapter"s);

    else return adapter;
}

winrt::com_ptr<ID3D12Device6> create_device(IDXGIAdapter4 *const hardware_adapter)
{
    winrt::com_ptr<ID3D12Device6> device;

    if (auto result = D3D12CreateDevice(hardware_adapter, D3D_FEATURE_LEVEL_12_1, winrt::guid_of<ID3D12Device6>(), device.put_void()); FAILED(result))
        throw dx::dxgi_factory(fmt::format("failed to create logical device: {0:#x}"s, result));

    {
        auto const requested_feature_levels = std::array{
            D3D_FEATURE_LEVEL_12_1
        };

        D3D12_FEATURE_DATA_FEATURE_LEVELS feature_levels{
            static_cast<UINT>(std::size(requested_feature_levels)), std::data(requested_feature_levels)
        };

        if (auto result = device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &feature_levels, sizeof(feature_levels)); FAILED(result))
            throw dx::device_error(fmt::format("failed to check device D3D12 feature support: {0:#x}"s, result));
    }

    return device;
}

winrt::com_ptr<IDXGISwapChain4>
create_swapchain(IDXGIFactory7 *const factory, ID3D12CommandQueue *const queue, platform::window const &window,
                 graphics::extent extent, DXGI_FORMAT format, std::uint32_t swapchain_buffer_count)
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
        DXGI_SAMPLE_DESC{1, 0},
        DXGI_USAGE_RENDER_TARGET_OUTPUT,
        swapchain_buffer_count,
        window.handle(),
        TRUE,
        DXGI_SWAP_EFFECT_FLIP_DISCARD,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
    };

    winrt::com_ptr<IDXGISwapChain> swap_chain;

    if (auto result = factory->CreateSwapChain(queue, &description, swap_chain.put()); FAILED(result))
        throw dx::dxgi_factory(fmt::format("failed to create a swap chain: {0:#x}"s, result));

    if (auto sc = swap_chain.try_as<IDXGISwapChain4>(); sc == nullptr)
        throw dx::dxgi_factory("failed to query 'IDXGISwapChain4' interface from swap chain"s);

    else return sc;
}

D3D12_CPU_DESCRIPTOR_HANDLE
current_back_buffer_view(ID3D12DescriptorHeap *const back_buffer, std::uint32_t current_back_buffer_index, std::uint32_t descriptor_byte_size)
{
    auto handle_start = back_buffer->GetCPUDescriptorHandleForHeapStart();

    return CD3DX12_CPU_DESCRIPTOR_HANDLE{handle_start, static_cast<std::int32_t>(current_back_buffer_index), descriptor_byte_size};
}

D3D12_CPU_DESCRIPTOR_HANDLE
depth_stencil_buffer_view(ID3D12DescriptorHeap *const depth_stencil_buffer)
{
    return depth_stencil_buffer->GetCPUDescriptorHandleForHeapStart();
}

std::vector<winrt::com_ptr<ID3D12Resource>>
create_swapchain_buffers(ID3D12Device6 *const device, IDXGISwapChain4 *const swapchain, std::uint32_t swapchain_buffer_count,
                      ID3D12DescriptorHeap *const rtv_descriptor_heap, std::uint32_t descriptor_byte_size)
{
    std::vector<winrt::com_ptr<ID3D12Resource>> swapchain_buffers(swapchain_buffer_count);

    auto rtv_heap_handle = static_cast<CD3DX12_CPU_DESCRIPTOR_HANDLE>(rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart());

    std::generate(std::begin(swapchain_buffers), std::end(swapchain_buffers), [&, i = 0u] () mutable
    {
        winrt::com_ptr<ID3D12Resource> buffer;

        if (auto result = swapchain->GetBuffer(i++, winrt::guid_of<ID3D12Resource>(), buffer.put_void()); FAILED(result))
            throw dx::swapchain(fmt::format("failed to get swapchain buffer: {0:#x}"s, result));

        device->CreateRenderTargetView(buffer.get(), nullptr, rtv_heap_handle);

        rtv_heap_handle.Offset(1, descriptor_byte_size);
        
        return buffer;
    });

    return swapchain_buffers;
}

winrt::com_ptr<ID3D12Resource>
create_depth_stencil_buffer(ID3D12Device6 *const device, ID3D12CommandAllocator *const command_allocator, ID3D12GraphicsCommandList5 *const command_list,
                            ID3D12CommandQueue *const command_queue, ID3D12DescriptorHeap *const descriptor_heap, graphics::extent extent, DXGI_FORMAT format)
{
    if (auto result = command_allocator->Reset(); FAILED(result))
        throw dx::swapchain(fmt::format("failed to reset command allocator: {0:#x}"s, result));

    if (auto result = command_list->Reset(command_allocator, nullptr); FAILED(result))
        throw dx::swapchain(fmt::format("failed to reset command list: {0:#x}"s, result));

    auto [width, height] = extent;

#if 1
    D3D12_RESOURCE_DESC const description{
        D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        0,
        width, height,
        1,
        1,
        format,
        DXGI_SAMPLE_DESC{1, 0},
        D3D12_TEXTURE_LAYOUT_UNKNOWN,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL /*| D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE*/
    };
#else
    auto description = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
#endif

    /*D3D12_HEAP_PROPERTIES const heap_properties{
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE,
        D3D12_MEMORY_POOL_L1,
        1,
        1
    };*/

    //auto constexpr initial_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
    auto constexpr initial_state = D3D12_RESOURCE_STATE_COMMON;
    //auto constexpr initial_state = D3D12_RESOURCE_STATE_DEPTH_WRITE;

    D3D12_CLEAR_VALUE const clear_value{
        .Format = format,
        .DepthStencil = D3D12_DEPTH_STENCIL_VALUE{1.f, 0}
    };

    winrt::com_ptr<ID3D12Resource> buffer;

    if (auto result = device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES{D3D12_HEAP_TYPE_DEFAULT}, D3D12_HEAP_FLAG_NONE, &description,
                                                      initial_state, &clear_value, winrt::guid_of<ID3D12Resource>(), buffer.put_void()); FAILED(result))
        throw dx::swapchain(fmt::format("failed to create depth-stencil buffer: {0:#x}"s, result));

    D3D12_DEPTH_STENCIL_VIEW_DESC const view_description{
        .Format = format,
        .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
        .Flags = D3D12_DSV_FLAG_NONE,
        .Texture2D = D3D12_TEX2D_DSV{ .MipSlice = 0 }
    };

    auto buffer_view = depth_stencil_buffer_view(descriptor_heap);

    /*try {
        auto x = buffer.get();
        auto y = &view_description;
        auto z = buffer_view;
        device->CreateDepthStencilView(x, y, z);
    } catch (std::exception const &ex) {
        std::cout << ex.what() << std::endl;
    }*/
    device->CreateDepthStencilView(buffer.get(), &view_description, buffer_view);

    auto barriers = std::array{
        CD3DX12_RESOURCE_BARRIER::Transition(buffer.get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE)
    };

    command_list->ResourceBarrier(static_cast<UINT>(std::size(barriers)), std::data(barriers));
    command_list->Close();

    std::array<ID3D12CommandList *, 1> command_lists{command_list};

    command_queue->ExecuteCommandLists(static_cast<UINT>(std::size(command_lists)), std::data(command_lists));

    return buffer;
}

void flush_command_queue(ID3D12CommandQueue *const command_queue, ID3D12Fence1 *const fence)
{
    static UINT64 current_fence = 0;

    if (auto result = command_queue->Signal(fence, ++current_fence); FAILED(result))
        throw dx::swapchain(fmt::format("failed to update a fence: {0:#x}"s, result));

    if (fence->GetCompletedValue() < current_fence) {
        auto event_handle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);

        if (auto result = fence->SetEventOnCompletion(current_fence, event_handle); FAILED(result))
            throw dx::swapchain(fmt::format("failed to specify a fence signaled event: {0:#x}"s, result));

        WaitForSingleObject(event_handle, INFINITE);

        CloseHandle(event_handle);
    }
}

app::D3D init_D3D(graphics::extent extent, platform::window const &window)
{
    if constexpr (app::kDEBUG_D3D) {
        winrt::com_ptr<ID3D12Debug3> debug_controller;

        if (auto result = D3D12GetDebugInterface(winrt::guid_of<ID3D12Debug3>(), debug_controller.put_void()); FAILED(result))
            throw dx::com_exception("failed get debug interface {0:#x}"s);

        debug_controller->EnableDebugLayer();
    }

    winrt::com_ptr<IDXGIFactory7> dxgi_factory;

    {
        UINT flags = 0;

        if constexpr (app::kDEBUG_D3D)
            flags = DXGI_CREATE_FACTORY_DEBUG;

        //if (auto result = CreateDXGIFactory2(flags, IID_IDXGIFactory7, dxgi_factory.put_void()); FAILED(result))
        if (auto result = CreateDXGIFactory2(flags, winrt::guid_of<IDXGIFactory7>(), dxgi_factory.put_void()); FAILED(result))
            throw dx::dxgi_factory(fmt::format("failed to create DXGI factory instance: {0:#x}"s, result));
    }

    auto hardware_adapter = pick_hardware_adapter(dxgi_factory.get());

    auto device = create_device(hardware_adapter.get());

    winrt::com_ptr<ID3D12Fence1> fence;

    if (auto result = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, winrt::guid_of<ID3D12Fence1>(), fence.put_void()); FAILED(result))
        throw dx::device_error(fmt::format("failed to create a fence: {0:#x}"s, result));

    auto const RTV_heap_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto const DSV_heap_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    auto const CBV_SRV_UAV_heap_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    auto constexpr back_buffer_format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;

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

        if (msaa_levels.NumQualityLevels < 1)
            throw dx::device_error("MSAA quality level lower than required level"s);
    }

    auto command_queue = create_command_queue(device.get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto command_allocator = create_command_allocator(device.get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto command_list = create_command_list(device.get(), command_allocator.get(), D3D12_COMMAND_LIST_TYPE_DIRECT);

    command_list->Close();

    auto swapchain = create_swapchain(dxgi_factory.get(), command_queue.get(), window, extent, back_buffer_format, app::kSWAPCHAIN_BUFFER_COUNT);

    auto rtv_descriptor_heaps = create_descriptor_heaps(device.get(), D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_RTV, app::kSWAPCHAIN_BUFFER_COUNT);
    auto dsv_descriptor_heap = create_descriptor_heaps(device.get(), D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);

    auto swapchain_buffers = create_swapchain_buffers(device.get(), swapchain.get(), app::kSWAPCHAIN_BUFFER_COUNT,
                                                      rtv_descriptor_heaps.get(), RTV_heap_size);

    auto depth_stencil_buffer = create_depth_stencil_buffer(device.get(), command_allocator.get(), command_list.get(), command_queue.get(),
                                                            dsv_descriptor_heap.get(), extent, graphics::kDEPTH_FORMAT);

    flush_command_queue(command_queue.get(), fence.get());

    return app::D3D{
        dxgi_factory,

        hardware_adapter,
        device,

        swapchain,
        swapchain_buffers,
        depth_stencil_buffer,

        command_list,
        command_allocator,
        command_queue,

        fence,

        rtv_descriptor_heaps,
        dsv_descriptor_heap
    };
}

void cleanup_D3D(app::D3D &d3d)
{
    d3d.dsv_descriptor_heap = nullptr;
    d3d.rtv_descriptor_heaps = nullptr;

    d3d.swapchain = nullptr;

    d3d.command_list = nullptr;
    d3d.command_allocator = nullptr;
    d3d.command_queue = nullptr;

    d3d.fence = nullptr;

    d3d.device = nullptr;
    d3d.hardware_adapter = nullptr;

    d3d.dxgi_factory = nullptr;
}

void draw(app::D3D &d3d, graphics::extent extent)
{
    if (auto result = d3d.command_allocator->Reset(); FAILED(result))
        throw dx::dxgi_factory(fmt::format("failed to reset a command allocator: {0:#x}"s, result));

    if (auto result = d3d.command_list->Reset(d3d.command_allocator.get(), nullptr); FAILED(result))
        throw dx::dxgi_factory(fmt::format("failed to reset a command list: {0:#x}"s, result));

    auto back_buffer_index = 0u;

    auto current_back_buffer = d3d.swapchain_buffers.at(back_buffer_index);

    //current_back_buffer_view(d3d.rtv_descriptor_heaps.get(), back_buffer_index, );

    auto barriers = std::array{
        CD3DX12_RESOURCE_BARRIER::Transition(current_back_buffer.get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
    };

    d3d.command_list->ResourceBarrier(static_cast<UINT>(std::size(barriers)), std::data(barriers));

    D3D12_VIEWPORT const viewport{
        0, 0,
        static_cast<float>(extent.width), static_cast<float>(extent.height),
        0, 1
    };

    d3d.command_list->RSSetViewports(1, &viewport);

    D3D12_RECT scissor{
        0, 0,
        static_cast<LONG>(extent.width), static_cast<LONG>(extent.height)
    };

    d3d.command_list->RSSetScissorRects(1, &scissor);
}


int main()
{
    if (auto result = glfwInit(); result != GLFW_TRUE)
        throw std::runtime_error(fmt::format("failed to init GLFW: {0:#x}\n"s, result));

#if defined(_DEBUG) || defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    graphics::extent extent{800, 600};

    platform::window window{"DX12 Project"sv, static_cast<std::int32_t>(extent.width), static_cast<std::int32_t>(extent.height)};

    auto d3d = init_D3D(extent, window);

    window.update([]
    {
        ;
    });

    cleanup_D3D(d3d);

    glfwTerminate();
}
