/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Ahmed ARIF <arif.ing@outlook.com>
 * The part of Microsoft::WRL::ComPtr the D3D12 Gallium driver uses, for an SDK without WRL.
 */
#pragma once

#include <unknwn.h>

namespace Microsoft { namespace WRL {

template <typename T>
class ComPtr
{
public:
    ComPtr() : ptr_(nullptr) {}
    ComPtr(T *other) : ptr_(other) { if (ptr_) ptr_->AddRef(); }
    ComPtr(const ComPtr &other) : ptr_(other.ptr_) { if (ptr_) ptr_->AddRef(); }
    ComPtr(ComPtr &&other) : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    ~ComPtr() { Reset(); }

    ComPtr &operator=(T *other)
    {
        if (other) other->AddRef();
        Reset();
        ptr_ = other;
        return *this;
    }
    ComPtr &operator=(const ComPtr &other) { return *this = other.ptr_; }
    ComPtr &operator=(ComPtr &&other)
    {
        if (this != &other)
        {
            Reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T *Get() const { return ptr_; }
    T *operator->() const { return ptr_; }
    explicit operator bool() const { return ptr_ != nullptr; }
    T **GetAddressOf() { return &ptr_; }
    T **ReleaseAndGetAddressOf() { Reset(); return &ptr_; }
    T **operator&() { return ReleaseAndGetAddressOf(); }
    T *Detach() { T *out = ptr_; ptr_ = nullptr; return out; }
    void Attach(T *other) { Reset(); ptr_ = other; }
    void Reset() { if (ptr_) { T *old = ptr_; ptr_ = nullptr; old->Release(); } }

    template <typename U>
    HRESULT As(U **out) const
    {
        return ptr_->QueryInterface(__uuidof(U), reinterpret_cast<void **>(out));
    }

private:
    T *ptr_;
};

} }
