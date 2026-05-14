#pragma once

#include "expected_compat.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef FTCL_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

namespace ftcl {

enum class DeviceKind {
    CPU,
    CUDA,
};

class Device {
public:
    static Device cpu() {
        return Device(DeviceKind::CPU, 0);
    }

    static Device cuda(int index) {
        return Device(DeviceKind::CUDA, index);
    }

    DeviceKind kind() const {
        return kind_;
    }

    int index() const {
        return index_;
    }

    bool is_cpu() const {
        return kind_ == DeviceKind::CPU;
    }

    bool is_cuda() const {
        return kind_ == DeviceKind::CUDA;
    }

    bool valid() const {
        return is_cpu() || index_ >= 0;
    }

    std::string to_string() const {
        if (is_cpu()) {
            return "cpu";
        }

        std::ostringstream oss;
        oss << "cuda:" << index_;
        return oss.str();
    }

private:
    Device(DeviceKind kind, int index)
        : kind_(kind),
          index_(index) {}

    DeviceKind kind_ = DeviceKind::CPU;
    int index_ = 0;
};

inline bool operator==(const Device& lhs, const Device& rhs) {
    return lhs.kind() == rhs.kind() && lhs.index() == rhs.index();
}

inline bool operator!=(const Device& lhs, const Device& rhs) {
    return !(lhs == rhs);
}

template <class T>
struct UniversalCopy : std::bool_constant<std::is_trivially_copyable_v<T>> {};

template <class T>
inline constexpr bool universal_copy_v = UniversalCopy<T>::value;

template <class T>
struct Zeroable : std::bool_constant<std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T>> {};

template <class T>
inline constexpr bool zeroable_v = Zeroable<T>::value;

template <class T>
class RawUPtr {
    static_assert(universal_copy_v<T>, "RawUPtr<T> requires a trivially copyable element type");

public:
    RawUPtr() = default;

    RawUPtr(const T* ptr, std::size_t len, Device device)
        : ptr_(ptr),
          len_(len),
          device_(device) {}

    const T* get() const {
        return ptr_;
    }

    std::size_t len() const {
        return len_;
    }

    Device device() const {
        return device_;
    }

    bool is_null() const {
        return ptr_ == nullptr;
    }

private:
    const T* ptr_ = nullptr;
    std::size_t len_ = 0;
    Device device_ = Device::cpu();
};

template <class T>
class RawUMutPtr {
    static_assert(universal_copy_v<T>, "RawUMutPtr<T> requires a trivially copyable element type");

public:
    RawUMutPtr() = default;

    RawUMutPtr(T* ptr, std::size_t len, Device device)
        : ptr_(ptr),
          len_(len),
          device_(device) {}

    T* get() const {
        return ptr_;
    }

    std::size_t len() const {
        return len_;
    }

    Device device() const {
        return device_;
    }

