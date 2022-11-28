#ifndef HML_PHYSICS
#define HML_PHYSICS

#include <vector>
#include <array>
#include <cmath>
#include <optional>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <memory>
#include <bitset>
#include <chrono>
#include <span>
#include <immintrin.h>

#include "HmlMath.h"

#include "../libs/ctpl_stl.h"


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
AxisAngle quatToAxisAngle(const glm::quat& q) noexcept;
std::ostream& operator<<(std::ostream& stream, const AxisAngle& aa);
glm::mat3 quatToMat3(const glm::quat& q) noexcept;


struct HmlPhysics {
    struct Object {
        // ============================================================
        // ============== Member types
        // ============================================================
        enum class Type {
            Sphere, Box
        };

        using Id = uint32_t;
        inline static constexpr Id INVALID_ID = 0;
        inline static Id generateId() noexcept {
            static Id nextIdToUse = 1;
            return nextIdToUse++;
        }

        struct DynamicProperties {
            float mass = std::numeric_limits<float>::max();
            float invMass = 0.0f;

            glm::mat3 rotationalInertiaTensor = glm::mat3(1);
            glm::mat3 invRotationalInertiaTensor = glm::mat3(0);

            glm::vec3 velocity = glm::vec3{0};
            glm::vec3 angularMomentum = glm::vec3{0};

            inline DynamicProperties(float mass, const glm::mat3& rotationalInertiaTensor,
                    const glm::vec3& velocity, const glm::vec3& angularVelocity) noexcept
                : mass(mass), invMass(1.0f / mass),
                  rotationalInertiaTensor(rotationalInertiaTensor), invRotationalInertiaTensor(glm::inverse(rotationalInertiaTensor)),
                  velocity(velocity), angularMomentum(angularVelocity * rotationalInertiaTensor) {}
            inline DynamicProperties() noexcept {}
        };
        inline bool isStationary() const noexcept { return !dynamicProperties.has_value(); }
        // ============================================================
        // ============== Members
        // ============================================================
        // XXX This must be the first object data to allow reinterpret_cast to work properly.
        // Possibly stores redundant data, but this way we can generalize across all shapes.
        glm::vec3 position;
        std::array<float, 3> dimensions;
        mutable std::optional<glm::mat4> modelMatrixCached;
        glm::quat orientation = glm::quat(1, 0, 0, 0); // unit = quat(w, x, y, z)
        // glm::quat orientation = glm::rotate(glm::quat(1, 0, 0, 0), 1.0f, glm::vec3(0,0,1)); // unit = quat(w, x, y, z)
        std::optional<DynamicProperties> dynamicProperties = std::nullopt;
        Type type;
        Id id;
        // ============================================================
        // ============== Object
        // ============================================================
        inline Object(Type type) : type(type), id(generateId()) {}


        const glm::mat4& modelMatrix() const noexcept;
        glm::mat4 generateModelMatrix() const noexcept;


        struct AABB {
            glm::vec3 begin;
            glm::vec3 end;
            bool intersects(const AABB& other) const noexcept;
        };
        inline AABB aabb() const noexcept {
            switch (type) {
                case Type::Box:    return asBox().aabb();
                case Type::Sphere: return asSphere().aabb();
                default: assert(false && "Unimplemented Object type."); return AABB{}; // stub
            }
        }
        // ============================================================
        // ============== Sphere
        // ============================================================
        struct Sphere {
            glm::vec3 center;
            float radius;
            float _stub1;
            float _stub2;

            AABB aabb() const noexcept;
            glm::mat3 rotationalInertiaTensor(float mass) const noexcept;
        };
        static_assert(sizeof(Sphere) == sizeof(position) + sizeof(dimensions) && "Please allocate larger dimensions[] in Object because Sphere doesn't fit.");

        static Object createSphere(const glm::vec3& center, float radius) noexcept;
        static Object createSphere(const glm::vec3& center, float radius,
            float mass, const glm::vec3& velocity, const glm::vec3& angularVelocity) noexcept;

        bool isSphere() const noexcept;
        Sphere& asSphere() noexcept;
        const Sphere& asSphere() const noexcept;
        // ============================================================
        // ============== Box
        // ============================================================
        struct Box {
            glm::vec3 center;
            glm::vec3 halfDimensions;

            struct OrientationData {
                glm::vec3 i, j, k;
            };
            OrientationData orientationDataNormalized() const noexcept;
            OrientationData orientationDataUnnormalized() const noexcept;
            std::array<glm::vec3, 8> toPoints() const noexcept;
            AABB aabb() const noexcept;
            glm::mat3 rotationalInertiaTensor(float mass) const noexcept;

            inline glm::vec3 dimensions() const noexcept { return halfDimensions * 2.0f; }
            inline       Object& asObject()       noexcept { return *reinterpret_cast<      Object*>(this); }
            inline const Object& asObject() const noexcept { return *reinterpret_cast<const Object*>(this); }
        };
        static_assert(sizeof(Box) == sizeof(position) + sizeof(dimensions) && "Please allocate larger dimensions[] in Object because Box doesn't fit.");

        static Object createBox(const glm::vec3& center, const glm::vec3& halfDimensions) noexcept;
        static Object createBox(const glm::vec3& center, const glm::vec3& halfDimensions,
            float mass, const glm::vec3& velocity, const glm::vec3& angularVelocity) noexcept;

        bool isBox() const noexcept;
        Box& asBox() noexcept;
        const Box& asBox() const noexcept;
    };
    // ============================================================
    // ============= Collisions
    // ============================================================
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

