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
template<typename T>
std::ostream& operator<<(std::ostream& ostream, const vec3_t<T>& v) {
    ostream << "vec3(\n"
        << '\t' << v.x << ";\n"
        << '\t' << v.y << ";\n"
        << '\t' << v.z
    << ")";
    return ostream;
}

std::ostream& operator<<(std::ostream& ostream, const TF128& tf) {
    constexpr size_t SIZE = 4;
    alignas(16) float arr[SIZE];
    store(arr, tf);
    for (size_t i = 0; i < SIZE; i++) {
        ostream << '[' << arr[SIZE - 1 - i] << ']';
    }
    return ostream;
}

std::ostream& operator<<(std::ostream& ostream, const TF256& tf) {
    constexpr size_t SIZE = 8;
    alignas(32) float arr[SIZE];
    store(arr, tf);
    for (size_t i = 0; i < SIZE; i++) {
        ostream << '[' << arr[SIZE - 1 - i] << ']';
    }
    return ostream;
}
// ============================================================================
vec3_256 gather_from_vec3(const float* src, const __m256i& indices) noexcept {
    return vec3_256(
        _mm256_i32gather_ps(src + 0, indices, 4),
        _mm256_i32gather_ps(src + 1, indices, 4),
        _mm256_i32gather_ps(src + 2, indices, 4)
    );
}
// ============================================================================
TF256 rotl_6(TF256 v) noexcept {
    auto tmp = _mm256_permute2f128_ps(v, v, 1); // swap the two 128-bit lanes
    tmp = _mm256_permute_ps(tmp, 0b11); // places the first(highest) element on the last(lowest) position on each lane
    auto tmp2 = _mm256_castps128_ps256(_mm_permute_ps(_mm256_castps256_ps128(tmp), 0b00)); // place the last (lowest) element everywhere
    tmp = _mm256_blend_ps(tmp, tmp2, 0b00001111); // concatenate high lane from tmp and low lane from tmp2
    auto res = _mm256_castsi256_ps(_mm256_bslli_epi128(_mm256_castps_si256(v), 4)); // shift left by one element
    return _mm256_blend_ps(res, tmp, 0b00010100); // place the two elements from tmp into v
}

vec3_256 rotl_6(const vec3_256& v) noexcept {
    return vec3_256 ( rotl_6(v.x), rotl_6(v.y), rotl_6(v.z) );
}
// ============================================================================
template<typename T>
void vec3_t<T>::store(TF* xsPtr, TF* ysPtr, TF* zsPtr) const noexcept {
    hml::store<T>(xsPtr, x);
    hml::store<T>(ysPtr, y);
    hml::store<T>(zsPtr, z);
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
template std::ostream& operator<<(std::ostream& ostream, const vec3& v);
template std::ostream& operator<<(std::ostream& ostream, const vec3_128& v);
template std::ostream& operator<<(std::ostream& ostream, const vec3_256& v);
// ============================================================================
template struct vec3_t<TF>;
template struct vec3_t<TF128>;
template struct vec3_t<TF256>;

}
