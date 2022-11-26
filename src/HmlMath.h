#ifndef HML_MATH
#define HML_MATH

#include <cmath>
#include <immintrin.h>

namespace hml {

template<typename T>
struct vec3_t {
    T x, y, z;

    vec3_t() noexcept;
    vec3_t(T x, T y, T z) noexcept;
};


using TF    = float;
using TF128 = __m128;
using TF256 = __m256;
using vec3     = vec3_t<TF>;
using vec3_128 = vec3_t<TF128>;
using vec3_256 = vec3_t<TF256>;


template<typename T>
T add(const T& t1, const T& t2) noexcept;

template<typename T>
T sub(const T& t1, const T& t2) noexcept;

template<typename T>
T mul(const T& t1, const T& t2) noexcept;

template<typename T>
T div(const T& t1, const T& t2) noexcept;

template<typename T>
T sqrt(const T& t1, const T& t2) noexcept;

template<typename T>
T rsqrt(const T& t1, const T& t2) noexcept;

template<typename T>
vec3_t<T> add(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;

template<typename T>
vec3_t<T> sub(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;

template<typename T>
vec3_t<T> mul(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;

template<typename T>
vec3_t<T> div(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept;


template<typename T>
inline vec3_t<T> operator+(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return add(v1, v2);
}

template<typename T>
inline vec3_t<T> operator-(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return sub(v1, v2);
}

template<typename T>
inline vec3_t<T> operator*(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return mul(v1, v2);
}

template<typename T>
inline vec3_t<T> operator/(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return div(v1, v2);
}


template<typename T>
vec3_t<T> normalize(const vec3_t<T>& v) noexcept;

template<typename T>
T dot(const vec3_t<T>& v) noexcept;

} // namespace hml


#endif