#ifndef HML_PHYSICS
#define HML_PHYSICS

#include <vector>
#include <array>
#include <cmath>
#include <optional>
#include <iostream> // TODO remove
#include <limits>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


inline std::ostream& operator<<(std::ostream& stream, const glm::vec3& v) {
    stream << "[" << v.x << ";" << v.y << ";" << v.z << "]";
    return stream;
}

namespace std {
    inline glm::vec3 abs(const glm::vec3& v) {
        return glm::vec3{ std::abs(v.x), std::abs(v.y), std::abs(v.z) };
    }

    // inline glm::vec3 copysign(const glm::vec3& v, const glm::vec3& sign) {
    //     return glm::vec3{
    //         std::copysign(v.x, sign.x),
    //         std::copysign(v.y, sign.y),
    //         std::copysign(v.z, sign.z)
    //     };
    // }

    inline float absmax(const glm::vec3& v) {
        const auto absV = std::abs(v);
        float max = v.x;
        float absMax = absV.x;
        bool foundNew;

        foundNew = absV.y > absMax;
        absMax = absMax * (1 - foundNew) + absV.y * foundNew;
        max    = max    * (1 - foundNew) + v.y    * foundNew;

        foundNew = absV.z > absMax;
        absMax = absMax * (1 - foundNew) + absV.z * foundNew;
        max    = max    * (1 - foundNew) + v.z    * foundNew;

        return max;
    }
}


struct HmlPhysics {
    struct Object {
        // XXX this must be the first object data to allow reinterpret_cast to work properly
        // Possibly stores redundant data, but this way we can generalize across all shapes
        std::array<float, 6> data;

        enum class Type {
            Sphere, Box
        } type;

        inline bool isSphere() const noexcept { return type == Type::Sphere; }
        inline bool isBox()  const noexcept { return type == Type::Box; }


        struct DynamicProperties {
            float mass = std::numeric_limits<float>::max();
            float invMass = 0.0f; // used in most math
            glm::vec3 velocity = glm::vec3{};
            inline DynamicProperties(float m, const glm::vec3& v) noexcept : mass(m), invMass(1.0f / m), velocity(v) {}
            inline DynamicProperties() noexcept {}
        };
        // std::nullopt means the object is permanently stationary
        std::optional<DynamicProperties> dynamicProperties;
        inline bool isStationary() const noexcept  { return !dynamicProperties.has_value(); }


        struct Sphere {
            glm::vec3 center;
            float radius;
        };
        struct Box {
            glm::vec3 center;
            glm::vec3 halfDimensions;

            inline glm::vec3 dimensions() const noexcept { return halfDimensions * 2.0f; }
        };

        inline Sphere& asSphere() noexcept {
            assert(isSphere() && "::> Trying to convert Object to Sphere but it is not one!");
            return *reinterpret_cast<Sphere*>(this);
        }
        inline const Sphere& asSphere() const noexcept {
            assert(isSphere() && "::> Trying to convert Object to Sphere but it is not one!");
            return *reinterpret_cast<const Sphere*>(this);
        }
        inline Box& asBox() noexcept {
            assert(isBox() && "::> Trying to convert Object to Box but it is not one!");
            return *reinterpret_cast<Box*>(this);
        }
        inline const Box& asBox() const noexcept {
            assert(isBox() && "::> Trying to convert Object to Box but it is not one!");
            return *reinterpret_cast<const Box*>(this);
        }

        inline static Object createSphere(const glm::vec3& center, float radius) noexcept {
            Object object;
            object.type = Type::Sphere;
            object.data = { center.x, center.y, center.z, radius };
            return object;
        }
        inline static Object createBox(const glm::vec3& center, const glm::vec3& halfDimensions) noexcept {
            Object object;
            object.type = Type::Box;
            object.data = { center.x, center.y, center.z, halfDimensions.x, halfDimensions.y, halfDimensions.z };
            return object;
        }
    };