    bool is_null() const {
        return ptr_ == nullptr;
    }

private:
    T* ptr_ = nullptr;
    std::size_t len_ = 0;
    Device device_ = Device::cpu();
};

template <class T>
RawUPtr<T> null_uptr(Device device = Device::cpu()) {
    return RawUPtr<T>(nullptr, 0, device);
}

template <class T>
RawUMutPtr<T> null_mut_uptr(Device device = Device::cpu()) {
    return RawUMutPtr<T>(nullptr, 0, device);
}

inline std::string cuda_disabled_message() {
    return "CUDA backend is not enabled";
}

#ifdef FTCL_ENABLE_CUDA
inline ftcl::expected<void*, std::string> cuda_allocate_bytes(std::size_t byte_count, Device device) {
    if (!device.valid() || !device.is_cuda()) {
        return ftcl::unexpected("invalid CUDA device \"" + device.to_string() + "\"");
    }

    auto set_result = cudaSetDevice(device.index());
    if (set_result != cudaSuccess) {
        return ftcl::unexpected(std::string("cudaSetDevice failed: ") + cudaGetErrorString(set_result));
    }

    void* ptr = nullptr;
    const auto alloc_size = byte_count == 0 ? 1 : byte_count;
    auto alloc_result = cudaMalloc(&ptr, alloc_size);
    if (alloc_result != cudaSuccess) {
        return ftcl::unexpected(std::string("cudaMalloc failed: ") + cudaGetErrorString(alloc_result));
    }

    return ptr;
}

inline ftcl::expected<std::size_t, std::string> cuda_copy_bytes(void* dst,
                                                               Device dst_device,
                                                               const void* src,
                                                               Device src_device,
                                                               std::size_t byte_count) {
    if (byte_count == 0) {
        return static_cast<std::size_t>(0);
    }
    if (dst == nullptr || src == nullptr) {
        return ftcl::unexpected("cannot copy through a null universal pointer");
    }

    if (dst_device.is_cpu() && src_device.is_cpu()) {
        std::memmove(dst, src, byte_count);
        return byte_count;
    }

    cudaMemcpyKind kind = cudaMemcpyDefault;
    if (dst_device.is_cpu() && src_device.is_cuda()) {
        kind = cudaMemcpyDeviceToHost;
    } else if (dst_device.is_cuda() && src_device.is_cpu()) {
        kind = cudaMemcpyHostToDevice;
    } else if (dst_device.is_cuda() && src_device.is_cuda()) {
        kind = cudaMemcpyDeviceToDevice;
    } else {
        return ftcl::unexpected("invalid device pair for copy");
    }

    if (dst_device.is_cuda()) {
        auto set_result = cudaSetDevice(dst_device.index());
        if (set_result != cudaSuccess) {
            return ftcl::unexpected(std::string("cudaSetDevice failed: ") + cudaGetErrorString(set_result));
        }
    } else if (src_device.is_cuda()) {
        auto set_result = cudaSetDevice(src_device.index());
        if (set_result != cudaSuccess) {
            return ftcl::unexpected(std::string("cudaSetDevice failed: ") + cudaGetErrorString(set_result));
        }
    }

    auto copy_result = cudaMemcpy(dst, src, byte_count, kind);
    if (copy_result != cudaSuccess) {
        return ftcl::unexpected(std::string("cudaMemcpy failed: ") + cudaGetErrorString(copy_result));
    }

    return byte_count;
}
#endif

template <class T>
ftcl::expected<std::size_t, std::string> copy(RawUMutPtr<T> dst, RawUPtr<T> src, std::size_t len) {
    if (len > dst.len() || len > src.len()) {
        return ftcl::unexpected("copy length exceeds pointer range");
    }
    if (len == 0) {
        return static_cast<std::size_t>(0);
    }
    if (dst.is_null() || src.is_null()) {
        return ftcl::unexpected("cannot copy through a null universal pointer");
    }

    const std::size_t byte_count = len * sizeof(T);
    if (dst.device().is_cpu() && src.device().is_cpu()) {
        std::memmove(dst.get(), src.get(), byte_count);
        return len;
    }

#ifdef FTCL_ENABLE_CUDA
    auto copied = cuda_copy_bytes(dst.get(), dst.device(), src.get(), src.device(), byte_count);
    if (!copied.has_value()) {
        return ftcl::unexpected(copied.error());
    }
    return len;
#else
    return ftcl::unexpected(cuda_disabled_message());
#endif
}

template <class T>
ftcl::expected<std::size_t, std::string> fill_len(RawUMutPtr<T> dst, const T& value, std::size_t len) {
    if (len > dst.len()) {
        return ftcl::unexpected("fill length exceeds pointer range");
    }
    if (len == 0) {
        return static_cast<std::size_t>(0);
    }
    if (dst.is_null()) {
        return ftcl::unexpected("cannot fill through a null universal pointer");
    }

    if (dst.device().is_cpu()) {
        std::fill_n(dst.get(), len, value);
        return len;
    }

#ifdef FTCL_ENABLE_CUDA
    std::vector<T> host(len, value);
    RawUPtr<T> host_src(host.data(), host.size(), Device::cpu());
    return copy(dst, host_src, len);
#else
    return ftcl::unexpected(cuda_disabled_message());
#endif
}

template <class T>
class UVec {
    static_assert(universal_copy_v<T>, "UVec<T> requires a trivially copyable element type");

public:
    UVec() = default;

    explicit UVec(std::vector<T> data)
        : cpu_(std::move(data)),
          cpu_valid_(true) {}

    UVec(const UVec& other)
        : cpu_(other.cpu_vector()),
          cpu_valid_(true) {}

    UVec& operator=(const UVec& other) {
        if (this == &other) {
            return *this;
        }
        release_cuda_buffers();
        cpu_ = other.cpu_vector();
        cpu_valid_ = true;
        return *this;
    }

    UVec(UVec&& other) noexcept
        : cpu_(std::move(other.cpu_)),
          cpu_valid_(other.cpu_valid_),
          cuda_buffers_(std::move(other.cuda_buffers_)) {
        other.cpu_valid_ = true;
        other.cuda_buffers_.clear();
    }

    UVec& operator=(UVec&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        release_cuda_buffers();
        cpu_ = std::move(other.cpu_);
        cpu_valid_ = other.cpu_valid_;
        cuda_buffers_ = std::move(other.cuda_buffers_);
        other.cpu_valid_ = true;
        other.cuda_buffers_.clear();
        return *this;
    }

    ~UVec() {
        release_cuda_buffers();
    }

    static UVec from_cpu(std::vector<T> data) {
        return UVec(std::move(data));
    }

