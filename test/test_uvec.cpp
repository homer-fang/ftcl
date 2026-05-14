#include "uvec.hpp"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void fail(const std::string& name, const std::string& message) {
    ++failures;
    std::cerr << "[FAIL] " << name << ": " << message << "\n";
}

template <class T>
ftcl::UVec<T> unwrap_vec(const std::string& name, ftcl::expected<ftcl::UVec<T>, std::string> result) {
    if (!result.has_value()) {
        fail(name, std::string("expected UVec but got error: ") + result.error());
        return ftcl::UVec<T>::from_cpu({});
    }
    return std::move(*result);
}

#define EXPECT_TRUE(name, expr) \
    do { \
        if (!(expr)) { \
            fail((name), std::string("expected true: ") + #expr); \
        } \
    } while (false)

#define EXPECT_FALSE(name, expr) \
    do { \
        if ((expr)) { \
            fail((name), std::string("expected false: ") + #expr); \
        } \
    } while (false)

#define EXPECT_EQ(name, lhs, rhs) \
    do { \
        const auto lhs_value = (lhs); \
        const auto rhs_value = (rhs); \
        if (!(lhs_value == rhs_value)) { \
            fail((name), std::string("expected equality: ") + #lhs + " == " + #rhs); \
        } \
    } while (false)

#define EXPECT_HAS_VALUE(name, expr) \
    do { \
        const auto result = (expr); \
        if (!result.has_value()) { \
            fail((name), std::string("expected value but got error: ") + result.error()); \
        } \
    } while (false)

#define EXPECT_ERROR_CONTAINS(name, expr, needle) \
    do { \
        const auto result = (expr); \
        if (result.has_value()) { \
            fail((name), std::string("expected error but got value: ") + #expr); \
        } else if (result.error().find((needle)) == std::string::npos) { \
            fail((name), std::string("expected error containing '") + (needle) + "' but got: " + result.error()); \
        } \
    } while (false)

void test_device_model() {
    const std::string name = "device_model";

    const auto cpu = ftcl::Device::cpu();
    const auto cuda0 = ftcl::Device::cuda(0);
    const auto cuda_bad = ftcl::Device::cuda(-1);

    EXPECT_TRUE(name, cpu.is_cpu());
    EXPECT_FALSE(name, cpu.is_cuda());
    EXPECT_TRUE(name, cuda0.is_cuda());
    EXPECT_FALSE(name, cuda0.is_cpu());
    EXPECT_TRUE(name, cpu.valid());
    EXPECT_TRUE(name, cuda0.valid());
    EXPECT_FALSE(name, cuda_bad.valid());
    EXPECT_EQ(name, cpu.to_string(), std::string("cpu"));
    EXPECT_EQ(name, cuda0.to_string(), std::string("cuda:0"));
    EXPECT_EQ(name, cuda_bad.to_string(), std::string("cuda:-1"));
    EXPECT_TRUE(name, ftcl::Device::cuda(0) == cuda0);
    EXPECT_FALSE(name, ftcl::Device::cuda(1) == cuda0);
}

void test_cpu_storage_and_pointers() {
    const std::string name = "cpu_storage_and_pointers";

    auto vec = unwrap_vec(name, ftcl::UVec<int>::filled(4, 7));
    EXPECT_EQ(name, vec.size(), static_cast<std::size_t>(4));
    EXPECT_FALSE(name, vec.empty());
    EXPECT_TRUE(name, vec.valid_on(ftcl::Device::cpu()));
    EXPECT_EQ(name, vec.latest_device().to_string(), std::string("cpu"));

    auto raw = vec.as_uptr(ftcl::Device::cpu());
    if (!raw.has_value()) {
        fail(name, raw.error());
        return;
    }

    EXPECT_EQ(name, raw->len(), static_cast<std::size_t>(4));
    EXPECT_TRUE(name, raw->device().is_cpu());
    EXPECT_TRUE(name, raw->get() != nullptr);
    EXPECT_EQ(name, raw->get()[0], 7);
    EXPECT_EQ(name, raw->get()[3], 7);

    auto mut = vec.as_mut_uptr(ftcl::Device::cpu());
    if (!mut.has_value()) {
        fail(name, mut.error());
        return;
    }

    mut->get()[1] = 42;
    EXPECT_EQ(name, vec[1], 42);
    EXPECT_EQ(name, vec.cpu_vector()[1], 42);
}

void test_zero_fill_and_copy() {
    const std::string name = "zero_fill_and_copy";

    auto vec = unwrap_vec(name, ftcl::UVec<int>::zeroed(3));
    EXPECT_EQ(name, vec[0], 0);
    EXPECT_EQ(name, vec[2], 0);

    EXPECT_HAS_VALUE(name, vec.fill(5));
    EXPECT_EQ(name, vec[0], 5);
    EXPECT_EQ(name, vec[2], 5);

    auto src = unwrap_vec(name, ftcl::UVec<int>::filled(3, 11));
    EXPECT_HAS_VALUE(name, vec.copy_from(src, ftcl::Device::cpu(), ftcl::Device::cpu(), 3));
    EXPECT_EQ(name, vec[0], 11);
    EXPECT_EQ(name, vec[1], 11);
    EXPECT_EQ(name, vec[2], 11);
}

void test_raw_copy_and_nulls() {
    const std::string name = "raw_copy_and_nulls";

    std::vector<int> src{1, 2, 3};
    std::vector<int> dst{0, 0, 0};

    ftcl::RawUPtr<int> src_ptr(src.data(), src.size(), ftcl::Device::cpu());
    ftcl::RawUMutPtr<int> dst_ptr(dst.data(), dst.size(), ftcl::Device::cpu());

    EXPECT_HAS_VALUE(name, ftcl::copy(dst_ptr, src_ptr, src.size()));
    EXPECT_EQ(name, dst[0], 1);
    EXPECT_EQ(name, dst[1], 2);
    EXPECT_EQ(name, dst[2], 3);

    EXPECT_ERROR_CONTAINS(name, ftcl::copy(dst_ptr, ftcl::RawUPtr<int>(nullptr, dst.size(), ftcl::Device::cpu()), 1), "null universal pointer");
    EXPECT_ERROR_CONTAINS(name, ftcl::copy(ftcl::RawUMutPtr<int>(nullptr, src.size(), ftcl::Device::cpu()), src_ptr, 1), "null universal pointer");
    EXPECT_HAS_VALUE(name, ftcl::copy(ftcl::null_mut_uptr<int>(), ftcl::null_uptr<int>(), 0));
    EXPECT_HAS_VALUE(name, ftcl::fill_len(ftcl::null_mut_uptr<int>(), 9, 0));
}

void test_raw_range_validation() {
    const std::string name = "raw_range_validation";

    std::vector<int> src{1, 2};
    std::vector<int> dst{0, 0};

    ftcl::RawUPtr<int> src_ptr(src.data(), src.size(), ftcl::Device::cpu());
    ftcl::RawUMutPtr<int> dst_ptr(dst.data(), dst.size(), ftcl::Device::cpu());

    EXPECT_ERROR_CONTAINS(name, ftcl::copy(dst_ptr, src_ptr, 3), "copy length exceeds pointer range");
    EXPECT_ERROR_CONTAINS(name, ftcl::fill_len(dst_ptr, 4, 3), "fill length exceeds pointer range");
}

void test_invalid_device_errors() {
    const std::string name = "invalid_device_errors";

    auto vec = unwrap_vec(name, ftcl::UVec<int>::filled(2, 1));
    EXPECT_ERROR_CONTAINS(name, vec.as_uptr(ftcl::Device::cuda(-1)), "invalid device");
    EXPECT_ERROR_CONTAINS(name, vec.as_mut_uptr(ftcl::Device::cuda(-1)), "invalid device");
    EXPECT_ERROR_CONTAINS(name, ftcl::UVec<int>::filled(2, 1, ftcl::Device::cuda(-1)), "invalid device");
}

void test_copy_and_move_semantics_cpu() {
    const std::string name = "copy_and_move_semantics_cpu";

    auto original = unwrap_vec(name, ftcl::UVec<int>::filled(3, 4));
    auto copied = original;

    original[0] = 99;
    EXPECT_EQ(name, original[0], 99);
    EXPECT_EQ(name, copied[0], 4);
    EXPECT_EQ(name, copied[1], 4);

    auto assigned = unwrap_vec(name, ftcl::UVec<int>::zeroed(3));
    assigned = copied;
    EXPECT_EQ(name, assigned[0], 4);
    EXPECT_EQ(name, assigned[2], 4);

    auto moved = std::move(assigned);
    EXPECT_EQ(name, moved.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(name, moved[0], 4);
    EXPECT_EQ(name, moved[2], 4);
}

void test_cpu_write_keeps_cpu_latest() {
    const std::string name = "cpu_write_keeps_cpu_latest";

    auto vec = unwrap_vec(name, ftcl::UVec<int>::filled(3, 2));
    auto mut = vec.as_mut_uptr(ftcl::Device::cpu());
    if (!mut.has_value()) {
        fail(name, mut.error());
        return;
    }

    mut->get()[2] = 17;
    EXPECT_TRUE(name, vec.valid_on(ftcl::Device::cpu()));
    EXPECT_EQ(name, vec.latest_device().to_string(), std::string("cpu"));
    EXPECT_EQ(name, vec.cpu_vector()[2], 17);
}

void test_cuda_backend_behavior() {
    const std::string name = "cuda_backend_behavior";

    auto vec = unwrap_vec(name, ftcl::UVec<int>::filled(3, 3));
    const auto cuda0 = ftcl::Device::cuda(0);

#ifdef FTCL_ENABLE_CUDA
    auto cuda_raw = vec.as_uptr(cuda0);
    if (!cuda_raw.has_value()) {
        fail(name, cuda_raw.error());
        return;
    }

    EXPECT_TRUE(name, cuda_raw->get() != nullptr);
    EXPECT_EQ(name, cuda_raw->len(), static_cast<std::size_t>(3));
    EXPECT_TRUE(name, cuda_raw->device() == cuda0);
    EXPECT_TRUE(name, vec.valid_on(cuda0));
    EXPECT_TRUE(name, vec.valid_on(ftcl::Device::cpu()));

    EXPECT_HAS_VALUE(name, vec.fill(8, cuda0));
    EXPECT_TRUE(name, vec.valid_on(cuda0));
    EXPECT_FALSE(name, vec.valid_on(ftcl::Device::cpu()));

    auto cpu_raw = vec.as_uptr(ftcl::Device::cpu());
    if (!cpu_raw.has_value()) {
        fail(name, cpu_raw.error());
        return;
    }

    EXPECT_EQ(name, cpu_raw->get()[0], 8);
    EXPECT_EQ(name, cpu_raw->get()[1], 8);
    EXPECT_EQ(name, cpu_raw->get()[2], 8);
    EXPECT_TRUE(name, vec.valid_on(ftcl::Device::cpu()));
#else
    EXPECT_ERROR_CONTAINS(name, vec.as_uptr(cuda0), "CUDA backend is not enabled");
    EXPECT_ERROR_CONTAINS(name, vec.as_mut_uptr(cuda0), "CUDA backend is not enabled");
    EXPECT_ERROR_CONTAINS(name, vec.fill(8, cuda0), "CUDA backend is not enabled");
    EXPECT_ERROR_CONTAINS(name, ftcl::UVec<int>::filled(2, 1, cuda0), "CUDA backend is not enabled");
#endif
}

void test_cuda_write_invalidation_or_disabled_errors() {
    const std::string name = "cuda_write_invalidation_or_disabled_errors";

    auto vec = unwrap_vec(name, ftcl::UVec<int>::filled(4, 6));
    const auto cuda0 = ftcl::Device::cuda(0);

#ifdef FTCL_ENABLE_CUDA
    auto cuda_read = vec.as_uptr(cuda0);
    if (!cuda_read.has_value()) {
        fail(name, cuda_read.error());
        return;
    }

    EXPECT_TRUE(name, vec.valid_on(ftcl::Device::cpu()));
    EXPECT_TRUE(name, vec.valid_on(cuda0));

    vec[0] = 13;
    EXPECT_TRUE(name, vec.valid_on(ftcl::Device::cpu()));
    EXPECT_FALSE(name, vec.valid_on(cuda0));

    auto cuda_after_cpu_write = vec.as_uptr(cuda0);
    if (!cuda_after_cpu_write.has_value()) {
        fail(name, cuda_after_cpu_write.error());
        return;
    }

    EXPECT_TRUE(name, vec.valid_on(cuda0));
    EXPECT_TRUE(name, vec.valid_on(ftcl::Device::cpu()));
    EXPECT_EQ(name, vec.cpu_vector()[0], 13);

    auto cuda_mut = vec.as_mut_uptr(cuda0);
    if (!cuda_mut.has_value()) {
        fail(name, cuda_mut.error());
        return;
    }

    EXPECT_TRUE(name, cuda_mut->get() != nullptr);
    EXPECT_TRUE(name, vec.valid_on(cuda0));
    EXPECT_FALSE(name, vec.valid_on(ftcl::Device::cpu()));

    auto cpu_after_cuda_mut = vec.as_uptr(ftcl::Device::cpu());
    if (!cpu_after_cuda_mut.has_value()) {
        fail(name, cpu_after_cuda_mut.error());
        return;
    }

    EXPECT_EQ(name, cpu_after_cuda_mut->get()[0], 13);
    EXPECT_TRUE(name, vec.valid_on(ftcl::Device::cpu()));
#else
    EXPECT_ERROR_CONTAINS(name, vec.as_uptr(cuda0), "CUDA backend is not enabled");
#endif
}

void test_cuda_device_to_device_copy_or_disabled_errors() {
    const std::string name = "cuda_device_to_device_copy_or_disabled_errors";

    auto src = unwrap_vec(name, ftcl::UVec<int>::filled(5, 12));
    auto dst = unwrap_vec(name, ftcl::UVec<int>::zeroed(5));
    const auto cuda0 = ftcl::Device::cuda(0);

#ifdef FTCL_ENABLE_CUDA
    auto copied = dst.copy_from(src, cuda0, cuda0, 5);
    if (!copied.has_value()) {
        fail(name, copied.error());
        return;
    }

    EXPECT_TRUE(name, src.valid_on(cuda0));
    EXPECT_TRUE(name, dst.valid_on(cuda0));
    EXPECT_FALSE(name, dst.valid_on(ftcl::Device::cpu()));

    auto cpu_raw = dst.as_uptr(ftcl::Device::cpu());
    if (!cpu_raw.has_value()) {
        fail(name, cpu_raw.error());
        return;
    }

    for (std::size_t i = 0; i < cpu_raw->len(); ++i) {
        EXPECT_EQ(name, cpu_raw->get()[i], 12);
    }
#else
    EXPECT_ERROR_CONTAINS(name, dst.copy_from(src, cuda0, cuda0, 5), "CUDA backend is not enabled");

    std::vector<int> dst_vec{0, 0};
    ftcl::RawUMutPtr<int> dst_ptr(dst_vec.data(), dst_vec.size(), ftcl::Device::cpu());
    ftcl::RawUPtr<int> cuda_src(reinterpret_cast<const int*>(0x1), dst_vec.size(), cuda0);
    EXPECT_ERROR_CONTAINS(name, ftcl::copy(dst_ptr, cuda_src, dst_vec.size()), "CUDA backend is not enabled");
#endif
}

}  // namespace

int main() {
    test_device_model();
    test_cpu_storage_and_pointers();
    test_zero_fill_and_copy();
    test_raw_copy_and_nulls();
    test_raw_range_validation();
    test_invalid_device_errors();
    test_copy_and_move_semantics_cpu();
    test_cpu_write_keeps_cpu_latest();
    test_cuda_backend_behavior();
    test_cuda_write_invalidation_or_disabled_errors();
    test_cuda_device_to_device_copy_or_disabled_errors();

    if (failures != 0) {
        std::cerr << failures << " UVec test assertion(s) failed\n";
        return 1;
    }

    std::cout << "UVec tests passed\n";
    return 0;
}
