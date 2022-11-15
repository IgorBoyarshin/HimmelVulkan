#include "HmlPhysics.h"


std::ostream& operator<<(std::ostream& stream, const glm::vec3& v) {
    stream << "[" << v.x << ";" << v.y << ";" << v.z << "]";
    return stream;
}

namespace std {
    glm::vec3 abs(const glm::vec3& v) {
        return glm::vec3{ std::abs(v.x), std::abs(v.y), std::abs(v.z) };
    }

    // glm::vec3 copysign(const glm::vec3& v, const glm::vec3& sign) {
    //     return glm::vec3{
    //         std::copysign(v.x, sign.x),
    //         std::copysign(v.y, sign.y),
    //         std::copysign(v.z, sign.z)
    //     };
    // }

    float absmax(const glm::vec3& v) {
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

namespace glm {
    bool operator<(const glm::vec3& v1, const glm::vec3& v2) {
        return v1.x < v2.x && v1.y < v2.y && v1.z < v2.z;
    }

    bool operator>(const glm::vec3& v1, const glm::vec3& v2) {
        return v1.x > v2.x && v1.y > v2.y && v1.z > v2.z;
    }
}


template<typename Arg1, typename Arg2>
std::optional<HmlPhysics::Detection> HmlPhysics::detect(const Arg1& arg1, const Arg2& arg2) noexcept {
    static bool alreadyAsserted = false;
    if (!alreadyAsserted) {
        alreadyAsserted = true;
        std::cerr << "::> HmlPhysics::detect(): using unimplemented template specialization: "
            << typeid(Arg1).name() << " and " << typeid(Arg2).name() << "\n";
    }
    return std::nullopt;
}


// Returns dir from s1 towards s2
template<>
std::optional<HmlPhysics::Detection> HmlPhysics::detect(const Object::Sphere& s1, const Object::Sphere& s2) noexcept {
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
template<>
std::optional<HmlPhysics::Detection> HmlPhysics::detect(const Object::Box& b, const Object::Sphere& s) noexcept {
    const auto start  = b.center - b.halfDimensions - s.radius;
    const auto finish = b.center + b.halfDimensions + s.radius;
    if (start < s.center && s.center < finish) {
        // NOTE we care only about the predominant direction, and in terms of math the
        // radius component has no effect because it is the same in all directions, so
        // we omit it to simplify calculations.
        const auto centers = s.center - b.center;
        const auto n = centers / b.halfDimensions;
        const float max = std::absmax(n);
        const auto dir = glm::vec3{
            std::copysign(max == n.x, n.x),
            std::copysign(max == n.y, n.y),
            std::copysign(max == n.z, n.z),
        };
        // NOTE second abs is actually redundant because "dir" has the same sign as "centers"
        const float extent = std::abs(glm::dot(b.halfDimensions, dir)) + s.radius - std::abs(glm::dot(centers, dir));
        return { Detection{
            .dir = dir,
            .extent = extent,
        }};
    }

    return std::nullopt;
}


// Returns dir from s towards b
template<>
std::optional<HmlPhysics::Detection> HmlPhysics::detect(const Object::Sphere& s, const Object::Box& b) noexcept {
    auto resultOpt = detect(b, s);
    if (!resultOpt) return resultOpt;
    resultOpt->dir *= -1;
    return resultOpt;
}


// Returns dir from b1 towards b2
template<>
std::optional<HmlPhysics::Detection> HmlPhysics::detect(const Object::Box& b1, const Object::Box& b2) noexcept {
    const auto start  = b1.center - b1.halfDimensions - b2.halfDimensions;
    const auto finish = b1.center + b1.halfDimensions + b2.halfDimensions;
    if (start < b2.center && b2.center < finish) {
        const auto centers = b2.center - b1.center;
        const auto n = centers / (b1.halfDimensions + b2.halfDimensions);
        const float max = std::absmax(n);
        const auto dir = glm::vec3{
            std::copysign(max == n.x, n.x),
            std::copysign(max == n.y, n.y),
            std::copysign(max == n.z, n.z),
        };
        // NOTE second abs is actually redundant because dir has the same sign as centers
        const float extent = std::abs(glm::dot(b1.halfDimensions + b2.halfDimensions, dir)) - std::abs(glm::dot(centers, dir));
        return { Detection{
            .dir = dir,
            .extent = extent,
        }};
    }

    return std::nullopt;
}


void HmlPhysics::resolveVelocities(Object& obj1, Object& obj2, const Detection& detection) noexcept {
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


void HmlPhysics::process(Object& obj1, Object& obj2) noexcept {
    std::optional<Detection> detectionOpt = std::nullopt;
    if      (obj1.isSphere() && obj2.isSphere()) detectionOpt = detect(obj1.asSphere(), obj2.asSphere());
    else if (obj1.isBox()    && obj2.isSphere()) detectionOpt = detect(obj1.asBox(),    obj2.asSphere());
    else if (obj1.isSphere() && obj2.isBox())    detectionOpt = detect(obj1.asSphere(), obj2.asBox());
    else if (obj1.isBox()    && obj2.isBox())    detectionOpt = detect(obj1.asBox(),    obj2.asBox());

    if (!detectionOpt) return;
    const auto& [dir, extent] = *detectionOpt;

    if (!obj1.isStationary()) obj1.position -= dir * extent * (obj2.isStationary() ? 1.0f : 0.5f);
    if (!obj2.isStationary()) obj2.position += dir * extent * (obj1.isStationary() ? 1.0f : 0.5f);

    resolveVelocities(obj1, obj2, *detectionOpt);
}


bool HmlPhysics::reassign(std::shared_ptr<Object> object, const Bucket& bucket, const Bucket::Bounding& boundingBucketsBefore, const Bucket::Bounding& boundingBucketsAfter) noexcept {
    bool removedFromCurrentBucket = false;

    const auto superset = boundingBucketsSum(boundingBucketsBefore, boundingBucketsAfter);
    // std::shared_ptr<Object> objCopy = object;
    for (Bucket::Coord x = superset.first.x; x <= superset.second.x; x++) {
        for (Bucket::Coord y = superset.first.y; y <= superset.second.y; y++) {
            for (Bucket::Coord z = superset.first.z; z <= superset.second.z; z++) {
                const Bucket b{ .x = x, .y = y, .z = z };
                const bool before = b.isInsideBoundingBuckets(boundingBucketsBefore);
                const bool after = b.isInsideBoundingBuckets(boundingBucketsAfter);
        // std::cout << "Check " << x << " " << y << " " << z << "--" << std::endl;
                if (before && !after) {
                    const auto bef = objectsInBuckets[b].size();
                    removeObjectWithIdFromBucket(object->id, b);
                    const auto af = objectsInBuckets[b].size();
                    // assert(af + 1 == bef);
                    // assert(object.use_count() > 1);
                    // NOTE don't i++ if the current bucket got changed (removed from)
                    // because the newly-current object is taken from the end and thus
                    // has not been visited yet.
                    if (b == bucket) removedFromCurrentBucket = true;
                    // if (b == bucket) std::cout << "SELF" << std::endl;
                    // std::cout << "yes1" << '\n';
                } else if (!before && after) {
                    objectsInBuckets[b].push_back(object);
                    // std::cout << "yes2" << '\n';
                }
                // std::cout << std::endl;
            }
        }
    }
    // std::cout << "done " << '\n';

    return removedFromCurrentBucket;
}


void HmlPhysics::updateForDt(float dt) noexcept {
    // std::cout << "CALL-------------------" << '\n';
    const uint8_t SUBSTEPS = 1;
    const float subDt = dt / SUBSTEPS;
    for (uint8_t substep = 0; substep < SUBSTEPS; substep++) {
        // Apply velocity onto position change and possibly move Objects between Buckets
        for (const auto& [bucket, objects] : objectsInBuckets) {
            for (int i = 0; i < objects.size(); i++) {
                const auto object = objects[i];
                if (object->isStationary()) continue;

                if (object->visitedByStep == Object::UpdateStep::First) continue;
                object->visitedByStep = Object::UpdateStep::First;

                const auto boundingBucketsBefore = boundingBucketsForObject(*object);
                object->position += object->dynamicProperties->velocity * subDt;
                object->position += object->dynamicProperties->velocity * subDt;
                const auto boundingBucketsAfter = boundingBucketsForObject(*object);

                if (boundingBucketsBefore == boundingBucketsAfter) continue;

                // std::cout << "For index " << i << '\n';
                // std::cout << object->id << " Moving from ";
                // {
                //     const auto& [x,y,z] = boundingBucketsBefore.first;
                //     std::cout << x << " " << y << " " << z << ";;";
                // }
                // {
                //     const auto& [x,y,z] = boundingBucketsBefore.second;
                //     std::cout << x << " " << y << " " << z << ";;";
                // }
                // std::cout << " to ";
                // {
                //     const auto& [x,y,z] = boundingBucketsAfter.first;
                //     std::cout << x << " " << y << " " << z << ";;";
                // }
                // {
                //     const auto& [x,y,z] = boundingBucketsAfter.second;
                //     std::cout << x << " " << y << " " << z << ";;";
                // }
                // std::cout << '\n';

                // Remove the Object from old Buckets and add to new Buckets
                reassign(object, bucket, boundingBucketsBefore, boundingBucketsAfter);
            }
        }

        // Check for and handle collisions
        // NOTE Ignore miniscule corrections here with regards to Objects movement between Buckets:
        // they will get sorted out during the next iteration in the previous section.
        for (const auto& [bucket, objects] : objectsInBuckets) {
            for (int i = 0; i < objects.size(); i++) {
                auto& obj1 = objects[i];
                // if (obj1->id == 6) std::cout << obj1->position << '\n';
                obj1->visitedByStep = Object::UpdateStep::None;

                // For each object, check test it against all other objects in current bucket...
                for (size_t j = i + 1; j < objects.size(); j++) {
                    auto& obj2 = objects[j];
                    if (obj1->isStationary() && obj2->isStationary()) continue;
                const auto boundingBucketsBefore1 = boundingBucketsForObject(*obj1);
                const auto boundingBucketsBefore2 = boundingBucketsForObject(*obj2);
                    process(*obj1, *obj2);
                const auto boundingBucketsAfter1 = boundingBucketsForObject(*obj1);
                const auto boundingBucketsAfter2 = boundingBucketsForObject(*obj2);
                if (boundingBucketsBefore2 != boundingBucketsAfter2) {
                    if (reassign(obj2, bucket, boundingBucketsBefore2, boundingBucketsAfter2)) j--;
                }
                if (boundingBucketsBefore1 != boundingBucketsAfter1) {
                    if (reassign(obj1, bucket, boundingBucketsBefore1, boundingBucketsAfter1)) {
                        i--;
                        break; // from inner loop
                    }
                }
                }
            }
        }
    }
}
// ============================================================================
HmlPhysics::Object::Id HmlPhysics::registerObject(Object&& object) noexcept {
    const auto id = object.id;
    std::cout << "Register " << id << " as " << '\n';
    const auto bounds = boundingBucketsForObject(object);
    const auto shared = std::make_shared<Object>(std::move(object));
    for (Bucket::Coord x = bounds.first.x; x <= bounds.second.x; x++) {
        for (Bucket::Coord y = bounds.first.y; y <= bounds.second.y; y++) {
            for (Bucket::Coord z = bounds.first.z; z <= bounds.second.z; z++) {
                std::cout << x << " " << y << " " << z <<'\n';
                const Bucket bucket{ .x = x, .y = y, .z = z };
                std::cout << std::bitset<64>(BucketHasher{}(bucket)) <<'\n';
            // const HmlPhysics::Bucket::Hash comb =
            //     (static_cast<uint64_t>(static_cast<uint16_t>(bucket.x)) << 32) |
            //     (static_cast<uint64_t>(static_cast<uint16_t>(bucket.y)) << 16) |
            //      static_cast<uint64_t>(static_cast<uint16_t>(bucket.z));
                // std::cout << std::bitset<64>(comb) <<'\n';
                objectsInBuckets[bucket].push_back(shared);
            }
        }
    }

    return id;
}


void HmlPhysics::removeObjectWithIdFromBucket(Object::Id id, const Bucket& bucket) noexcept {
    // std::cout << "For " << id << std::endl;
    auto& objects = objectsInBuckets[bucket];
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        // std::cout << (*it)->id << std::endl;
        if ((*it)->id == id) {
            objects.erase(it);
            // std::swap((*it), (*objects.end()));
            // objects.pop_back();
            return;
        }
    }
    // std::cout << "No " << id << std::endl;
    assert(false && "::> There was no such Object in the Bucket");
}


// HmlPhysics::Object& HmlPhysics::getObject(HmlPhysics::Object::Id id) noexcept {
//     assert(false && "Should you really use this function?");
//     for (const auto& [_bucket, objects] : objectsInBuckets) {
//         for (auto& object : objects) {
//             if (object.id == id) return object;
//         }
//     }
//
//     assert(false && "::> No Object with provided Id found.");
//     return objects[Bucket()][0]; // stub
// }
// ============================================================================
// ============================================================================
// ============================================================================
std::optional<HmlPhysics::LineIntersectsTriangleResult> HmlPhysics::lineIntersectsTriangle(
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
// ============================================================================
float HmlPhysics::pointToLineDstSqr(const glm::vec3& point, const glm::vec3& l1, const glm::vec3& l2) noexcept {
    const auto nom = glm::cross(l1 - l2, l2 - point);
    const float nomSqr = glm::dot(nom, nom);
    const auto denom = (l1 - l2);
    const float denomSqr = glm::dot(denom, denom);
    return nomSqr / denomSqr;
}
