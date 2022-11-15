#ifndef HML_PHYSICS
#define HML_PHYSICS

#include <vector>
#include <array>
#include <cmath>
#include <optional>
#include <iostream>
#include <limits>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


std::ostream& operator<<(std::ostream& stream, const glm::vec3& v);

namespace std {
    glm::vec3 abs(const glm::vec3& v);
    // glm::vec3 copysign(const glm::vec3& v, const glm::vec3& sign);
    float absmax(const glm::vec3& v);
}

namespace glm {
    bool operator<(const glm::vec3& v1, const glm::vec3& v2);
    bool operator>(const glm::vec3& v1, const glm::vec3& v2);
}


struct HmlPhysics {
    struct Object {
        // XXX this must be the first object data to allow reinterpret_cast to work properly.
        // Possibly stores redundant data, but this way we can generalize across all shapes
        glm::vec3 position;
        std::array<float, 3> data;

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

        std::optional<DynamicProperties> dynamicProperties;
        inline bool isStationary() const noexcept  { return !dynamicProperties.has_value(); }


        struct AABB {
            glm::vec3 begin;
            glm::vec3 end;
        };
        inline AABB aabb() const noexcept {
            switch (type) {
                case Type::Box:    return asBox().aabb();
                case Type::Sphere: return asSphere().aabb();
                default: assert(false && "Unimplemented Object type."); return AABB{}; // stub
            }
        }


        struct Sphere {
            glm::vec3 center;
            float radius;

            inline AABB aabb() const noexcept {
                return AABB{
                    .begin = center - glm::vec3{radius},
                    .end   = center + glm::vec3{radius},
                };
            }
        };
        static_assert(sizeof(Sphere) <= sizeof(position) + sizeof(data) && "Please allocate larger data[] in Object because Sphere doesn't fit.");

        struct Box {
            glm::vec3 center;
            glm::vec3 halfDimensions;

            inline glm::vec3 dimensions() const noexcept { return halfDimensions * 2.0f; }
            inline AABB aabb() const noexcept {
                return AABB{
                    .begin = center - halfDimensions,
                    .end   = center + halfDimensions,
                };
            }
        };
        static_assert(sizeof(Box) <= sizeof(position) + sizeof(data) && "Please allocate larger data[] in Object because Box doesn't fit.");

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
            object.position = { center.x, center.y, center.z };
            object.data = { radius };
            return object;
        }
        inline static Object createBox(const glm::vec3& center, const glm::vec3& halfDimensions) noexcept {
            Object object;
            object.type = Type::Box;
            object.position = { center.x, center.y, center.z };
            object.data = { halfDimensions.x, halfDimensions.y, halfDimensions.z };
            return object;
        }
    };


    struct Detection {
        glm::vec3 dir;
        float extent;
    };

    // Returns dir from arg1 towards arg2
    template<typename Arg1, typename Arg2>
    static std::optional<Detection> detect(const Arg1& arg1, const Arg2& arg2) noexcept;

    static void resolveVelocities(Object& obj1, Object& obj2, const Detection& detection) noexcept;
    static void process(Object& obj1, Object& obj2) noexcept;
    // ========================================================================
    void updateForDt(float dt) noexcept;
    // ========================================================================
    using Id = uint32_t;
    inline static constexpr Id INVALID_ID = 0;

    Id registerObject(Object&& object) noexcept;
    Object& getObject(Id id) noexcept;

    std::vector<Object> objects;
    std::vector<Id> ids;
    // ========================================================================
    struct LineIntersectsTriangleResult {
        float t;
        float u;
        float v;
        glm::vec3 N;
    };
    static std::optional<LineIntersectsTriangleResult> lineIntersectsTriangle(
        const glm::vec3& start, const glm::vec3& finish,
        const glm::vec3& A, const glm::vec3& B, const glm::vec3& C) noexcept;
    // ========================================================================
    static float pointToLineDstSqr(const glm::vec3& point, const glm::vec3& l1, const glm::vec3& l2) noexcept;
};


#endif
