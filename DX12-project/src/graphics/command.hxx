#pragma once

#include "main.hxx"
#include "utility/exception.hxx"


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
