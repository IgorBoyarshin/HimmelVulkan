#ifndef HML_PHYSICS
#define HML_PHYSICS

#include <vector>
#include <array>
#include <cmath>
#include <optional>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <map>
#include <memory>
#include <bitset>
#include <chrono>
#include <span>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>


std::ostream& operator<<(std::ostream& stream, const glm::vec3& v);
std::ostream& operator<<(std::ostream& stream, const glm::quat& q);

namespace std {
    glm::vec3 abs(const glm::vec3& v);
    float absmax(const glm::vec3& v);
    float min(const glm::vec3& v);
}

namespace glm {
    bool operator<(const glm::vec3& v1, const glm::vec3& v2);
    bool operator>(const glm::vec3& v1, const glm::vec3& v2);
    // glm::mat4 cross(const glm::quat& q, const glm::mat4& m);
    // glm::mat4 cross(const glm::mat4& m, const glm::quat& q);
}



struct AxisAngle {
    float angle;
    glm::vec3 axis;
};
inline static AxisAngle quatToAxisAngle(const glm::quat& q) noexcept {
    glm::vec3 axis{0,0,0};
    const auto angle = 2 * glm::acos(q.w);
    if (q.w != 1.0f) {
        const auto invSinHalfAngle = 1.0f / glm::sin(angle * 0.5f);
        axis.x = q.x * invSinHalfAngle;
        axis.y = q.y * invSinHalfAngle;
        axis.z = q.z * invSinHalfAngle;
    }
    return AxisAngle{
        .angle = angle,
        .axis = axis,
    };
}
inline std::ostream& operator<<(std::ostream& stream, const AxisAngle& aa) {
    stream << aa.axis << "{" << aa.angle << "}";
    return stream;
}


inline static glm::mat3 quatToMat3(const glm::quat& q) noexcept {
    const auto& q0 = q.w;
    const auto& q1 = q.x;
    const auto& q2 = q.y;
    const auto& q3 = q.z;
    return glm::mat3{
        1.0f - 2*q2*q2 - 2*q3*q3, 2*q1*q2 - 2*q0*q3, 2*q1*q3 + 2*q0*q2,
        2*q1*q2 + 2*q0*q3, 1.0f - 2*q1*q1 - 2*q3*q3, 2*q2*q3 - 2*q0*q1,
        2*q1*q3 - 2*q0*q2, 2*q2*q3 + 2*q0*q1, 1.0f - 2*q1*q1 - 2*q2*q2
    };
}


struct HmlPhysics {
    struct Object {
        // XXX this must be the first object data to allow reinterpret_cast to work properly.
        // Possibly stores redundant data, but this way we can generalize across all shapes
        glm::vec3 position;
        std::array<float, 3> dimensions;
        mutable std::optional<glm::mat4> modelMatrixCached;
        glm::quat orientation = glm::quat(1, 0, 0, 0); // unit = quat(w, x, y, z)
        // glm::quat orientation = glm::rotate(glm::quat(1, 0, 0, 0), 1.0f, glm::vec3(0,0,1)); // unit = quat(w, x, y, z)


        enum class Type {
            Sphere, Box
        } type;

        using Id = uint32_t;
        inline static constexpr Id INVALID_ID = 0;
        Id id;
        inline static Id generateId() noexcept {
            static Id nextIdToUse = 1;
            return nextIdToUse++;
        }

        inline Object(Type type) : type(type), id(generateId()) {}

        inline bool isSphere() const noexcept { return type == Type::Sphere; }
        inline bool isBox()    const noexcept { return type == Type::Box; }


        inline void stuff() {
            // dynamicProperties->angularVelocity = angularMomentum * invMass;
            // const auto& [x,y,z] = dynamicProperties->angularVelocity;
            // const auto w = glm::quat(0, x, y, z);
            // // NOTE can be done only once in a while, not every frame
            // orientation = glm::normalize(orientation); // to ensure that it still represents rotation
            // const auto spin = 0.5f * glm::cross(w, orientation);

            // axis, damping factor
            // const glm::vec3 torque = glm::vec3{1,0,0} - angularVelocity * 0.1f;


            // World-space inverse inertia tensor
            // const auto I = object->orientation * object->dynamicProperties->invRotationalInertiaTensor * glm::conjugate(object->orientation);
            // Angular velocity = I * angular momentum
            // const auto w = I * object->dynamicProperties->angularMomentum;

            // Calcualte forces F and application points r
            // const auto r = contactPoint - obj.center;
            // F = sum(Fi); torque = sum(glm::cross(ri, Fi))
            // v_point = v_linear + cross(v_angular, lever)

            // Integrate
            // object->position += subDt * object->dynamicProperties->velocity;
            // object->dynamicProperties->velocity += subDt * F * object->dynamicProperties->invMass;
            // object->orientation = object->orientation + subDt * angularVelocity * object->orientation;
            // object->dynamicProperties->angularMomentum += subDt * torque;
            // orientation = glm::normalize(orientation); // to ensure that it still represents rotation
        }


