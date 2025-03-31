#pragma once

#ifdef _MSC_VER
#include "exception.hpp"
#include <malloc.h>
_VCRT_EXPORT_STD _NODISCARD _Ret_notnull_ _Post_writable_byte_size_(_Size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(
    size_t _Size
    )
{
    void* ptr;
    while ((ptr = std::malloc(_Size)) == nullptr) {
        std::new_handler handler = std::get_new_handler();
        if (!handler) {
            throw memory::exception::bad_alloc("内存分配失败");
        }
        handler();
    }
    return ptr;
}

_VCRT_EXPORT_STD _NODISCARD _Ret_notnull_ _Post_writable_byte_size_(_Size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](
    size_t _Size
    )
{
    return operator new(_Size);
}

#ifdef __cpp_aligned_new
_VCRT_EXPORT_STD _NODISCARD _Ret_notnull_ _Post_writable_byte_size_(_Size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(
    size_t             _Size,
    ::std::align_val_t _Al
    )
{
    if (_Size == 0) _Size = 1;
    void* ptr = _aligned_malloc(_Size, static_cast<size_t>(_Al));
    if (!ptr) throw memory::exception::bad_alloc("内存分配失败(你看看你都做了些什么!)");
    return ptr;
}

_VCRT_EXPORT_STD _NODISCARD _Ret_notnull_ _Post_writable_byte_size_(_Size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](
    size_t             _Size,
    ::std::align_val_t _Al
    )
{
    return operator new(_Size, _Al);
}
#endif
#endif