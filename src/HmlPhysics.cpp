#include "HmlPhysics.h"


std::ostream& operator<<(std::ostream& stream, const glm::vec3& v) {
    stream << "[" << v.x << ";" << v.y << ";" << v.z << "]";
    return stream;
}

namespace std {
    glm::vec3 abs(const glm::vec3& v) {
        return glm::vec3{ std::abs(v.x), std::abs(v.y), std::abs(v.z) };
    }

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

    float min(const glm::vec3& v) {
        float min = v.x;
        bool foundNew;

        foundNew = v.y < min;
        min = min * (1 - foundNew) + v.y * foundNew;
        foundNew = v.z < min;
        min = min * (1 - foundNew) + v.z * foundNew;

        return min;
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
        const auto centers = s.center - b.center;
        const auto extent = b.halfDimensions + s.radius - std::abs(centers); // all non-negative
        // NOTE disable y-axis collision resolution for now
        const float minExtent = std::min(extent.x, extent.z); 
        const auto dir = glm::vec3{
            std::copysign(minExtent == extent.x, centers.x),
            0.0f,
            std::copysign(minExtent == extent.z, centers.z),
        };

        return { Detection{
            .dir = dir,
            .extent = minExtent,
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
        const auto extent = b1.halfDimensions + b2.halfDimensions - std::abs(centers); // all non-negative
        // NOTE disable y-axis collision resolution for now
        const float minExtent = std::min(extent.x, extent.z); 
        const auto dir = glm::vec3{
            std::copysign(minExtent == extent.x, centers.x),
            0.0f,
            std::copysign(minExtent == extent.z, centers.z),
        };

        return { Detection{
            .dir = dir,
            .extent = minExtent,
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


void HmlPhysics::updateForDt(float dt) noexcept {
    if (firstUpdateAfterLastRegister) {
        firstUpdateAfterLastRegister = false;

        allBoundingBucketsBefore.clear();
        for (auto& object : objects) {
            if (object->isStationary()) continue;
            allBoundingBucketsBefore.push_back(boundingBucketsForObject(*object));
        }
    }

    const uint8_t SUBSTEPS = 1;
    const float subDt = dt / SUBSTEPS;
    static int stepTimer = 0;
    for (uint8_t substep = 0; substep < SUBSTEPS; substep++) {
        // Apply velocity onto position change
        const auto mark1 = std::chrono::high_resolution_clock::now();
        for (auto& object : objects) {
            if (object->isStationary()) continue;
            object->position += object->dynamicProperties->velocity * subDt;
        }
        const auto mark2 = std::chrono::high_resolution_clock::now();

        // Check for and handle collisions
        for (const auto& [bucket, objects] : objectsInBuckets) {
            if (objects.empty()) {
                objectsInBuckets.erase(bucket);
                continue;
            }

            for (size_t i = 0; i < objects.size(); i++) {
                auto& obj1 = objects[i];

                // For each object, test it against all other objects in current bucket...
                for (size_t j = i + 1; j < objects.size(); j++) {
                    auto& obj2 = objects[j];
                    if (obj1->isStationary() && obj2->isStationary()) continue;
                    process(*obj1, *obj2);
                }
            }
        }
        const auto mark3 = std::chrono::high_resolution_clock::now();

        // Put objects into proper buckets after they have finished being moved
        reassign();
        const auto mark4 = std::chrono::high_resolution_clock::now();

        stepTimer++;
        if (false && stepTimer == 100) {
            stepTimer = 0;
            const auto step1Mks = std::chrono::duration_cast<std::chrono::microseconds>(mark2 - mark1).count();
            const auto step2Mks = std::chrono::duration_cast<std::chrono::microseconds>(mark3 - mark2).count();
            const auto step3Mks = std::chrono::duration_cast<std::chrono::microseconds>(mark4 - mark3).count();
            std::cout << "Step 1 = " << static_cast<float>(step1Mks) << " mks\n";
            std::cout << "Step 2 = " << static_cast<float>(step2Mks) << " mks\n";
            std::cout << "Step 3 = " << static_cast<float>(step3Mks) << " mks\n";
        }
    }
}
// ============================================================================
HmlPhysics::Object::Id HmlPhysics::registerObject(Object&& object) noexcept {
    firstUpdateAfterLastRegister = true;
    const auto id = object.id;
    const auto bounds = boundingBucketsForObject(object);
    const auto shared = std::make_shared<Object>(std::move(object));
    objects.push_back(shared);
    for (Bucket::Coord x = bounds.first.x; x <= bounds.second.x; x++) {
        for (Bucket::Coord y = bounds.first.y; y <= bounds.second.y; y++) {
            for (Bucket::Coord z = bounds.first.z; z <= bounds.second.z; z++) {
                const Bucket bucket{ .x = x, .y = y, .z = z };
                objectsInBuckets[bucket].push_back(shared);
            }
        }
    }

    return id;
}


void HmlPhysics::removeObjectWithIdFromBucket(Object::Id id, const Bucket& bucket) noexcept {
    auto& objects = objectsInBuckets[bucket];
    const auto size = objects.size();
    for (size_t i = 0; i < size; i++) {
        const auto& object = objects[i];
        if (object->id == id) {
            // NOTE we don't check if this is the same object to remove the unnesessary if 
            // NOTE swap, but with fewer steps because we'll remove the current object anyway
            objects[i] = objects[size - 1];
            objects.pop_back();
            return;
        }
    }
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
void HmlPhysics::printStats() const noexcept {
    std::map<uint32_t, uint32_t> countOfBucketsWithSize;
    for (const auto& [bucket, objects] : objectsInBuckets) {
        const auto size = objects.size();
        countOfBucketsWithSize[size]++;
    }

    uint32_t min = objects.size();
    uint32_t max = 0;
    uint32_t avg = 0;
    for (const auto& [size, count] : countOfBucketsWithSize) {
        std::cout << "With size [" << size << "] there are " << count << " buckets\n";
        if (size < min) min = size;
        if (size > max) max = size;
        avg += size * count;
    }

    std::cout << "Min size=" << min << "; Max size=" << max << "; Avg in bucket=" << (avg / countOfBucketsWithSize.size()) << "\n";
}


// NOTE arg does not store BB for stationary objects
void HmlPhysics::reassign() noexcept {
    auto boundingBoundsBeforeIt = allBoundingBucketsBefore.begin();
    for (const auto& object : objects) {
        if (object->isStationary()) continue;

        const auto boundingBucketsBefore = *boundingBoundsBeforeIt;
        const auto boundingBucketsAfter = boundingBucketsForObject(*object);
        *boundingBoundsBeforeIt = boundingBucketsAfter; // for use during next iteration
        ++boundingBoundsBeforeIt;
        if (boundingBucketsBefore == boundingBucketsAfter) continue;

        const auto superset = boundingBucketsSum(boundingBucketsBefore, boundingBucketsAfter);
        for (Bucket::Coord x = superset.first.x; x <= superset.second.x; x++) {
            for (Bucket::Coord y = superset.first.y; y <= superset.second.y; y++) {
                for (Bucket::Coord z = superset.first.z; z <= superset.second.z; z++) {
                    const Bucket b{ .x = x, .y = y, .z = z };
                    const bool before = b.isInsideBoundingBuckets(boundingBucketsBefore);
                    const bool after  = b.isInsideBoundingBuckets(boundingBucketsAfter);
                    if (before && !after) { // remove from this bucket
                        removeObjectWithIdFromBucket(object->id, b);
                    } else if (!before && after) { // add to this bucket
                        objectsInBuckets[b].push_back(object);
                    }
                }
            }
        }
    }
}


inline HmlPhysics::Bucket::Bounding HmlPhysics::boundingBucketsSum(const Bucket::Bounding& bb1, const Bucket::Bounding& bb2) noexcept {
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
        }
    );
}

inline HmlPhysics::Bucket::Bounding HmlPhysics::boundingBucketsForObject(const Object& object) noexcept {
    const auto aabb = object.aabb();
    const auto begin = Bucket::fromPos(aabb.begin);
    const auto end = Bucket::fromPos(aabb.end);
    return std::make_pair(begin, end);
}
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
