#ifndef HML_SETTINGS_SIMD
#define HML_SETTINGS_SIMD


#ifdef __linux__
#define HAS_AVX __AVX2__ && 1
#define HAS_SSE __SSE2__ && 1
#endif
#ifdef _WIN32
#define HAS_AVX __AVX2__ && 1
#define HAS_SSE __SSE2__ && 1

// HACK for AVX on Windows to fix the mingw gcc bug that prevents 32-byte alignment
__asm__ (
    ".macro vmovapd args:vararg\n"
    "    vmovupd \\args\n"
    ".endm\n"
    ".macro vmovaps args:vararg\n"
    "    vmovups \\args\n"
    ".endm\n"
    ".macro vmovdqa args:vararg\n"
    "    vmovdqu \\args\n"
    ".endm\n"
    ".macro vmovdqa32 args:vararg\n"
    "    vmovdqu32 \\args\n"
    ".endm\n"
    ".macro vmovdqa64 args:vararg\n"
    "    vmovdqu64 \\args\n"
    ".endm\n"
);

#endif

#if HAS_AVX
#define ALIGN 32
#elif HAS_SSE
#define ALIGN 16
#else
#define ALIGN 8
#endif


#endif