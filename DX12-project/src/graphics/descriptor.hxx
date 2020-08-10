#pragma once

#include "main.hxx"
#include "utility/exception.hxx"


winrt::com_ptr<ID3D12DescriptorHeap>
create_descriptor_heaps(ID3D12Device1 *const device, D3D12_DESCRIPTOR_HEAP_TYPE type, std::uint32_t number)
{
    D3D12_DESCRIPTOR_HEAP_DESC description{
        type,
        number,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        0
    };

    winrt::com_ptr<ID3D12DescriptorHeap> heap;

    if (auto result = device->CreateDescriptorHeap(&description, __uuidof(heap), heap.put_void()); FAILED(result))
        throw dx::dxgi_factory(fmt::format("failed to create a descriptor heap(s): {0:#x}"s, result));

    return heap;
}
