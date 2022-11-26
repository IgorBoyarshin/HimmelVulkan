#include "HmlMath.h"

namespace hml {

template<typename T>
vec3_t<T>::vec3_t() noexcept {}

template<typename T>
vec3_t<T>::vec3_t(T x, T y, T z) noexcept : x(x), y(y), z(z) {}

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
T add(const T& v1, const T& v2) noexcept {
    return vec3<T>(
        add(v1.x, v2.x),
        add(v1.y, v2.y),
        add(v1.z, v2.z));
}

template<typename T>
T sub(const T& v1, const T& v2) noexcept {
    return vec3<T>(
        sub(v1.x, v2.x),
        sub(v1.y, v2.y),
        sub(v1.z, v2.z));
}

template<typename T>
T mul(const T& v1, const T& v2) noexcept {
    return vec3<T>(
        mul(v1.x, v2.x),
        mul(v1.y, v2.y),
        mul(v1.z, v2.z));
}

template<typename T>
T div(const T& v1, const T& v2) noexcept {
    return vec3<T>(
        div(v1.x, v2.x),
        div(v1.y, v2.y),
        div(v1.z, v2.z));
}
// template<>
// vec3 sub(const vec3& v1, const vec3& v2) noexcept {
//     return vec3(
//         v1.x - v2.x,
//         v1.y - v2.y,
//         v1.z - v2.z);
// }

// template<>
// vec3_128 sub(const vec3_128& v1, const vec3_128& v2) noexcept {
//     return vec3_128(
//         _mm_sub_ps(v1.x, v2.x),
//         _mm_sub_ps(v1.y, v2.y),
//         _mm_sub_ps(v1.z, v2.z));
// }

// template<>
// vec3_256 sub(const vec3_256& v1, const vec3_256& v2) noexcept {
//     return vec3_256(
//         _mm256_sub_ps(v1.x, v2.x),
//         _mm256_sub_ps(v1.y, v2.y),
//         _mm256_sub_ps(v1.z, v2.z));
// }
// ============================================================================
// template<>
// vec3 mul(const vec3& v1, const vec3& v2) noexcept {
//     return vec3(
//         v1.x * v2.x,
//         v1.y * v2.y,
//         v1.z * v2.z);
// }

// template<>
// vec3_128 mul(const vec3_128& v1, const vec3_128& v2) noexcept {
//     return vec3_128(
//         _mm_mul_ps(v1.x, v2.x),
//         _mm_mul_ps(v1.y, v2.y),
//         _mm_mul_ps(v1.z, v2.z));
// }

// template<>
// vec3_256 mul(const vec3_256& v1, const vec3_256& v2) noexcept {
//     return vec3_256(
//         _mm256_mul_ps(v1.x, v2.x),
//         _mm256_mul_ps(v1.y, v2.y),
//         _mm256_mul_ps(v1.z, v2.z));
// }
// ============================================================================
// template<>
// vec3 div(const vec3& v1, const vec3& v2) noexcept {
//     return vec3(
//         v1.x / v2.x,
//         v1.y / v2.y,
//         v1.z / v2.z);
// }

// template<>
// vec3_128 div(const vec3_128& v1, const vec3_128& v2) noexcept {
//     return vec3_128(
//         _mm_div_ps(v1.x, v2.x),
//         _mm_div_ps(v1.y, v2.y),
//         _mm_div_ps(v1.z, v2.z));
// }

// template<>
// vec3_256 div(const vec3_256& v1, const vec3_256& v2) noexcept {
//     return vec3_256(
//         _mm256_div_ps(v1.x, v2.x),
//         _mm256_div_ps(v1.y, v2.y),
//         _mm256_div_ps(v1.z, v2.z));
// }
// ============================================================================
template<>
vec3 normalize(const vec3& v) noexcept {
    const float denom = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    return vec3(v.x / denom, v.y / denom, v.z / denom);
}

template<>
vec3_128 normalize(const vec3_128& v) noexcept {
    vec3_128 sqr = mul(v, v);
    TF128 denom = _mm_rsqrt_ps(add(add(sqr.x, sqr.y), sqr.z));
    return vec3_128(mul(v.x, denom), mul(v.y, denom), mul(v.z, denom));
}

// template<>
// T dot(const vec3_t<T>& v) noexcept;
// ============================================================================

}