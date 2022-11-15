#ifndef HML_PHYSICS
#define HML_PHYSICS

#include <vector>
#include <array>
#include <cmath>
#include <optional>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <memory>
#include <bitset>

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

        using Id = uint32_t;
        inline static constexpr Id INVALID_ID = 0;
        Id id;
        inline static Id generateId() noexcept {
            static Id nextIdToUse = 1;
            return nextIdToUse++;
        }

        enum UpdateStep {
            None, First, Second, Third, Fourth, Last // TODO rename
        };
        UpdateStep visitedByStep = UpdateStep::None;

        inline bool isSphere() const noexcept { return type == Type::Sphere; }
        inline bool isBox()    const noexcept { return type == Type::Box; }


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
            object.id = Object::generateId();
            return object;
        }
        inline static Object createBox(const glm::vec3& center, const glm::vec3& halfDimensions) noexcept {
            Object object;
            object.type = Type::Box;
            object.position = { center.x, center.y, center.z };
            object.data = { halfDimensions.x, halfDimensions.y, halfDimensions.z };
            object.id = Object::generateId();
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
    Object::Id registerObject(Object&& object) noexcept;
    // Object& getObject(Object::Id id) noexcept;

    // std::vector<Object> objects;
    // std::vector<Id> ids;
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

        inline Bucket nextX() const noexcept { return Bucket{.x = static_cast<Coord>(x + 1), .y = y, .z = z }; }
        inline Bucket nextY() const noexcept { return Bucket{.x = x, .y = static_cast<Coord>(y + 1), .z = z }; }
        inline Bucket nextZ() const noexcept { return Bucket{.x = x, .y = y, .z = static_cast<Coord>(z + 1) }; }

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

        // inline Hash hash() const noexcept {
        //     return x + "." + y + "." + z;
        // }

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
    inline static Bucket::Bounding boundingBucketsSum(const Bucket::Bounding& bb1, const Bucket::Bounding& bb2) noexcept {
        return std::make_pair(
            Bucket{
                .x = std::min(bb1.first.x, bb2.first.x),
                .y = std::min(bb1.first.y, bb2.first.y),
                .z = std::min(bb1.first.z, bb2.first.z),
            },
            Bucket{
                .x = std::max(bb1.second.x, bb2.second.x),
                .y = std::max(bb1.second.y, bb2.second.y),
                .z = std::max(bb1.second.z, bb2.second.z),
            });
    }

    inline static Bucket::Bounding boundingBucketsForObject(const Object& object) noexcept {
        const auto aabb = object.aabb();
        const auto begin = Bucket::fromPos(aabb.begin);
        const auto end = Bucket::fromPos(aabb.end);
        return std::make_pair(begin, end);
    }


bool reassign(std::shared_ptr<Object> object, const Bucket& bucket, const Bucket::Bounding& boundingBucketsBefore, const Bucket::Bounding& boundingBucketsAfter) noexcept;

    // inline std::vector<Bucket> bucketsForObject(const Object& object) noexcept {
    //     const auto aabb = object.aabb();
    //     const auto bucketStart = Bucket::fromPos(aabb.start);
    //     const auto bucketEnd = Bucket::fromPos(aabb.end);
    //     auto bucket = bucketStart;
    //
    //     for (Bucket::Coord x = bucketStart.x; x <= bucketEnd.x; x++) {
    //         for (Bucket::Coord y = bucketStart.y; y <= bucketEnd.y; y++) {
    //             for (Bucket::Coord z = bucketStart.z; z <= bucketEnd.z; z++) {
    //                 buckets.push_back(bucket);
    //             }
    //         }
    //     }
    //
    //     std::vector<Bucket> buckets;
    //     return buckets;
    // }

    void removeObjectWithIdFromBucket(Object::Id id, const Bucket& bucket) noexcept;

    // inline static std::vector<Bucket> nearBucketsForPos() noexcept {
    //     std::vector<Bucket> buckets;
    //     bucket.reserve(4);
    //     return buckets;
    // }

    std::unordered_map<Bucket, std::vector<std::shared_ptr<Object>>, BucketHasher> objectsInBuckets;
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
