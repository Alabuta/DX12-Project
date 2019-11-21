#include "main.hxx"
#include "utility/exception.hxx"

#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3D12.lib")


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

    return adapters.empty() ? nullptr : adapters.front();
}

int main()
{
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

    if (hardware_adapter == nullptr)
        throw dx::dxgi_factory("failed to pick hardware adapter"s);

    winrt::com_ptr<ID3D12Device1> device;

    if (auto result = D3D12CreateDevice(hardware_adapter.get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device1), device.put_void()); FAILED(result))
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
            throw dx::device_error(fmt::format("failed to check device feature support: {0:#x}"s, result));
    }

    device = nullptr;
    hardware_adapter = nullptr;

    dxgi_factory = nullptr;

    std::cin.get();
}