        inline const glm::mat4& modelMatrix() const noexcept {
            if (!modelMatrixCached) modelMatrixCached = generateModelMatrix();
            return *modelMatrixCached;
        }

        inline glm::mat4 generateModelMatrix() const noexcept {
            auto modelMatrix = glm::mat4(1.0f);
            switch (type) {
                case Type::Box: {
                    const auto& b = asBox();
                    modelMatrix = glm::translate(modelMatrix, b.center);
                    modelMatrix = modelMatrix * glm::mat4_cast(orientation);
                    modelMatrix = glm::scale(modelMatrix, glm::vec3(b.halfDimensions));
                    // modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(1, 0, 0));
                    break;
                }
                case Type::Sphere: {
                    const auto& s = asSphere();
                    modelMatrix = glm::translate(modelMatrix, s.center);
                    modelMatrix = modelMatrix * glm::mat4_cast(orientation);
                    modelMatrix = glm::scale(modelMatrix, glm::vec3(s.radius));
                    // modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(1, 0, 0));
                    break;
                }
                default: assert(false && "::> Unimplemented object type.");
            }
            return modelMatrix;
        }


        struct AABB {
            glm::vec3 begin;
            glm::vec3 end;

            inline bool intersects(const AABB& other) const noexcept {
                return (begin.x       < other.end.x &&
                        other.begin.x < end.x       &&
                        begin.y       < other.end.y &&
                        other.begin.y < end.y       &&
                        begin.z       < other.end.z &&
                        other.begin.z < end.z);
            }
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
            float _stub1;
            float _stub2;
            std::optional<glm::mat4> modelMatrixCached;

            inline AABB aabb() const noexcept {
                return AABB{
                    .begin = center - glm::vec3{radius},
                    .end   = center + glm::vec3{radius},
                };
            }

            inline glm::mat3 rotationalInertiaTensor(float mass) const noexcept {
                const float m = 2.0f / 5.0f * mass * radius * radius;
                return glm::mat3{
                    m, 0, 0,
                    0, m, 0,
                    0, 0, m
                };
            }
        };
        static_assert(sizeof(Sphere) == sizeof(position) + sizeof(dimensions) + sizeof(modelMatrixCached) && "Please allocate larger dimensions[] in Object because Sphere doesn't fit.");

        struct Box {
            glm::vec3 center;
            glm::vec3 halfDimensions;
            std::optional<glm::mat4> modelMatrixCached;

            struct OrientationData {
                glm::vec3 i, j, k;
            };

            OrientationData orientationDataUnnormalized() const noexcept {
                const auto& mm = asObject().modelMatrix();
                return OrientationData{
                    .i = glm::vec3{ mm[0][0], mm[0][1], mm[0][2] },
                    .j = glm::vec3{ mm[1][0], mm[1][1], mm[1][2] },
                    .k = glm::vec3{ mm[2][0], mm[2][1], mm[2][2] }
                };
            };

            std::array<glm::vec3, 8> toPoints() const noexcept {
                const auto& [i, j, k] = orientationDataUnnormalized();
                std::array<glm::vec3, 8> ps;
                size_t indexP = 0;
                for (float a = -1; a <= 1; a += 2) {
                    for (float b = -1; b <= 1; b += 2) {
                        for (float c = -1; c <= 1; c += 2) {
                            ps[indexP++] = (a * i + b * j + c * k) + center;
                        }
                    }
                }
                assert(indexP == 8);
                return ps;
            }

            inline glm::vec3 dimensions() const noexcept { return halfDimensions * 2.0f; }
            inline AABB aabb() const noexcept {
                return AABB{
                    .begin = center - halfDimensions,
                    .end   = center + halfDimensions,
                };
            }

