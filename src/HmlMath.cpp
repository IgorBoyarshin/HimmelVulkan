#include "HmlMath.h"


namespace hml {

template<typename T>
vec3_t<T>::vec3_t() noexcept {}

template<typename T>
vec3_t<T>::vec3_t(T a) noexcept
    : x(a), y(a), z(a) {}

template<typename T>
vec3_t<T>::vec3_t(T x, T y, T z) noexcept
    : x(x), y(y), z(z) {}

template<typename T>
vec3_t<T>::vec3_t(const TF* xsPtr, const TF* ysPtr, const TF* zsPtr) noexcept
    : x(load<T>(xsPtr)), y(load<T>(ysPtr)), z(load<T>(zsPtr)) {}
// ============================================================================
template<typename T>
void vec3_t<T>::store(TF* xsPtr, TF* ysPtr, TF* zsPtr) const noexcept {
    hml::store<T>(xsPtr, x);
    hml::store<T>(ysPtr, y);
    hml::store<T>(zsPtr, z);
}
// ============================================================================
template<>
void store(TF* ptr, const TF& value) noexcept {
    *ptr = value;
}

template<>
void store(TF* ptr, const TF128& value) noexcept {
    _mm_store_ps(ptr, value);
}

template<>
void store(TF* ptr, const TF256& value) noexcept {
    _mm256_store_ps(ptr, value);
}
// ============================================================================
template<>
TF load(const TF* ptr) noexcept {
    return *ptr;
}

template<>
TF128 load(const TF* ptr) noexcept {
    return _mm_load_ps(ptr);
}

template<>
TF256 load(const TF* ptr) noexcept {
    return _mm256_load_ps(ptr);
}
// ============================================================================
template<>
TF add(const TF& t1, const TF& t2) noexcept {
    return t1 + t2;
}

template<>
TF128 add(const TF128& t1, const TF128& t2) noexcept {
    return _mm_add_ps(t1, t2);
}

template<>
TF256 add(const TF256& t1, const TF256& t2) noexcept {
    return _mm256_add_ps(t1, t2);
}
// ============================================================================
template<>
TF sub(const TF& t1, const TF& t2) noexcept {
    return t1 - t2;
}

template<>
TF128 sub(const TF128& t1, const TF128& t2) noexcept {
    return _mm_sub_ps(t1, t2);
}

template<>
TF256 sub(const TF256& t1, const TF256& t2) noexcept {
    return _mm256_sub_ps(t1, t2);
}
// ============================================================================
template<>
TF mul(const TF& t1, const TF& t2) noexcept {
    return t1 * t2;
}

template<>
TF128 mul(const TF128& t1, const TF128& t2) noexcept {
    return _mm_mul_ps(t1, t2);
}

template<>
TF256 mul(const TF256& t1, const TF256& t2) noexcept {
    return _mm256_mul_ps(t1, t2);
}
// ============================================================================
template<>
TF div(const TF& t1, const TF& t2) noexcept {
    return t1 / t2;
}

template<>
TF128 div(const TF128& t1, const TF128& t2) noexcept {
    return _mm_div_ps(t1, t2);
}

template<>
TF256 div(const TF256& t1, const TF256& t2) noexcept {
    return _mm256_div_ps(t1, t2);
}
// ============================================================================
template<>
TF sqrt(const TF& t) noexcept {
    return std::sqrt(t);
}

template<>
TF128 sqrt(const TF128& t) noexcept {
    return _mm_sqrt_ps(t);
}

template<>
TF256 sqrt(const TF256& t) noexcept {
    return _mm256_sqrt_ps(t);
}
// ============================================================================
template<>
TF rsqrt(const TF& t) noexcept {
    return 1.0f / std::sqrt(t);
}

template<>
TF128 rsqrt(const TF128& t) noexcept {
    return _mm_rsqrt_ps(t);
}

template<>
TF256 rsqrt(const TF256& t) noexcept {
    return _mm256_rsqrt_ps(t);
}
// ============================================================================
// template<>
// vec3 add(const vec3& v1, const vec3& v2) noexcept {
//     return vec3(
//         add(v1.x, v2.x),
//         add(v1.y, v2.y),
//         add(v1.z, v2.z));
// }

// template<>
// vec3_128 add(const vec3_128& v1, const vec3_128& v2) noexcept {
//     return vec3_128(
//         add(v1.x, v2.x),
//         add(v1.y, v2.y),
//         add(v1.z, v2.z));
// }

// template<>
// vec3_256 add(const vec3_256& v1, const vec3_256& v2) noexcept {
//     return vec3_256(
//         add(v1.x, v2.x),
//         add(v1.y, v2.y),
//         add(v1.z, v2.z));
// }
// ============================================================================
template<typename T>
vec3_t<T> add(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return vec3_t<T>(
        add(v1.x, v2.x),
        add(v1.y, v2.y),
        add(v1.z, v2.z));
}

template<typename T>
vec3_t<T> sub(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return vec3_t<T>(
        sub(v1.x, v2.x),
        sub(v1.y, v2.y),
        sub(v1.z, v2.z));
}

template<typename T>
vec3_t<T> mul(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return vec3_t<T>(
        mul(v1.x, v2.x),
        mul(v1.y, v2.y),
        mul(v1.z, v2.z));
}

template<typename T>
vec3_t<T> div(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return vec3_t<T>(
        div(v1.x, v2.x),
        div(v1.y, v2.y),
        div(v1.z, v2.z));
}
// ============================================================================
template<typename T>
vec3_t<T> operator+(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return add(v1, v2);
}

template<typename T>
vec3_t<T> operator-(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return sub(v1, v2);
}

template<typename T>
vec3_t<T> operator*(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return mul(v1, v2);
}

template<typename T>
vec3_t<T> operator/(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return div(v1, v2);
}

template vec3     operator+(const vec3&     v1, const vec3&     v2) noexcept;
template vec3_128 operator+(const vec3_128& v1, const vec3_128& v2) noexcept;
template vec3_256 operator+(const vec3_256& v1, const vec3_256& v2) noexcept;
template vec3     operator-(const vec3&     v1, const vec3&     v2) noexcept;
template vec3_128 operator-(const vec3_128& v1, const vec3_128& v2) noexcept;
template vec3_256 operator-(const vec3_256& v1, const vec3_256& v2) noexcept;
template vec3     operator*(const vec3&     v1, const vec3&     v2) noexcept;
template vec3_128 operator*(const vec3_128& v1, const vec3_128& v2) noexcept;
template vec3_256 operator*(const vec3_256& v1, const vec3_256& v2) noexcept;
template vec3     operator/(const vec3&     v1, const vec3&     v2) noexcept;
template vec3_128 operator/(const vec3_128& v1, const vec3_128& v2) noexcept;
template vec3_256 operator/(const vec3_256& v1, const vec3_256& v2) noexcept;
// ============================================================================
template<typename T>
vec3_t<T> normalize(const vec3_t<T>& v) noexcept {
    vec3_t<T> sqr = mul(v, v);
    auto rsqrt = hml::rsqrt(add(add(sqr.x, sqr.y), sqr.z));
    return vec3_t<T>(mul(v.x, rsqrt), mul(v.y, rsqrt), mul(v.z, rsqrt));
}

template vec3     normalize(const vec3&     v) noexcept;
template vec3_128 normalize(const vec3_128& v) noexcept;
template vec3_256 normalize(const vec3_256& v) noexcept;

template<typename T>
T dot(const vec3_t<T>& v1, const vec3_t<T>& v2) noexcept {
    return add(add(
        mul(v1.x, v2.x),
        mul(v1.y, v2.y)),
        mul(v1.z, v2.z));
}

template TF    dot(const vec3&     v1, const vec3&     v2) noexcept;
template TF128 dot(const vec3_128& v1, const vec3_128& v2) noexcept;
template TF256 dot(const vec3_256& v1, const vec3_256& v2) noexcept;
// ============================================================================
template class vec3_t<TF>;
template class vec3_t<TF128>;
template class vec3_t<TF256>;

}