    struct VelocitiesAdjustment {
        glm::vec3 velocity        = glm::vec3(0.0f);
        glm::vec3 angularMomentum = glm::vec3(0.0f);
    };
    struct ObjectAdjustment {
        // NOTE storing idOther ensures that interactions with multiple objects for a single object are recorded
        Object::Id id             = Object::INVALID_ID;
        Object::Id idOther        = Object::INVALID_ID;
        glm::vec3 position        = glm::vec3(0.0f);
        glm::vec3 velocity        = glm::vec3(0.0f);
        glm::vec3 angularMomentum = glm::vec3(0.0f);
    };
    struct ObjectAdjustmentHasher {
        inline size_t operator()(const ObjectAdjustment& objectAdjustment) const {
            return std::hash<Object::Id>{}(objectAdjustment.id);
        }
    };
    struct ObjectAdjustmentEqual {
        inline size_t operator()(const ObjectAdjustment& objAdj1, const ObjectAdjustment& objAdj2) const {
            return objAdj1.id == objAdj2.id && objAdj1.idOther == objAdj2.idOther;
        }
    };

    using ProcessResult           = std::pair<ObjectAdjustment, ObjectAdjustment>;
    using ResolveVelocitiesResult = std::pair<VelocitiesAdjustment, VelocitiesAdjustment>;
    static ProcessResult           process(const Object& obj1, const Object& obj2) noexcept;
    static ResolveVelocitiesResult resolveVelocities(const Object& obj1, const Object& obj2, const Detection& detection) noexcept;
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
        inline static constexpr float SIZE = 9.0f;

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
    void applyAdjustments() noexcept;
    void advanceState(float dt) noexcept;
    void checkForAndHandleCollisions() noexcept;
    void reassign() noexcept;
    void removeObjectWithIdFromBucket(Object::Id id, const Bucket& bucket) noexcept;
    void printStats() const noexcept;

    Object::Id registerObject(Object&& object) noexcept;
    // Object& getObject(Object::Id id) noexcept;

    ctpl::thread_pool thread_pool;
    bool firstUpdateAfterLastRegister = true;
    std::unordered_map<Bucket, std::vector<std::shared_ptr<Object>>, BucketHasher> objectsInBuckets;
    std::vector<std::shared_ptr<Object>> objects;
    // Not to allocate every frame
    std::vector<Bucket::Bounding> allBoundingBucketsBefore;

    // struct PairHasher {
    //     inline size_t operator()(const std::pair<Object::Id, Object::Id>& pair) const {
    //         const uint64_t comb = (std::hash<uint64_t>{}(pair.first) << 32) | std::hash<uint64_t>{}(pair.second);
    //         return std::hash<uint64_t>{}(comb);
    //     }
    // };
    // std::unordered_set<std::pair<Object::Id, Object::Id>, PairHasher> processedPairsOnThisIteration;
    using Adjustments = std::unordered_set<ObjectAdjustment, ObjectAdjustmentHasher, ObjectAdjustmentEqual>;
    Adjustments adjustments;
    // ========================================================================
    // ============== Geometry helpers
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

    static float pointToLineDstSqr(const glm::vec3& point, const glm::vec3& l1, const glm::vec3& l2) noexcept;
    static std::optional<glm::vec3> linePlaneIntersection(
            const glm::vec3& linePoint, const glm::vec3& lineDirNorm,
            const glm::vec3& planePoint, const glm::vec3& planeDir) noexcept;
    static std::optional<glm::vec3> edgePlaneIntersection(
            const glm::vec3& linePoint1, const glm::vec3& linePoint2,
            const glm::vec3& planePoint, const glm::vec3& planeDir) noexcept;
    static bool pointInsideRect(const glm::vec3& P, const glm::vec3& A, const glm::vec3& B, const glm::vec3& C) noexcept;

    static void edgeFaceIntersection8(
            const float edgePointA_xs_ptr[8],
            const float edgePointA_ys_ptr[8],
            const float edgePointA_zs_ptr[8],
            const float edgePointB_xs_ptr[8],
            const float edgePointB_ys_ptr[8],
            const float edgePointB_zs_ptr[8],
            const float planePointA_xs_ptr[8],
            const float planePointA_ys_ptr[8],
            const float planePointA_zs_ptr[8],
            const float planePointB_xs_ptr[8],
            const float planePointB_ys_ptr[8],
            const float planePointB_zs_ptr[8],
            const float planePointC_xs_ptr[8],
            const float planePointC_ys_ptr[8],
            const float planePointC_zs_ptr[8],
            const float planeDir_xs_ptr[8],
            const float planeDir_ys_ptr[8],
            const float planeDir_zs_ptr[8],
            bool foundIntersection_ptr[8],
            float I_xs_ptr[8],
            float I_ys_ptr[8],
            float I_zs_ptr[8]) noexcept;
    static void edgeFaceIntersection4(
            const float edgePointA_xs_ptr[4],
            const float edgePointA_ys_ptr[4],
            const float edgePointA_zs_ptr[4],
            const float edgePointB_xs_ptr[4],
            const float edgePointB_ys_ptr[4],
            const float edgePointB_zs_ptr[4],
            const float planePointA_xs_ptr[4],
            const float planePointA_ys_ptr[4],
            const float planePointA_zs_ptr[4],
            const float planePointB_xs_ptr[4],
            const float planePointB_ys_ptr[4],
            const float planePointB_zs_ptr[4],
            const float planePointC_xs_ptr[4],
            const float planePointC_ys_ptr[4],
            const float planePointC_zs_ptr[4],
            const float planeDir_xs_ptr[4],
            const float planeDir_ys_ptr[4],
            const float planeDir_zs_ptr[4],
            bool foundIntersection_ptr[4],
            float I_xs_ptr[4],
            float I_ys_ptr[4],
            float I_zs_ptr[4]) noexcept;
};


#endif