            inline glm::mat3 rotationalInertiaTensor(float mass) const noexcept {
                const auto dimSqr = 4.0f * halfDimensions * halfDimensions;
                const float m = mass / 12.0f;
                return glm::mat3{
                    m * (dimSqr.y + dimSqr.z), 0, 0,
                    0, m * (dimSqr.x + dimSqr.y), 0,
                    0, 0, m * (dimSqr.x + dimSqr.z)
                };
            }

            inline       Object& asObject()       noexcept { return *reinterpret_cast<      Object*>(this); }
            inline const Object& asObject() const noexcept { return *reinterpret_cast<const Object*>(this); }
        };
        static_assert(sizeof(Box) == sizeof(position) + sizeof(dimensions) + sizeof(modelMatrixCached) && "Please allocate larger dimensions[] in Object because Box doesn't fit.");

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
            Object object{Type::Sphere};
            object.position = { center.x, center.y, center.z };
            object.dimensions = { radius };
            return object;
        }
        inline static Object createSphere(const glm::vec3& center, float radius,
                float mass,
                const glm::vec3& velocity, const glm::vec3& angularVelocity) noexcept {
            Object object{Type::Sphere};
            object.position = { center.x, center.y, center.z };
            object.dimensions = { radius };
            const auto rotationalInertiaTensor = object.asSphere().rotationalInertiaTensor(mass);
            object.dynamicProperties = DynamicProperties(mass, rotationalInertiaTensor, velocity, angularVelocity);
            return object;
        }
        inline static Object createBox(const glm::vec3& center, const glm::vec3& halfDimensions) noexcept {
            Object object{Type::Box};
            object.position = { center.x, center.y, center.z };
            object.dimensions = { halfDimensions.x, halfDimensions.y, halfDimensions.z };
            return object;
        }

        inline static Object createBox(const glm::vec3& center, const glm::vec3& halfDimensions,
                float mass,
                const glm::vec3& velocity, const glm::vec3& angularVelocity) noexcept {
            Object object{Type::Box};
            object.position = { center.x, center.y, center.z };
            object.dimensions = { halfDimensions.x, halfDimensions.y, halfDimensions.z };
            const auto rotationalInertiaTensor = object.asBox().rotationalInertiaTensor(mass);
            object.dynamicProperties = DynamicProperties(mass, rotationalInertiaTensor, velocity, angularVelocity);
            return object;
        }

        struct DynamicProperties {
            float mass = std::numeric_limits<float>::max();
            float invMass = 0.0f;

            glm::mat3 rotationalInertiaTensor; // TODO init
            glm::mat3 invRotationalInertiaTensor; // TODO init

            glm::vec3 velocity = glm::vec3{0};
            glm::vec3 angularMomentum = glm::vec3{0};

            inline DynamicProperties(float mass, const glm::mat3& rotationalInertiaTensor,
                    const glm::vec3& velocity, const glm::vec3& angularVelocity) noexcept
                : mass(mass), invMass(1.0f / mass),
                  rotationalInertiaTensor(rotationalInertiaTensor), invRotationalInertiaTensor(glm::inverse(rotationalInertiaTensor)),
                  velocity(velocity), angularMomentum(angularVelocity * rotationalInertiaTensor) {}
            inline DynamicProperties() noexcept {}
        };

