#ifndef HML_MATH
#define HML_MATH

#include <cmath>
#include <immintrin.h>
#include <array>
#include <iostream>

#include "settings.h"
#include "settings_simd.h"

namespace hml {

using TF    = float;
using TF128 = __m128;
using TF256 = __m256;


template<typename T>
struct vec3_t {
    T x, y, z;

    vec3_t() noexcept;
    vec3_t(T x) noexcept;
    vec3_t(T x, T y, T z) noexcept;
    vec3_t(const TF* xsPtr, const TF* ysPtr, const TF* zsPtr) noexcept;

    void store(TF* xsPtr, TF* ysPtr, TF* zsPtr) const noexcept;
};

using vec3     = vec3_t<TF>;
using vec3_128 = vec3_t<TF128>;
using vec3_256 = vec3_t<TF256>;

template<typename T>
std::ostream& operator<<(std::ostream& ostream, const vec3_t<T>& v);

std::ostream& operator<<(std::ostream& ostream, const TF128& tf);
std::ostream& operator<<(std::ostream& ostream, const TF256& tf);

vec3_256 gather_from_vec3(const float* src, const __m256i& indices) noexcept;

// Rotate left within the high-most 6 elements
TF256    rotl_6(TF256 v) noexcept;
vec3_256 rotl_6(const vec3_256& v) noexcept;

template<typename T>
void store(TF* ptr, const T& value) noexcept;

template<typename T>
T load(const TF* ptr) noexcept;


template<typename T>
T add(const T& t1, const T& t2) noexcept;

template<typename T>
T sub(const T& t1, const T& t2) noexcept;

template<typename T>
T mul(const T& t1, const T& t2) noexcept;

template<typename T>
T div(const T& t1, const T& t2) noexcept;

template<typename T>
T sqrt(const T& t) noexcept;

template<typename T>
T rsqrt(const T& t) noexcept;

template<typename T>
vec3_t<T> add(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;

template<typename T>
vec3_t<T> sub(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;

template<typename T>
vec3_t<T> mul(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;

template<typename T>
vec3_t<T> div(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;


template<typename T>
vec3_t<T> operator+(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;

template<typename T>
vec3_t<T> operator-(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;

template<typename T>
vec3_t<T> operator*(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;

template<typename T>
vec3_t<T> operator/(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;


template<typename T>
vec3_t<T> normalize(const vec3_t<T>& v) noexcept;

template<typename T>
T dot(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;

} // namespace hml

// float hml_hsum128(__m128 v) noexcept {
//     __m128 shuf = _mm_movehdup_ps(v);        // broadcast elements 3,1 to 2,0
//     __m128 sums = _mm_add_ps(v, shuf);
//     shuf        = _mm_movehl_ps(shuf, sums); // high half -> low half
//     sums        = _mm_add_ss(sums, shuf);
//     return        _mm_cvtss_f32(sums);
// }
//
//
// float hml_hsum256(__m256 v) noexcept {
//     __m128 vlow  = _mm256_castps256_ps128(v);
//     __m128 vhigh = _mm256_extractf128_ps(v, 1); // high 128
//            vlow  = _mm_add_ps(vlow, vhigh);     // add the low 128
//     return hml_hsum128(vlow);
// }


#endif