    static ftcl::expected<UVec<T>, std::string> filled(std::size_t len,
                                                       const T& value,
                                                       Device device = Device::cpu()) {
        if (!device.valid()) {
            return ftcl::unexpected("invalid device \"" + device.to_string() + "\"");
        }

        UVec<T> out{std::vector<T>(len, value)};
        if (device.is_cpu()) {
            return out;
        }

        auto dst = out.as_mut_uptr(device);
        if (!dst.has_value()) {
            return ftcl::unexpected(dst.error());
        }
        auto filled_count = fill_len(*dst, value, len);
        if (!filled_count.has_value()) {
            return ftcl::unexpected(filled_count.error());
        }
        return out;
    }

    static ftcl::expected<UVec<T>, std::string> zeroed(std::size_t len, Device device = Device::cpu()) {
        static_assert(zeroable_v<T>, "UVec<T>::zeroed requires a zeroable element type");
        return filled(len, T{}, device);
    }

    std::size_t size() const {
        return cpu_.size();
    }

    bool empty() const {
        return cpu_.empty();
    }

    Device latest_device() const {
        if (cpu_valid_) {
            return Device::cpu();
        }
        for (const auto& buffer : cuda_buffers_) {
            if (buffer.valid) {
                return Device::cuda(buffer.device_index);
            }
        }
        return Device::cpu();
    }

    bool valid_on(Device device) const {
        if (!device.valid()) {
            return false;
        }
        if (device.is_cpu()) {
            return cpu_valid_;
        }

        const auto* buffer = find_cuda_buffer(device.index());
        return buffer != nullptr && buffer->valid;
    }

    ftcl::expected<RawUPtr<T>, std::string> as_uptr(Device device = Device::cpu()) const {
        auto ready = ensure_readable(device);
        if (!ready.has_value()) {
            return ftcl::unexpected(ready.error());
        }

        if (device.is_cpu()) {
            return RawUPtr<T>(cpu_.data(), cpu_.size(), device);
        }

        const auto* buffer = find_cuda_buffer(device.index());
        if (buffer == nullptr || buffer->ptr == nullptr) {
            return ftcl::unexpected("CUDA buffer is not allocated");
        }
        return RawUPtr<T>(buffer->ptr, cpu_.size(), device);
    }

    ftcl::expected<RawUMutPtr<T>, std::string> as_mut_uptr(Device device = Device::cpu()) {
        auto ready = ensure_readable(device);
        if (!ready.has_value()) {
            return ftcl::unexpected(ready.error());
        }

        mark_only_valid(device);
        if (device.is_cpu()) {
            return RawUMutPtr<T>(cpu_.data(), cpu_.size(), device);
        }

        auto* buffer = find_cuda_buffer(device.index());
        if (buffer == nullptr || buffer->ptr == nullptr) {
            return ftcl::unexpected("CUDA buffer is not allocated");
        }
        return RawUMutPtr<T>(buffer->ptr, cpu_.size(), device);
    }

    ftcl::expected<std::size_t, std::string> fill(const T& value, Device device = Device::cpu()) {
        auto dst = as_mut_uptr(device);
        if (!dst.has_value()) {
            return ftcl::unexpected(dst.error());
        }
        return fill_len(*dst, value, size());
    }

    ftcl::expected<std::size_t, std::string> copy_from(const UVec<T>& src,
                                                       Device dst_device,
                                                       Device src_device,
                                                       std::size_t len) {
        if (len > size() || len > src.size()) {
            return ftcl::unexpected("copy length exceeds UVec bounds");
        }

        auto dst = as_mut_uptr(dst_device);
        if (!dst.has_value()) {
            return ftcl::unexpected(dst.error());
        }

        auto src_ptr = src.as_uptr(src_device);
        if (!src_ptr.has_value()) {
            return ftcl::unexpected(src_ptr.error());
        }

        return copy(*dst, *src_ptr, len);
    }

    const T& operator[](std::size_t index) const {
        auto ready = ensure_readable(Device::cpu());
        if (!ready.has_value()) {
            throw std::runtime_error(ready.error());
        }
        return cpu_[index];
    }

    T& operator[](std::size_t index) {
        auto ready = ensure_readable(Device::cpu());
        if (!ready.has_value()) {
            throw std::runtime_error(ready.error());
        }
        mark_only_valid(Device::cpu());
        return cpu_[index];
    }

    const std::vector<T>& cpu_vector() const {
        auto ready = ensure_readable(Device::cpu());
        if (!ready.has_value()) {
            throw std::runtime_error(ready.error());
        }
        return cpu_;
    }

private:
    struct CudaBuffer {
        int device_index = -1;
        T* ptr = nullptr;
        bool valid = false;
    };

