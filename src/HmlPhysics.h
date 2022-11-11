#ifndef HML_PHYSICS
#define HML_PHYSICS

#include <vector>
#include <array>
#include <cmath>
#include <optional>
#include <iostream> // TODO remove

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


struct HmlPhysics {
    struct Object {
        enum class Type {
            Sphere, Box
        } type;

        inline bool isSphere() const noexcept { return type == Type::Sphere; }
        inline bool isBox()  const noexcept { return type == Type::Box; }


        // Possibly stores redundant data, but this way we can generalize across all shapes
        std::array<float, 6> data;
        float mass = 1.0f;
        // Speed. std::nullopt means the object is permanently stationary
        std::optional<glm::vec3> v = std::nullopt;
        inline bool isStationary() const noexcept  { return !v.has_value(); }


        // TODO reinterpret_cast on the whole object, having it pad the missing data[]
        struct Sphere {
            glm::vec3& center;
            float& radius;
            // glm::vec3 center;
            // float radius;
        };
        struct Box {
            glm::vec3 start;
            glm::vec3 finish;

            inline glm::vec3 dimensions() const noexcept { return finish - start; }
        };

        inline Sphere asSphere() noexcept {
            assert(isSphere() && "::> Trying to convert Object to Sphere but it is not one!");
            return Sphere{
                .center = *reinterpret_cast<glm::vec3*>(&data[0]),
                .radius = *reinterpret_cast<float*>(&data[3]),
                // .center = { data[0], data[1], data[2] },
                // .radius = data[3],
            };
        }
        inline Box asBox() const noexcept {
            assert(isBox() && "::> Trying to convert Object to Box but it is not one!");
            return Box{
                .start  = { data[0], data[1], data[2] },
                .finish = { data[3], data[4], data[5] },
            };
        }

        inline static Object createSphere(glm::vec3 center, float radius) noexcept {
            Object object;
            object.type = Type::Sphere;
            object.data = { center.x, center.y, center.z, radius };
            return object;
        }
        inline static Object createBox(glm::vec3 start, glm::vec3 finish) noexcept {
            Object object;
            object.type = Type::Box;
            object.data = { start.x, start.y, start.z, finish.x, finish.y, finish.z };
            return object;
        }
    };


    // Positive means no interaction
    // abs(Negative) denotes intersection length
    // NOTE optimization: calc sqrt only when there for sure is an intersection
    static float interact(const Object::Sphere& s1, const Object::Sphere& s2) noexcept {
        return glm::distance(s1.center, s2.center) - (s1.radius + s2.radius);
    }


    // TODO use reinterpret_cast to store reference in Sphere to data[] as pos&
    inline void updateForDt(float dt) noexcept {
        // Apply interaction onto velocity change
        for (size_t i = 0; i < objects.size(); i++) {
            auto& obj1 = objects[i];
            for (size_t j = i + 1; j < objects.size(); j++) {
                auto& obj2 = objects[j];

                if (obj1.isStationary() && obj2.isStationary()) continue;

                if (obj1.isSphere() && obj2.isSphere()) {
                    const auto s1 = obj1.asSphere();
                    const auto s2 = obj2.asSphere();

                    const float d = interact(s1, s2);
                    if (d >= 0.0f) continue; // no interaction
                    const float penetration = -d; // positive

                    const float extent1 = penetration / s1.radius;
                    const float extent2 = penetration / s2.radius;
                    static const auto forceFunc = [](float arg){
                        const float mult = 100.0f;
                        return mult * arg * arg;
                    };

                    const auto forceOn1From2 = forceFunc(extent1) * forceFunc(extent2) * glm::normalize(s1.center - s2.center);
                    const auto forceOn2From1 = -forceOn1From2;

                    if (!obj1.isStationary()) *(obj1.v) += forceOn1From2 / obj1.mass * dt;
                    if (!obj2.isStationary()) *(obj2.v) += forceOn2From1 / obj2.mass * dt;

                    // if (obj1.isStationary()) {
                    //     s2.center += penetration * glm::normalize(obj2.v);
                    //     obj2.v = -obj2.v; // TODO dissipation
                    // } else if (obj2.isStationary()) {
                    //     s1.center += penetration * glm::normalize(obj1.v);
                    //     obj1.v = -obj1.v; // TODO dissipation
                    // } else { // both in motion
                    //
                    // }
                } else {
                    // TODO
                }
            }
        }

        // Apply velocity onto position change
        for (auto& obj : objects) {
            if (obj.isSphere()) {
                obj.asSphere().center += obj.v.value_or(glm::vec3{0,0,0}) * dt;
            } else {
                // TODO
            }
        }
    }

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
};


#endif
