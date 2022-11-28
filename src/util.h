#ifndef HML_UTIL
#define HML_UTIL

#include <random>


namespace hml {


// float, double, long double
// [low, high)
template<typename T = float>
inline T getRandomUniformFloat(T low, T high) noexcept {
    // static std::random_device rd;
    // static std::mt19937 e2(rd());
    static std::seed_seq seed{1, 2, 3, 300};
    static std::mt19937 e2(seed);
    std::uniform_real_distribution<T> dist(low, high);

    return dist(e2);
}


}


#endif