    CudaBuffer* find_cuda_buffer(int index) {
        for (auto& buffer : cuda_buffers_) {
            if (buffer.device_index == index) {
                return &buffer;
            }
        }
        return nullptr;
    }

    const CudaBuffer* find_cuda_buffer(int index) const {
        for (const auto& buffer : cuda_buffers_) {
            if (buffer.device_index == index) {
                return &buffer;
            }
        }
        return nullptr;
    }

    ftcl::expected<CudaBuffer*, std::string> ensure_cuda_buffer(int index) const {
#ifdef FTCL_ENABLE_CUDA
        if (index < 0) {
            return ftcl::unexpected("invalid CUDA device index");
        }

        auto* existing = const_cast<UVec<T>*>(this)->find_cuda_buffer(index);
        if (existing != nullptr) {
            return existing;
        }

        auto allocated = cuda_allocate_bytes(size() * sizeof(T), Device::cuda(index));
        if (!allocated.has_value()) {
            return ftcl::unexpected(allocated.error());
        }

        CudaBuffer buffer;
        buffer.device_index = index;
        buffer.ptr = static_cast<T*>(*allocated);
        buffer.valid = false;
        auto& list = const_cast<UVec<T>*>(this)->cuda_buffers_;
        list.push_back(buffer);
        return &list.back();
#else
        (void)index;
        return ftcl::unexpected(cuda_disabled_message());
#endif
    }

    ftcl::expected<std::size_t, std::string> ensure_readable(Device device) const {
        if (!device.valid()) {
            return ftcl::unexpected("invalid device \"" + device.to_string() + "\"");
        }

        if (device.is_cpu()) {
            if (cpu_valid_) {
                return static_cast<std::size_t>(0);
            }

            const auto* src = first_valid_cuda_buffer();
            if (src == nullptr) {
                return ftcl::unexpected("no valid copy exists for CPU synchronization");
            }

#ifdef FTCL_ENABLE_CUDA
            auto copied = cuda_copy_bytes(const_cast<T*>(cpu_.data()),
                                          Device::cpu(),
                                          src->ptr,
                                          Device::cuda(src->device_index),
                                          size() * sizeof(T));
            if (!copied.has_value()) {
                return ftcl::unexpected(copied.error());
            }
            const_cast<UVec<T>*>(this)->cpu_valid_ = true;
            return *copied;
#else
            return ftcl::unexpected(cuda_disabled_message());
#endif
        }

        auto dst = ensure_cuda_buffer(device.index());
        if (!dst.has_value()) {
            return ftcl::unexpected(dst.error());
        }
        if ((*dst)->valid) {
            return static_cast<std::size_t>(0);
        }

        if (cpu_valid_) {
#ifdef FTCL_ENABLE_CUDA
            auto copied = cuda_copy_bytes((*dst)->ptr,
                                          device,
                                          cpu_.data(),
                                          Device::cpu(),
                                          size() * sizeof(T));
            if (!copied.has_value()) {
                return ftcl::unexpected(copied.error());
            }
            (*dst)->valid = true;
            return *copied;
#else
            return ftcl::unexpected(cuda_disabled_message());
#endif
        }

        const auto* src = first_valid_cuda_buffer();
        if (src == nullptr) {
            return ftcl::unexpected("no valid copy exists for CUDA synchronization");
        }

#ifdef FTCL_ENABLE_CUDA
        auto copied = cuda_copy_bytes((*dst)->ptr,
                                      device,
                                      src->ptr,
                                      Device::cuda(src->device_index),
                                      size() * sizeof(T));
        if (!copied.has_value()) {
            return ftcl::unexpected(copied.error());
        }
        (*dst)->valid = true;
        return *copied;
#else
        return ftcl::unexpected(cuda_disabled_message());
#endif
    }

    const CudaBuffer* first_valid_cuda_buffer() const {
        for (const auto& buffer : cuda_buffers_) {
            if (buffer.valid) {
                return &buffer;
            }
        }
        return nullptr;
    }

    void mark_only_valid(Device device) {
        cpu_valid_ = device.is_cpu();
        for (auto& buffer : cuda_buffers_) {
            buffer.valid = device.is_cuda() && buffer.device_index == device.index();
        }
    }

    void release_cuda_buffers() {
#ifdef FTCL_ENABLE_CUDA
        for (auto& buffer : cuda_buffers_) {
            if (buffer.ptr != nullptr) {
                cudaSetDevice(buffer.device_index);
                cudaFree(buffer.ptr);
                buffer.ptr = nullptr;
            }
        }
#endif
        cuda_buffers_.clear();
    }

    std::vector<T> cpu_;
    mutable bool cpu_valid_ = true;
    mutable std::vector<CudaBuffer> cuda_buffers_;
};

}  // namespace ftcl
