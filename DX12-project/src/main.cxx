#include "main.hxx"

#pragma comment(lib, "DXGI.lib")

int main()
{
#if defined(_DEBUG) || defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    IDXGIFactory *dxgi_factory = nullptr;

    if (auto result = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory)); result != S_OK)
        throw std::runtime_error(fmt::format("failed to create DXGI factory instance: {0:#x}"s, result));

    std::vector<IDXGIAdapter *> adapters(16);

    std::generate(std::begin(adapters), std::end(adapters), [dxgi_factory, i = 0u] () mutable
    {
        IDXGIAdapter *adapter = nullptr;

        dxgi_factory->EnumAdapters(i++, &adapter);

        return adapter;
    });

    std::sort(std::begin(adapters), std::end(adapters));

    {
        auto it = std::remove_if(std::begin(adapters), std::end(adapters), [] (auto adapter) { return adapter == nullptr; });

        adapters.erase(it, std::end(adapters));
    }

    for (auto adapter : adapters) {
        DXGI_ADAPTER_DESC description;
        adapter->GetDesc(&description);

        std::wcout << L"Adapter "s + description.Description << std::endl;
    }

    for (auto adapter : adapters)
        adapter->Release();

    //if (auto result = D3D12CreateDevice(); )

    dxgi_factory->Release();

    std::cin.get();
}