    struct Detection {
        glm::vec3 dir;
        float extent;
    };

    // Returns dir from s1 towards s2
    static std::optional<Detection> detect(const Object::Sphere& s1, const Object::Sphere& s2) noexcept {
        const auto c = s2.center - s1.center;
        const auto lengthC = glm::length(c);
        const float extent = (s1.radius + s2.radius) - lengthC;
        if (extent <= 0.0f) return std::nullopt;
        const auto normC = c / lengthC;
        return { Detection {
            .dir = normC,
            .extent = extent,
        }};
    }


    // Returns dir from b towards s
    static std::optional<Detection> detect(const Object::Box& b, const Object::Sphere& s) noexcept {
        const auto start  = b.center - b.halfDimensions - s.radius;
        const auto finish = b.center + b.halfDimensions + s.radius;
        if (start.x < s.center.x && s.center.x < finish.x &&
            start.y < s.center.y && s.center.y < finish.y &&
            start.z < s.center.z && s.center.z < finish.z) {
            // NOTE we care only about the predominant direction, and in terms of math the
            // radius component has no effect, no we omit it to simplify calculations.
            const auto centers = s.center - b.center;
            const auto n = centers / b.halfDimensions;
            const float max = std::absmax(n);
            const auto dir = glm::vec3{
                std::copysign(max == n.x, n.x),
                std::copysign(max == n.y, n.y),
                std::copysign(max == n.z, n.z),
            };
            const float extent = std::abs(glm::dot(b.halfDimensions, dir)) + s.radius - std::abs(glm::dot(centers, dir)); // NOTE second abs is actually redundant
            return { Detection{
                .dir = dir,
                .extent = extent,
            }};
        }

        return std::nullopt;
    }


    static void resolveVelocities(Object& obj1, Object& obj2, const Detection& detection) noexcept {
        const auto& [dir, extent] = detection;

        const auto obj1DP = obj1.dynamicProperties.value_or(Object::DynamicProperties{});
        const auto obj2DP = obj2.dynamicProperties.value_or(Object::DynamicProperties{});
        const auto relativeV = obj2DP.velocity - obj1DP.velocity;
        if (glm::dot(relativeV, dir) > 0.0f) {
            // Objects are already moving apart
            return;
        }
        // e == 0 --- perfectly inelastic collision
        // e == 1 --- perfectly elastic collision, no kinetic energy is dissipated
        // const float e = std::min(obj1.restitution, obj2.restitution);
        const float e = 1.0f; // restitution
        const float j = -(1.0f + e) * glm::dot(relativeV, dir) / (obj1DP.invMass + obj2DP.invMass);
        const auto impulse = j * dir;
        if (!obj1.isStationary()) obj1.dynamicProperties->velocity -= impulse * obj1DP.invMass;
        if (!obj2.isStationary()) obj2.dynamicProperties->velocity += impulse * obj2DP.invMass;
    }


    static void processHardSphereSphere(Object& obj1, Object& obj2, float dt) noexcept {
        assert(obj1.isSphere() && obj2.isSphere() && "::> Expected to process Sphere and Sphere.");

        auto& s1 = obj1.asSphere();
        auto& s2 = obj2.asSphere();

        const auto detectionOpt = detect(s1, s2);
        if (!detectionOpt) return;
        const auto& [dir, extent] = *detectionOpt;

        if (!obj1.isStationary()) s1.center -= dir * extent * (obj2.isStationary() ? 1.0f : 0.5f);
        if (!obj2.isStationary()) s2.center += dir * extent * (obj1.isStationary() ? 1.0f : 0.5f);

        resolveVelocities(obj1, obj2, *detectionOpt);
    }


