#pragma once

#include "exception.hpp"

#include <malloc.h>


_Ret_notnull_ _Success_(return != 0) _Post_writable_byte_size_(size)
void* __CRTDECL operator new(std::size_t size)
{
    void* ptr = std::malloc(size);
    if (!ptr)
    {
        throw memory::exception::bad_alloc("内存分配失败(你看看你都做了些什么!)");
    }
    return ptr;
}

_Ret_notnull_ _Success_(return != 0) _Post_writable_byte_size_(size)
void* __CRTDECL operator new[](std::size_t size)
{
    return operator new(size);
}

_Ret_maybenull_ _Post_writable_byte_size_(size)
void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    return std::malloc(size);
}

_Ret_maybenull_ _Post_writable_byte_size_(size)
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    return std::malloc(size);
}