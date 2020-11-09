#pragma once
#if defined(__clang__)
#define compiler_is_gcc_m 0
#define compiler_is_clang_m 1
#define compiler_is_msvc_m 0
#define compiler_is_llvm_m 0
#define compiler_is_intel_m 0
#elif defined(__GNUC__) || defined(__GNUG__)
#define compiler_is_gcc_m 1
#define compiler_is_clang_m 0
#define compiler_is_msvc_m 0
#define compiler_is_llvm_m 0
#define compiler_is_intel_m 0
#elif defined(_MSC_VER)
#define compiler_is_gcc_m 0
#define compiler_is_clang_m 0
#define compiler_is_msvc_m 1
#define compiler_is_llvm_m 0
#define compiler_is_intel_m 0
#elif defined(__llvm__)
#define compiler_is_gcc_m 0
#define compiler_is_clang_m 0
#define compiler_is_msvc_m 0
#define compiler_is_llvm_m 1
#define compiler_is_intel_m 0
#elif defined(__INTEL_COMPILER)
#define compiler_is_gcc_m 0
#define compiler_is_clang_m 0
#define compiler_is_msvc_m 0
#define compiler_is_llvm_m 0
#define compiler_is_intel_m 1
#endif