    static void processHardBoxSphere(Object& obj1, Object& obj2, float dt) noexcept {
        assert(obj1.isBox() && obj2.isSphere() && "::> Expected to process Box and Sphere.");

        auto& b = obj1.asBox();
        auto& s = obj2.asSphere();

        const auto detectionOpt = detect(b, s);
        if (!detectionOpt) return;
        const auto& [dir, extent] = *detectionOpt;

        if (!obj1.isStationary()) b.center -= dir * extent * (obj2.isStationary() ? 1.0f : 0.5f);
        if (!obj2.isStationary()) s.center += dir * extent * (obj1.isStationary() ? 1.0f : 0.5f);

        resolveVelocities(obj1, obj2, *detectionOpt);
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    static float _interact(const Object::Sphere& s1, const Object::Sphere& s2) noexcept {
        return glm::distance(s1.center, s2.center) - (s1.radius + s2.radius);
    }


    static std::optional<glm::vec3> _interact(const Object::Sphere& s, const Object::Box& b) noexcept {
        const auto start  = b.center - b.halfDimensions - s.radius;
        const auto finish = b.center + b.halfDimensions + s.radius;
        if (start.x < s.center.x && s.center.x < finish.x &&
            start.y < s.center.y && s.center.y < finish.y &&
            start.z < s.center.z && s.center.z < finish.z) {
            const auto n = s.center - b.center;
            const auto n2 = glm::vec3{std::abs(n.x), std::abs(n.y), std::abs(n.z)};
            const auto norm = n2 / b.halfDimensions;
            const auto max = std::max(std::max(norm.x, norm.y), norm.z);

            return { glm::vec3{ // normalize to correctly output normal for equal directions
                // (magnitude, sign)
                std::copysign((max == norm.x) * (b.halfDimensions.x + s.radius - n2.x), n.x),
                std::copysign((max == norm.y) * (b.halfDimensions.y + s.radius - n2.y), n.y),
                std::copysign((max == norm.z) * (b.halfDimensions.z + s.radius - n2.z), n.z)
            }};
        }

        return std::nullopt;
    }


    static void _processSoftSphereSphere(Object& obj1, Object& obj2, float dt) noexcept {
        assert(obj1.isSphere() && obj2.isSphere() && "::> Expected to process Sphere and Sphere.");

        const auto& s1 = obj1.asSphere();
        const auto& s2 = obj2.asSphere();

        const float d = _interact(s1, s2);
        if (d >= 0.0f) return; // no interaction
        const float penetration = -d; // positive

        const float extent1 = penetration / s1.radius;
        const float extent2 = penetration / s2.radius;
        static const auto forceFunc = [](float arg){
            const float mult = 100.0f;
            return mult * arg * arg;
        };

        const auto forceOn1From2 = forceFunc(extent1) * forceFunc(extent2) * glm::normalize(s1.center - s2.center);
        const auto forceOn2From1 = -forceOn1From2;

        if (!obj1.isStationary()) obj1.dynamicProperties->velocity += forceOn1From2 * obj1.dynamicProperties->invMass * dt;
        if (!obj2.isStationary()) obj2.dynamicProperties->velocity += forceOn2From1 * obj2.dynamicProperties->invMass * dt;
    }


    static void _processSoftSphereBox(Object& obj1, Object& obj2, float dt) noexcept {
        assert(obj1.isSphere() && obj2.isBox() && "::> Expected to process Sphere and Box.");

        const auto& s = obj1.asSphere();
        const auto& b = obj2.asBox();

        const auto penOpt = _interact(s, b);
        if (!penOpt) return;
        const auto dir = *penOpt;

        static const auto forceFunc = [](const glm::vec3& arg){
            const float mult = 10000.0f;
            return mult * glm::vec3{
                std::copysign(arg.x * arg.x * arg.x, arg.x),
                std::copysign(arg.y * arg.y * arg.y, arg.y),
                std::copysign(arg.z * arg.z * arg.z, arg.z)
            };
        };

        const auto forceOnSFromB = forceFunc(dir) / s.radius / b.halfDimensions;
        const auto forceOnBFromS = -forceOnSFromB;

        if (!obj1.isStationary()) obj1.dynamicProperties->velocity += forceOnSFromB * obj1.dynamicProperties->invMass * dt;
        if (!obj2.isStationary()) obj2.dynamicProperties->velocity += forceOnBFromS * obj2.dynamicProperties->invMass * dt;
    }
    // ========================================================================
    inline void updateForDt(float dt) noexcept {
        // Apply velocity onto position change
        for (auto& obj : objects) {
            if (obj.isStationary()) continue;

            if (obj.isSphere()) {
                obj.asSphere().center += obj.dynamicProperties->velocity * dt;
            } else if (obj.isBox()) {
                obj.asBox().center += obj.dynamicProperties->velocity * dt;
            } else {
                // TODO
            }
        }

        // Check for and handle collisions
        for (size_t i = 0; i < objects.size(); i++) {
            auto& obj1 = objects[i];
            for (size_t j = i + 1; j < objects.size(); j++) {
                auto& obj2 = objects[j];

                if (obj1.isStationary() && obj2.isStationary()) continue;

                if      (obj1.isSphere() && obj2.isSphere()) processHardSphereSphere(obj1, obj2, dt);
                else if (obj1.isBox()    && obj2.isSphere()) processHardBoxSphere(obj1, obj2, dt);
                else if (obj1.isSphere() && obj2.isBox())    processHardBoxSphere(obj2, obj1, dt);
                else {
                    // TODO
                }
            }
        }
    }
    // ========================================================================
    using Id = uint32_t;
    static constexpr Id INVALID_ID = 0;

    inline Id registerObject(Object&& object) noexcept {
        objects.push_back(std::move(object));
        const auto id = objects.size();
        ids.push_back(id);
        return id;
    }

    inline Object& getObject(Id id) noexcept {
        for (size_t i = 0; i < objects.size(); i++) {
            if (ids[i] == id) return objects[i];
        }
        assert(false && "::> No Object with provided Id found.");
        return objects[0]; // stub
    }

    std::vector<Object> objects;
    std::vector<Id> ids;
    // ========================================================================
    // ========================================================================
    // ========================================================================
    struct LineIntersectsTriangleResult {
        float t;
        float u;
        float v;
        glm::vec3 N;
    };
    inline static std::optional<LineIntersectsTriangleResult> lineIntersectsTriangle(
            const glm::vec3& start, const glm::vec3& finish,
            const glm::vec3& A, const glm::vec3& B, const glm::vec3& C) noexcept {
        const float eps = 1e-5;
        const auto dir = finish - start;

        const glm::vec3 E1 = B - A;
        const glm::vec3 E2 = C - A;
        const glm::vec3 N = glm::cross(E1, E2);
        const float det = -glm::dot(dir, N);
        const float invdet = 1.0f / det;
        const glm::vec3 AO  = start - A;
        const glm::vec3 DAO = glm::cross(AO, dir);
        const float u =  glm::dot(E2, DAO) * invdet;
        const float v = -glm::dot(E1, DAO) * invdet;
        const float t =  glm::dot(AO, N)  * invdet;

        if (std::abs(det) >= eps && 1.0f >= t && t >= 0.0f && u >= 0.0f && v >= 0.0f && 1.0f >= (u + v)) {
            return { LineIntersectsTriangleResult{
                .t = t,
                .u = u,
                .v = v,
                .N = N,
            }};
        } else return std::nullopt;
    }
    // ========================================================================
    static float pointToLineDstSqr(const glm::vec3& point, const glm::vec3& l1, const glm::vec3& l2) noexcept {
        const auto nom = glm::cross(l1 - l2, l2 - point);
        const float nomSqr = glm::dot(nom, nom);
        const auto denom = (l1 - l2);
        const float denomSqr = glm::dot(denom, denom);
        return nomSqr / denomSqr;
    }
};


#endif