        std::optional<DynamicProperties> dynamicProperties = std::nullopt;
        inline bool isStationary() const noexcept  { return !dynamicProperties.has_value(); }
    };


    struct Detection {
        glm::vec3 dir;
        float extent;
        std::vector<glm::vec3> contactPoints;
    };

    static std::optional<Detection> detectAxisAlignedBoxSphere(const Object::Box& b, const Object::Sphere& s) noexcept;
    static std::optional<Detection> detectOrientedBoxSphere(const Object::Box& b, const Object::Sphere& s) noexcept;
    static std::optional<Detection> detectAxisAlignedBoxes(const Object::Box& b1, const Object::Box& b2) noexcept;
    static std::optional<Detection> detectOrientedBoxesWithSat(const Object::Box& b1, const Object::Box& b2) noexcept;

    // Returns dir from arg1 towards arg2
    template<typename Arg1, typename Arg2>
    static std::optional<Detection> detect(const Arg1& arg1, const Arg2& arg2) noexcept;

    static void resolveVelocities(Object& obj1, Object& obj2, const Detection& detection) noexcept;
    static void process(Object& obj1, Object& obj2) noexcept;
    // ========================================================================
    struct Simplex {
        inline Simplex() noexcept : points({ glm::vec3{0}, glm::vec3{0}, glm::vec3{0}, glm::vec3{0} }), count(0) {}
        inline Simplex& operator=(std::initializer_list<glm::vec3> list) noexcept {
            for (auto v = list.begin(); v != list.end(); ++v) {
                points[std::distance(list.begin(), v)] = *v;
            }
            count = list.size();
            return *this;
        }

        inline void push_front(const glm::vec3& p) noexcept {
            points = { p, points[0], points[1], points[2] };
            count = std::min(count + 1, static_cast<size_t>(4));
        }

        inline glm::vec3& operator[](size_t i) noexcept { return points[i]; }
        inline const glm::vec3& operator[](size_t i) const noexcept { return points[i]; }
        inline size_t size() const noexcept { return count; }

        auto begin() const noexcept { return points.begin(); }
        auto end()   const noexcept { return points.end() - (4 - count); }

        private:
        std::array<glm::vec3, 4> points;
        size_t count;
    };

    static glm::vec3 furthestPointInDir(const glm::vec3& dir, std::span<const glm::vec3> ps) noexcept;
    static glm::vec3 calcSupport(const glm::vec3& dir, std::span<const glm::vec3> ps1, std::span<const glm::vec3> ps2) noexcept;
    static std::optional<Detection> gjk(const Object::Box& b1, const Object::Box& b2) noexcept;
    static std::optional<Detection> epa(const Simplex& simplex, const auto& ps1, const auto& ps2) noexcept;
    // ========================================================================
    void updateForDt(float dt) noexcept;
    // ========================================================================
    struct Bucket {
        inline static constexpr float SIZE = 8.0f;

        using Coord = int16_t;
        using Hash = uint64_t;
        using Bounding = std::pair<Bucket, Bucket>;

        Coord x;
        Coord y;
        Coord z;

        friend auto operator<=>(const Bucket&, const Bucket&) = default;

        inline static Bucket fromPos(const glm::vec3& pos) noexcept {
            return Bucket{
                .x = toCoord(pos.x),
                .y = toCoord(pos.y),
                .z = toCoord(pos.z),
            };
        }

        inline static Coord toCoord(float x) noexcept {
            return static_cast<Coord>(std::floor(x / SIZE));
        }

        inline bool isInsideBoundingBuckets(const Bounding& bounding) const noexcept {
            return (bounding.first.x <= x && x <= bounding.second.x &&
                    bounding.first.y <= y && y <= bounding.second.y &&
                    bounding.first.z <= z && z <= bounding.second.z);
        }
    };

    struct BucketHasher {
        inline size_t operator()(const Bucket& bucket) const {
            const HmlPhysics::Bucket::Hash comb =
                (static_cast<uint64_t>(static_cast<uint16_t>(bucket.x)) << 32) |
                (static_cast<uint64_t>(static_cast<uint16_t>(bucket.y)) << 16) |
                 static_cast<uint64_t>(static_cast<uint16_t>(bucket.z));
            return std::hash<HmlPhysics::Bucket::Hash>{}(comb);
        }
    };

    // NOTE can contain Buckets not present in any of the inputs
    static Bucket::Bounding boundingBucketsSum(const Bucket::Bounding& bb1, const Bucket::Bounding& bb2) noexcept;
    static Bucket::Bounding boundingBucketsForObject(const Object& object) noexcept;
    void reassign() noexcept;
    void removeObjectWithIdFromBucket(Object::Id id, const Bucket& bucket) noexcept;
    void printStats() const noexcept;

    Object::Id registerObject(Object&& object) noexcept;
    // Object& getObject(Object::Id id) noexcept;

    bool firstUpdateAfterLastRegister = true;
    std::unordered_map<Bucket, std::vector<std::shared_ptr<Object>>, BucketHasher> objectsInBuckets;
    std::vector<std::shared_ptr<Object>> objects;
    // Not to allocate every frame
    std::vector<Bucket::Bounding> allBoundingBucketsBefore;
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
    static std::optional<glm::vec3> linePlaneIntersection(
            const glm::vec3& linePoint, const glm::vec3& lineDirNorm,
            const glm::vec3& planePoint, const glm::vec3& planeDir) noexcept;
    static std::optional<glm::vec3> edgePlaneIntersection(
            const glm::vec3& linePoint1, const glm::vec3& linePoint2,
            const glm::vec3& planePoint, const glm::vec3& planeDir) noexcept;
};


#endif
