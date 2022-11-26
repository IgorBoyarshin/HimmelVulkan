#include "HmlPhysics.h"
// ============================================================================
// ========================== Abstract detectors ==============================
// ============================================================================
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
    const auto middlePoint = s1.center + normC * (s1.radius + extent * 0.5f);
    return { Detection {
        .dir = normC,
        .extent = extent,
        .contactPoints = { middlePoint },
    }};
}


// Returns dir from b towards s
template<>
std::optional<HmlPhysics::Detection> HmlPhysics::detect(const Object::Box& b, const Object::Sphere& s) noexcept {
    // return detectAxisAlignedBoxSphere(b, s);
    return detectOrientedBoxSphere(b, s);
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
    // return detectAxisAlignedBoxes(b1, b2);
    return detectOrientedBoxesWithSat(b1, b2);
    // return gjk(b1, b2);
}
// ============================================================================
// ========================== Concrete detectors ==============================
// ============================================================================
// Returns dir from b towards s
std::optional<HmlPhysics::Detection> HmlPhysics::detectOrientedBoxSphere(const Object::Box& b, const Object::Sphere& s) noexcept {
    const auto& [iNorm, jNorm, kNorm] = b.orientationDataNormalized();
    const auto v = s.center - b.center;
    const glm::vec3 proj{ glm::dot(v, iNorm), glm::dot(v, jNorm), glm::dot(v, kNorm) };
    const auto dim = b.halfDimensions + glm::vec3{s.radius};
    if (glm::vec3{-dim} < proj && proj < glm::vec3{dim}) {
        const auto projAbs = std::abs(proj);
        const auto extent = dim - projAbs;
        const float minExtent = std::min(extent);
        const auto dir =
            std::copysign(static_cast<float>(minExtent == extent.x), proj.x) * iNorm +
            std::copysign(static_cast<float>(minExtent == extent.y), proj.y) * jNorm +
            std::copysign(static_cast<float>(minExtent == extent.z), proj.z) * kNorm;
        const auto contactPoint = s.center - dir * (s.radius + minExtent * 0.5f);

        return { Detection{
            .dir = dir,
            .extent = minExtent,
            .contactPoints = { contactPoint },
        }};
    }

    return std::nullopt;
}


// Returns dir from b1 towards b2
std::optional<HmlPhysics::Detection> HmlPhysics::detectOrientedBoxesWithSat(const Object::Box& b1, const Object::Box& b2) noexcept {
    // const auto mark1 = std::chrono::high_resolution_clock::now();

    const auto orientationData1 = b1.orientationDataNormalized();
    const auto orientationData2 = b2.orientationDataNormalized();
    const auto& [i1, j1, k1] = orientationData1;
    const auto& [i2, j2, k2] = orientationData2;
    const auto p1 = b1.toPoints();
    const auto p2 = b2.toPoints();

    static const auto minMaxProjAt = [](const glm::vec3& axis, std::span<const glm::vec3> ps){
        float min = std::numeric_limits<float>::max();
        float max = std::numeric_limits<float>::lowest();
        for (const auto& p : ps) {
            const auto proj = glm::dot(p, axis);
            const bool newMin = proj < min;
            const bool newMax = proj > max;
            min = min * (1 - newMin) + proj * newMin;
            max = max * (1 - newMax) + proj * newMax;
        }
        return std::make_pair(min, max);
    };

    glm::vec3 dir;
    float minExtent = std::numeric_limits<float>::max();
    for (const auto& axis : { i1, j1, k1, i2, j2, k2 }) {
        const auto& [min1, max1] = minMaxProjAt(axis, p1);
        const auto& [min2, max2] = minMaxProjAt(axis, p2);

        if (!(min1 < max2 && min2 < max1)) return std::nullopt;

        const auto extent = std::min(max1 - min2, max2 - min1);
        if (extent < minExtent) {
            minExtent = extent;
            dir = axis;
        }
    }

    const auto centers = b2.center - b1.center;
    const bool aligned = glm::dot(centers, dir) >= 0.0f;
    if (!aligned) dir *= -1;

    // const auto mark2 = std::chrono::high_resolution_clock::now();

    // Find contact points

    static const auto addContactPointsFromOnto = [](
            std::span<const glm::vec3> psFrom,
            std::span<const glm::vec3> psOnto,
            const Object::Box::OrientationData& orientationDataOnto,
            std::vector<glm::vec3>& contactPoints){
        static const std::array<std::pair<size_t, size_t>, 12> edges{
            std::make_pair(0, 1), std::make_pair(2, 3), std::make_pair(0, 2), std::make_pair(1, 3),
            std::make_pair(4, 5), std::make_pair(6, 7), std::make_pair(4, 6), std::make_pair(5, 7),
            std::make_pair(0, 4), std::make_pair(1, 5), std::make_pair(2, 6), std::make_pair(3, 7)
        };
        static const std::array<std::array<size_t, 4>, 6> faces{
            std::array<size_t, 4>{0,1,2,3}, std::array<size_t, 4>{4,5,6,7},
            std::array<size_t, 4>{0,1,4,5}, std::array<size_t, 4>{2,3,6,7},
            std::array<size_t, 4>{0,2,4,6}, std::array<size_t, 4>{1,3,5,7}
        };
        static const auto faceNormalForIndex = [](size_t i, const Object::Box::OrientationData& orientationData){
            switch (i / 2) {
                case 0: return orientationData.i;
                case 1: return orientationData.j;
                case 2: return orientationData.k;
            }
            assert(false && "Unexpected index in faceNormalForIndex()"); return glm::vec3{}; // stub
        };
        for (const auto& edge : edges) {
            const auto& linePoint1 = psFrom[edge.first];
            const auto& linePoint2 = psFrom[edge.second];
            for (size_t i = 0; i < faces.size(); i++) {
                const auto& face = faces[i];
                const auto& planePoint = psOnto[face[0]]; // any point on plane
                const auto planeDir = faceNormalForIndex(i, orientationDataOnto);
                const auto intOpt = edgePlaneIntersection(linePoint1, linePoint2, planePoint, planeDir);
                if (!intOpt) continue;
                if (pointInsideRect(*intOpt, psOnto[face[0]], psOnto[face[1]], psOnto[face[2]])) {
                    contactPoints.push_back(*intOpt);
                }
            }
        }
    };

    static const auto addContactPointsFromOntoAvx = [](
            std::span<const glm::vec3> psFrom,
            std::span<const glm::vec3> psOnto,
            const Object::Box::OrientationData& orientationDataOnto,
            std::vector<glm::vec3>& contactPoints){

        // for (size_t f = 0; f < 6; f++) {
        //     const auto& face = faces[f];
        //     const glm::vec3& planePointA = psOnto[face[0]];
        //     const glm::vec3& planePointB = psOnto[face[1]];
        //     const glm::vec3& planePointC = psOnto[face[2]];
        //     const glm::vec3& planeDir = faceNormalForIndex(i, orientationDataOnto); // TODO
        //     for (size_t e = 0; e < 12; e++) {
        //         const auto& edge = edges[e];
        //
        //         const glm::vec3& edgePointA = psFrom[edge.first];
        //         const glm::vec3& edgePointB = psFrom[edge.second];
        //
        //         // XXX
        //     }
        // }




        static const std::array<std::pair<size_t, size_t>, 12> edges{
            std::make_pair(0, 1), std::make_pair(2, 3), std::make_pair(0, 2), std::make_pair(1, 3),
            std::make_pair(4, 5), std::make_pair(6, 7), std::make_pair(4, 6), std::make_pair(5, 7),
            std::make_pair(0, 4), std::make_pair(1, 5), std::make_pair(2, 6), std::make_pair(3, 7)
        };
        static const std::array<std::array<size_t, 4>, 6> faces{
            std::array<size_t, 4>{0,1,2,3}, std::array<size_t, 4>{4,5,6,7},
            std::array<size_t, 4>{0,1,4,5}, std::array<size_t, 4>{2,3,6,7},
            std::array<size_t, 4>{0,2,4,6}, std::array<size_t, 4>{1,3,5,7}
        };
        static const auto faceNormalForIndex = [](size_t i, const Object::Box::OrientationData& orientationData){
            switch (i / 2) {
                case 0: return orientationData.i;
                case 1: return orientationData.j;
                case 2: return orientationData.k;
            }
            assert(false && "Unexpected index in faceNormalForIndex()"); return glm::vec3{}; // stub
        };
        for (const auto& edge : edges) {
            const auto& linePoint1 = psFrom[edge.first];
            const auto& linePoint2 = psFrom[edge.second];
            for (size_t i = 0; i < faces.size(); i++) {
                const auto& face = faces[i];
                const auto& planePoint = psOnto[face[0]]; // any point on plane
                const auto planeDir = faceNormalForIndex(i, orientationDataOnto);
                const auto intOpt = edgePlaneIntersection(linePoint1, linePoint2, planePoint, planeDir);
                if (!intOpt) continue;
                if (pointInsideRect(*intOpt, psOnto[face[0]], psOnto[face[1]], psOnto[face[2]])) {
                    contactPoints.push_back(*intOpt);
                }
            }
        }
    };

    std::vector<glm::vec3> contactPoints;
    addContactPointsFromOnto(p1, p2, orientationData2, contactPoints);
    addContactPointsFromOnto(p2, p1, orientationData1, contactPoints);

    // const auto mark3 = std::chrono::high_resolution_clock::now();
    // const auto step1Mks = std::chrono::duration_cast<std::chrono::microseconds>(mark2 - mark1).count();
    // const auto step2Mks = std::chrono::duration_cast<std::chrono::microseconds>(mark3 - mark2).count();
    // std::cout << "SAT=" << static_cast<float>(step1Mks)
    //     << "mks. Contact points=" << static_cast<float>(step2Mks)
    //     << "mks\n";

    return { Detection{
        .dir = dir,
        .extent = minExtent,
        .contactPoints = contactPoints,
    }};
}


// Returns dir from b towards s
std::optional<HmlPhysics::Detection> HmlPhysics::detectAxisAlignedBoxSphere(const Object::Box& b, const Object::Sphere& s) noexcept {
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
        const auto contactPoint = s.center - dir * (s.radius + minExtent * 0.5f);

        return { Detection{
            .dir = dir,
            .extent = minExtent,
            .contactPoints = { contactPoint },
        }};
    }

    return std::nullopt;
}


// Returns dir from b1 towards b2
std::optional<HmlPhysics::Detection> HmlPhysics::detectAxisAlignedBoxes(const Object::Box& b1, const Object::Box& b2) noexcept {
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
            .contactPoints = {},
        }};
    }

    return std::nullopt;
}
// ============================================================================
// ===================== Resolver =============================================
// ============================================================================
void HmlPhysics::resolveVelocities(Object& obj1, Object& obj2, const Detection& detection) noexcept {
    const auto& [dir, extent, contactPoints] = detection;

    static const auto avg = [](std::span<const glm::vec3> vs){
        glm::vec3 sum{0};
        for (const auto& v : vs) sum += v;
        return sum / static_cast<float>(vs.size());
    };

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
    const float nom = -(1.0f + e) * glm::dot(relativeV, dir);
    float denom = (obj1DP.invMass + obj2DP.invMass);
    const auto rap = avg(contactPoints) - obj1.position;
    const auto rbp = avg(contactPoints) - obj2.position;
    const auto a = glm::cross(obj1DP.invRotationalInertiaTensor * glm::cross(rap, dir), dir);
    const auto b = glm::cross(obj2DP.invRotationalInertiaTensor * glm::cross(rbp, dir), dir);
    denom += glm::dot(a + b, dir);
    const float j = nom / denom;
    const auto impulse = j * dir;
    if (!obj1.isStationary()) {
        obj1.dynamicProperties->velocity -= impulse * obj1DP.invMass;
        obj1.dynamicProperties->angularMomentum -= glm::cross(rap, impulse) * obj1DP.invRotationalInertiaTensor;
    }
    if (!obj2.isStationary()) {
        obj2.dynamicProperties->velocity += impulse * obj2DP.invMass;
        obj2.dynamicProperties->angularMomentum += glm::cross(rbp, impulse) * obj2DP.invRotationalInertiaTensor;
    }
}


void HmlPhysics::process(Object& obj1, Object& obj2) noexcept {
    // const auto mark1 = std::chrono::high_resolution_clock::now();
    std::optional<Detection> detectionOpt = std::nullopt;
    if      (obj1.isSphere() && obj2.isSphere()) detectionOpt = detect(obj1.asSphere(), obj2.asSphere());
    else if (obj1.isBox()    && obj2.isSphere()) detectionOpt = detect(obj1.asBox(),    obj2.asSphere());
    else if (obj1.isSphere() && obj2.isBox())    detectionOpt = detect(obj1.asSphere(), obj2.asBox());
    else if (obj1.isBox()    && obj2.isBox())    detectionOpt = detect(obj1.asBox(),    obj2.asBox());
    // const auto mark2 = std::chrono::high_resolution_clock::now();

    if (!detectionOpt) return;
    const auto& [dir, extent, _contactPoints] = *detectionOpt;

    if (!obj1.isStationary()) obj1.position -= dir * extent * (obj2.isStationary() ? 1.0f : 0.5f);
    if (!obj2.isStationary()) obj2.position += dir * extent * (obj1.isStationary() ? 1.0f : 0.5f);

    resolveVelocities(obj1, obj2, *detectionOpt);
    // const auto mark3 = std::chrono::high_resolution_clock::now();

    // const auto step1Mks = std::chrono::duration_cast<std::chrono::microseconds>(mark2 - mark1).count();
    // const auto step2Mks = std::chrono::duration_cast<std::chrono::microseconds>(mark3 - mark2).count();
    // std::cout << "Detection=" << static_cast<float>(step1Mks)
    //     << "mks. Resolve=" << static_cast<float>(step2Mks)
    //     << "mks. (" << (obj1.isBox() ? "Box":"Sphere") << "--" << (obj2.isBox() ? "Box":"Sphere") << ")\n";
}
// ============================================================================
// ===================== Main Update ==========================================
// ============================================================================
void HmlPhysics::updateForDt(float dt) noexcept {
    if (firstUpdateAfterLastRegister) {
        firstUpdateAfterLastRegister = false;

        allBoundingBucketsBefore.clear();
        for (auto& object : objects) {
            if (object->isStationary()) continue;
            allBoundingBucketsBefore.push_back(boundingBucketsForObject(*object));
        }
    }

    const glm::vec3 F {0,0,0};
    // const glm::vec3 cp{1,0,0};
    // const glm::vec3 Ft{0,0,0};

    const uint8_t SUBSTEPS = 1;
    const float subDt = dt / SUBSTEPS;
    for (uint8_t substep = 0; substep < SUBSTEPS; substep++) {
        // Apply velocity onto position change
        const auto mark1 = std::chrono::high_resolution_clock::now();
        for (auto& object : objects) {
            if (object->isStationary()) continue;

            object->position += subDt * object->dynamicProperties->velocity;
            object->dynamicProperties->velocity += subDt * F * object->dynamicProperties->invMass;

            // World-space inverse inertia tensor
            const auto I =
                quatToMat3(object->orientation) *
                object->dynamicProperties->invRotationalInertiaTensor *
                quatToMat3(glm::conjugate(object->orientation));
            const auto angularVelocity = I * object->dynamicProperties->angularMomentum;
            object->orientation += subDt * glm::cross(glm::quat(0, angularVelocity), object->orientation);
            // const auto torque = glm::cross(cp, Ft);
            // object->dynamicProperties->angularMomentum += subDt * torque;

            // NOTE can be done not every step, but this is too trivial so we don't bother
            object->orientation = glm::normalize(object->orientation);

            // Invalidate modelMatrix (force further recalculation)
            // Do it here because this has been the biggest state change,
            // but not later because the next step requires presice positions
            object->modelMatrixCached = std::nullopt;
        }
        const auto mark2 = std::chrono::high_resolution_clock::now();

        // Check for and handle collisions
        processedPairsOnThisIteration.clear();
        for (auto it = objectsInBuckets.begin(); it != objectsInBuckets.end();) {
            const auto& [_bucket, objects] = *it;
            if (objects.empty()) {
                it = objectsInBuckets.erase(it);
                continue;
            }

            for (size_t i = 0; i < objects.size(); i++) {
                auto& obj1 = objects[i];

                // For each object, test it against all other objects in current bucket...
                for (size_t j = i + 1; j < objects.size(); j++) {
                    auto& obj2 = objects[j];
                    if (obj1->isStationary() && obj2->isStationary()) continue;

                    auto id1 = obj1->id;
                    auto id2 = obj2->id;
                    if (id1 > id2) std::swap(id1, id2);
                    const auto& [_, notPresentBefore] = processedPairsOnThisIteration.emplace(id1, id2);
                    if (notPresentBefore) process(*obj1, *obj2);
                }
            }

            ++it;
        }
        const auto mark3 = std::chrono::high_resolution_clock::now();

        // Put objects into proper buckets after they have finished being moved
        reassign();
        const auto mark4 = std::chrono::high_resolution_clock::now();

        static int stepTimer = 0;
        if (false && ++stepTimer == 100) {
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
// =================== Register/remove/get ====================================
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
// =================== Reassign ===============================================
// ============================================================================
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


void HmlPhysics::printStats() const noexcept {
    std::map<uint32_t, uint32_t> countOfBucketsWithSize;
    for (const auto& [bucket, objects] : objectsInBuckets) {
        const auto size = objects.size();
        countOfBucketsWithSize[size]++;
    }

    uint32_t min = objects.size();
    uint32_t max = 0;
    float avg = 0;
    for (const auto& [size, count] : countOfBucketsWithSize) {
        std::cout << "With size [" << size << "] there are " << count << " buckets\n";
        if (size < min) min = size;
        if (size > max) max = size;
        avg += size * count;
    }

    std::cout << "Min size=" << min << "; Max size=" << max << "; Avg in bucket=" << (avg / objectsInBuckets.size()) << "\n";
}
// ============================================================================
// ======================== Geometry helpers ==================================
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


float HmlPhysics::pointToLineDstSqr(const glm::vec3& point, const glm::vec3& l1, const glm::vec3& l2) noexcept {
    const auto nom = glm::cross(l1 - l2, l2 - point);
    const float nomSqr = glm::dot(nom, nom);
    const auto denom = (l1 - l2);
    const float denomSqr = glm::dot(denom, denom);
    return nomSqr / denomSqr;
}


// NOTE planeDir need not be normalized thanks to the formula for t
std::optional<glm::vec3> HmlPhysics::linePlaneIntersection(
        const glm::vec3& linePoint, const glm::vec3& lineDirNorm,
        const glm::vec3& planePoint, const glm::vec3& planeDir) noexcept {
    const auto dot = glm::dot(lineDirNorm, planeDir);
    if (dot == 0) return std::nullopt;
    const auto t = (glm::dot(planeDir, planePoint) - glm::dot(planeDir, linePoint)) / dot;
    return { linePoint + lineDirNorm * t };
}


std::optional<glm::vec3> HmlPhysics::edgePlaneIntersection(
        const glm::vec3& linePoint1, const glm::vec3& linePoint2,
        const glm::vec3& planePoint, const glm::vec3& planeDir) noexcept {
    const auto lineDir = glm::normalize(linePoint2 - linePoint1);
    const auto dot = glm::dot(lineDir, planeDir);
    if (dot == 0) return std::nullopt;
    const auto tp = glm::dot(planeDir, planePoint);
    const auto t1 = glm::dot(planeDir, linePoint1);
    const auto t2 = glm::dot(planeDir, linePoint2);
    if ((tp - t1) * (t2 - tp) < 0) return std::nullopt;
    const auto t = (tp - t1) / dot;
    const auto res = linePoint1 + lineDir * t;
    return { res };
}


// Returns whether point P located on the same plane as ABC is
// inside the rectangle formed by two sides AB and AC
bool HmlPhysics::pointInsideRect(const glm::vec3& P, const glm::vec3& A, const glm::vec3& B, const glm::vec3& C) noexcept {
    const auto AB = B - A;
    const auto AC = C - A;
    const auto AP = P - A;
    const float projAb = glm::dot(AP, AB);
    const float projAc = glm::dot(AP, AC);
    const float maxAb = glm::dot(AB, AB);
    const float maxAc = glm::dot(AC, AC);
    return 0.0f <= projAb && projAb <= maxAb && 0.0f <= projAc && projAc <= maxAc;
};
// ============================================================================
// ===================== AVX/SSE ==============================================
// ============================================================================
void edgeFaceIntersection(
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
        const float planeDir_zs_ptr[4]
        ) noexcept {
    const __m128 edgePointA_xs = _mm_load_ps(edgePointA_xs_ptr);
    const __m128 edgePointA_ys = _mm_load_ps(edgePointA_ys_ptr);
    const __m128 edgePointA_zs = _mm_load_ps(edgePointA_zs_ptr);
    const __m128 edgePointB_xs = _mm_load_ps(edgePointB_xs_ptr);
    const __m128 edgePointB_ys = _mm_load_ps(edgePointB_ys_ptr);
    const __m128 edgePointB_zs = _mm_load_ps(edgePointB_zs_ptr);
    const __m128 planePointA_xs = _mm_load_ps(planePointA_xs_ptr);
    const __m128 planePointA_ys = _mm_load_ps(planePointA_ys_ptr);
    const __m128 planePointA_zs = _mm_load_ps(planePointA_zs_ptr);
    const __m128 planePointB_xs = _mm_load_ps(planePointB_xs_ptr);
    const __m128 planePointB_ys = _mm_load_ps(planePointB_ys_ptr);
    const __m128 planePointB_zs = _mm_load_ps(planePointB_zs_ptr);
    const __m128 planePointC_xs = _mm_load_ps(planePointC_xs_ptr);
    const __m128 planePointC_ys = _mm_load_ps(planePointC_ys_ptr);
    const __m128 planePointC_zs = _mm_load_ps(planePointC_zs_ptr);
    const __m128 planeDir_xs = _mm_load_ps(planeDir_xs_ptr);
    const __m128 planeDir_ys = _mm_load_ps(planeDir_ys_ptr);
    const __m128 planeDir_zs = _mm_load_ps(planeDir_zs_ptr);

    // edgeDir = edgePointB - edgePointA;
    __m128 edgeDir_xs = _mm_sub_ps(edgePointB_xs, edgePointA_xs);
    __m128 edgeDir_ys = _mm_sub_ps(edgePointB_ys, edgePointA_ys);
    __m128 edgeDir_zs = _mm_sub_ps(edgePointB_zs, edgePointA_zs);
    // normalize(edgeDir);
    hml_norm128(edgeDir_xs, edgeDir_ys, edgeDir_zs);

    // dot = glm::dot(edgeDir, planeDir)
    const __m128 dot = hml_dot128(
            edgeDir_xs, edgeDir_ys, edgeDir_zs,
            planeDir_xs, planeDir_ys, planeDit_zs);
}


// void edgeFaceIntersection(
//         const float edgePointA_xs_ptr[8],
//         const float edgePointA_ys_ptr[8],
//         const float edgePointA_zs_ptr[8],
//         const float edgePointB_xs_ptr[8],
//         const float edgePointB_ys_ptr[8],
//         const float edgePointB_zs_ptr[8],
//         const float planePointA_xs_ptr[8],
//         const float planePointA_ys_ptr[8],
//         const float planePointA_zs_ptr[8],
//         const float planePointB_xs_ptr[8],
//         const float planePointB_ys_ptr[8],
//         const float planePointB_zs_ptr[8],
//         const float planePointC_xs_ptr[8],
//         const float planePointC_ys_ptr[8],
//         const float planePointC_zs_ptr[8],
//         const float planeDir_xs_ptr[8],
//         const float planeDir_ys_ptr[8],
//         const float planeDir_zs_ptr[8]
//         ) noexcept {
//
// }
//
//
// void edgeFaceIntersection(
//         const float edgePointA_xs_ptr[12],
//         const float edgePointA_ys_ptr[12],
//         const float edgePointA_zs_ptr[12],
//         const float edgePointB_xs_ptr[12],
//         const float edgePointB_ys_ptr[12],
//         const float edgePointB_zs_ptr[12],
//         const float planePointA_xs_ptr[12],
//         const float planePointA_ys_ptr[12],
//         const float planePointA_zs_ptr[12],
//         const float planePointB_xs_ptr[12],
//         const float planePointB_ys_ptr[12],
//         const float planePointB_zs_ptr[12],
//         const float planePointC_xs_ptr[12],
//         const float planePointC_ys_ptr[12],
//         const float planePointC_zs_ptr[12],
//         const float planeDir_xs_ptr[12],
//         const float planeDir_ys_ptr[12],
//         const float planeDir_zs_ptr[12]
//         ) noexcept {
//
// }


__m128 hml_dot128(
        __m128 xs1, __m128 ys1, __m128 zs1,
        __m128 xs2, __m128 ys2, __m128 zs2) noexcept {
    return _mm_add_ps(
        _mm_add_ps(
            _mm_mul_ps(xs1, xs2),
            _mm_mul_ps(ys1, ys2)),
        _mm_mul_ps(zs1, zs2));
}


__m256 hml_dot256(
        __m256 xs1, __m256 ys1, __m256 zs1,
        __m256 xs2, __m256 ys2, __m256 zs2) noexcept {
    return _mm256_add_ps(
        _mm256_add_ps(
            _mm256_mul_ps(xs1, xs2),
            _mm256_mul_ps(ys1, ys2)),
        _mm256_mul_ps(zs1, zs2));
}


void hml_norm128(__m128& xs, __m128& ys, __m128& zs) noexcept {
    const __m128 xs_sqr = _mm_mul_ps(xs, xs);
    const __m128 ys_sqr = _mm_mul_ps(ys, ys);
    const __m128 zs_sqr = _mm_mul_ps(zs, zs);
    const __m128 s = _mm_add_ps(_mm_add_ps(xs_sqr, ys_sqr), zs_sqr);
    const __m128 s_rsqrt = _mm_rsqrt_ps(s);
    xs = _mm_mul_ps(xs, s_rsqrt);
    ys = _mm_mul_ps(ys, s_rsqrt);
    zs = _mm_mul_ps(zs, s_rsqrt);
}


void hml_norm256(__m256& xs, __m256& ys, __m256& zs) noexcept {
    const __m256 xs_sqr = _mm256_mul_ps(xs, xs);
    const __m256 ys_sqr = _mm256_mul_ps(ys, ys);
    const __m256 zs_sqr = _mm256_mul_ps(zs, zs);
    const __m256 s = _mm256_add_ps(_mm256_add_ps(xs_sqr, ys_sqr), zs_sqr);
    const __m256 s_rsqrt = _mm256_rsqrt_ps(s);
    xs = _mm256_mul_ps(xs, s_rsqrt);
    ys = _mm256_mul_ps(ys, s_rsqrt);
    zs = _mm256_mul_ps(zs, s_rsqrt);
}


float hml_hsum128(__m128 v) noexcept {
    __m128 shuf = _mm_movehdup_ps(v);        // broadcast elements 3,1 to 2,0
    __m128 sums = _mm_add_ps(v, shuf);
    shuf        = _mm_movehl_ps(shuf, sums); // high half -> low half
    sums        = _mm_add_ss(sums, shuf);
    return        _mm_cvtss_f32(sums);
}


float hml_hsum256(__m256 v) noexcept {
    __m128 vlow  = _mm256_castps256_ps128(v);
    __m128 vhigh = _mm256_extractf128_ps(v, 1); // high 128
           vlow  = _mm_add_ps(vlow, vhigh);     // add the low 128
    return hml_hsum128(vlow);
}
// ============================================================================
// ===================== GJK ==================================================
// ============================================================================
glm::vec3 HmlPhysics::furthestPointInDir(const glm::vec3& dir, std::span<const glm::vec3> ps) noexcept {
    float max = std::numeric_limits<float>::lowest();
    glm::vec3 furthest;
    for (const auto& p : ps) {
        const auto proj = glm::dot(dir, p);
        const float foundNew = static_cast<float>(proj > max);
        max      = max      * (1 - foundNew) + proj * foundNew;
        furthest = furthest * (1 - foundNew) + p    * foundNew;
    }
    return furthest;
}


glm::vec3 HmlPhysics::calcSupport(const glm::vec3& dir, std::span<const glm::vec3> ps1, std::span<const glm::vec3> ps2) noexcept {
    return furthestPointInDir(dir, ps1) - furthestPointInDir(-dir, ps2);
}


std::optional<HmlPhysics::Detection> HmlPhysics::gjk(const Object::Box& b1, const Object::Box& b2) noexcept {
    static const auto sameDir = [](const glm::vec3& a, const glm::vec3& b){
        return glm::dot(a, b) >= 0.0f;
    };
    static const auto forLine = [](Simplex& simplex, glm::vec3& dir){
        const auto& a = simplex[0];
        const auto& b = simplex[1];
        const auto ab = b - a;
        const auto ao =   - a;
        if (sameDir(ab, ao)) {
            dir = glm::cross(glm::cross(ab, ao), ab);
        } else {
            simplex = { a };
            dir = ao;
        }
        return false;
    };
    static const auto forTriangle = [](Simplex& simplex, glm::vec3& dir){
        const auto& a = simplex[0];
        const auto& b = simplex[1];
        const auto& c = simplex[2];
        const auto ab = b - a;
        const auto ac = c - a;
        const auto ao =   - a;
        const auto abc = glm::cross(ab, ac);
        if (sameDir(glm::cross(abc, ac), ao)) {
            if (sameDir(ac, ao)) {
                simplex = { a, c };
                dir = glm::cross(glm::cross(ac, ao), ac);
            } else {
                simplex = { a, b };
                return forLine(simplex, dir);
            }
        } else {
            if (sameDir(glm::cross(ab, abc), ao)) {
                simplex = { a, b };
                return forLine(simplex, dir);
            } else {
                if (sameDir(abc, ao)) {
                    dir = abc;
                } else {
                    simplex = { a, c, b };
                    dir = -abc;
                }
                return false;
            }
        }
        return false;
    };
    static const auto forTetrahedron = [](Simplex& simplex, glm::vec3& dir){
        const auto& a = simplex[0];
        const auto& b = simplex[1];
        const auto& c = simplex[2];
        const auto& d = simplex[3];
        const auto ab = b - a;
        const auto ac = c - a;
        const auto ad = d - a;
        const auto ao =   - a;
        const auto abc = glm::cross(ab, ac);
        const auto acd = glm::cross(ac, ad);
        const auto adb = glm::cross(ad, ab);
        if (sameDir(abc, ao)) {
            simplex = { a, b, c };
            return forTriangle(simplex, dir);
        }
        if (sameDir(acd, ao)) {
            simplex = { a, c, d };
            return forTriangle(simplex, dir);
        }
        if (sameDir(adb, ao)) {
            simplex = { a, d, b };
            return forTriangle(simplex, dir);
        }
        return true;
    };
    static const auto nextSimplex = [](Simplex& simplex, glm::vec3& dir){
        switch (simplex.size()) {
            case 2: return forLine       (simplex, dir);
            case 3: return forTriangle   (simplex, dir);
            case 4: return forTetrahedron(simplex, dir);
        }
        assert(false && "Unexpected Simplex size in nextSimplex()"); return false; // stub
    };

    const glm::vec3 initialDir{1, 0, 0};
    const auto ps1 = b1.toPoints();
    const auto ps2 = b2.toPoints();
    auto support = calcSupport(initialDir, ps1, ps2);

    Simplex simplex;
    simplex.push_front(support);

    glm::vec3 dir = -support;
    while (true) {
        support = calcSupport(dir, ps1, ps2);
        if (glm::dot(support, dir) <= 0.0f) return std::nullopt; // no collision
        simplex.push_front(support);
        if (nextSimplex(simplex, dir)) {
            auto detOpt = epa(simplex, ps1, ps2);
            assert(detOpt && "EPA result differs from GJK");
            // detOpt->contactPoints = { avg(std::span(simplex.begin(), simplex.end())) };
            return detOpt;
        }
    }

    return std::nullopt;
}
// ============================================================================
// ===================== EPA ==================================================
// ============================================================================
std::optional<HmlPhysics::Detection> HmlPhysics::epa(const Simplex& simplex, const auto& ps1, const auto& ps2) noexcept {
    static const auto getFaceNormals = [](
            const std::vector<glm::vec3>& polytope,
            const std::vector<size_t>& faces){
        std::vector<glm::vec4> normals;
        size_t minTriangle = 0;
        float minDst = std::numeric_limits<float>::max();

        for (size_t i = 0; i < faces.size(); i += 3) {
            const auto& a = polytope[faces[i + 0]];
            const auto& b = polytope[faces[i + 1]];
            const auto& c = polytope[faces[i + 2]];

            auto normal = glm::normalize(glm::cross(b - a, c - a));
            float dst = glm::dot(normal, a);
            if (dst < 0) {
                normal *= -1.0f;
                dst    *= -1.0f;
            }

            normals.emplace_back(normal.x, normal.y, normal.z, dst);
            if (dst < minDst) {
                minTriangle = i / 3;
                minDst = dst;
            }
        }

        return std::make_pair<std::vector<glm::vec4>, size_t>(std::move(normals), std::move(minTriangle));
    };
    static const auto addIfUniqueEdge = [](std::vector<std::pair<size_t, size_t>>& edges, const std::vector<size_t>& faces, size_t a, size_t b){
        //    0--<--3
        //   / \ B /  A: 2-0
        //  / A \ /   B: 0-2
        // 1-->--2
        const auto reverse = std::find(edges.begin(), edges.end(), std::make_pair(faces[b], faces[a]));
        if (reverse != edges.end()) edges.erase(reverse);
        else edges.emplace_back(faces[a], faces[b]);
    };

    std::vector<glm::vec3> polytope(simplex.begin(), simplex.end());
    std::vector<size_t> faces = {
        0, 1, 2,
        0, 3, 1,
        0, 2, 3,
        1, 3, 2
    };

    auto [normals, minFace] = getFaceNormals(polytope, faces);

    float minDst = std::numeric_limits<float>::max();
    glm::vec3 minNormal;
    while (minDst == std::numeric_limits<float>::max()) {
        minNormal = glm::vec3(normals[minFace]);
        minDst = normals[minFace].w;

        const auto support = calcSupport(minNormal, ps1, ps2);
        const float supportDst = glm::dot(minNormal, support);
        if (std::abs(supportDst - minDst) > 0.001f) {
            minDst = std::numeric_limits<float>::max();

            std::vector<std::pair<size_t, size_t>> uniqueEdges;
            for (size_t i = 0; i < normals.size(); i++) {
                if (glm::dot(glm::vec3(normals[i]), support) < 0.0f) continue;

                size_t f = i * 3;
                addIfUniqueEdge(uniqueEdges, faces, f + 0, f + 1);
                addIfUniqueEdge(uniqueEdges, faces, f + 1, f + 2);
                addIfUniqueEdge(uniqueEdges, faces, f + 2, f + 0);

                faces[f + 2] = faces.back(); faces.pop_back();
                faces[f + 1] = faces.back(); faces.pop_back();
                faces[f + 0] = faces.back(); faces.pop_back();
                normals[i] = normals.back(); normals.pop_back();

                i--;
            }

            std::vector<size_t> newFaces;
            for (const auto& [edgeIndex1, edgeIndex2] : uniqueEdges) {
                newFaces.push_back(edgeIndex1);
                newFaces.push_back(edgeIndex2);
                newFaces.push_back(polytope.size());
            }

            polytope.push_back(support);

            const auto& [newNormals, newMinFace] = getFaceNormals(polytope, newFaces);

            float oldMinDst = std::numeric_limits<float>::max();
            for (size_t i = 0; i < normals.size(); i++) {
                if (normals[i].w < oldMinDst) {
                    oldMinDst = normals[i].w;
                    minFace = i;
                }
            }

            if (newNormals[newMinFace].w < oldMinDst) {
                minFace = newMinFace + normals.size();
            }

            faces  .insert(faces  .end(), newFaces  .begin(), newFaces  .end());
            normals.insert(normals.end(), newNormals.begin(), newNormals.end());
        }
    }

    return Detection{
        .dir = minNormal,
        .extent = minDst + 0.001f,
        .contactPoints = {},
    };
}
// ============================================================================
// ===================== Helpers ==============================================
// ============================================================================
std::ostream& operator<<(std::ostream& stream, const glm::vec3& v) {
    stream << "[" << v.x << ";" << v.y << ";" << v.z << "]";
    return stream;
}

std::ostream& operator<<(std::ostream& stream, const glm::quat& q) {
    stream << "[" << q.x << ";" << q.y << ";" << q.z << ";(" << q.w << ")" << "]";
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

    // glm::mat4 cross(const glm::quat& q, const glm::mat4& m) {
    //     glm::mat4 res;
    //     for (size_t i = 0; i < 4; i++) {
    //         // const glm::quat mq{m[i][3], m[i][0], m[i][1], m[i][2]};
    //         const glm::quat mq{m[i][0], m[i][1], m[i][2], m[i][3]};
    //         const auto r = glm::cross(q, mq);
    //         res[i] = { r.x, r.y, r.z, r.w };
    //         // res[i] = { r.w, r.x, r.y, r.z };
    //     }
    //     return res;
    // }
    //
    // glm::mat4 cross(const glm::mat4& m, const glm::quat& q) {
    //     return glm::cross(q, m);
    //     // glm::mat4 res;
    //     // for (size_t i = 0; i < 4; i++) {
    //     //     const glm::quat mq{m[i][0], m[i][1], m[i][2], m[i][3]};
    //     //     const auto r = glm::cross(q, mq);
    //     //     res[i] = { r.x, r.y, r.z, r.w };
    //     // }
    //     // return res;
    // }
}


AxisAngle quatToAxisAngle(const glm::quat& q) noexcept {
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

std::ostream& operator<<(std::ostream& stream, const AxisAngle& aa) {
    stream << aa.axis << "{" << aa.angle << "}";
    return stream;
}


glm::mat3 quatToMat3(const glm::quat& q) noexcept {
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
// ============================================================================
// ======================== Object ============================================
// ============================================================================
const glm::mat4& HmlPhysics::Object::modelMatrix() const noexcept {
    if (!modelMatrixCached) modelMatrixCached = generateModelMatrix();
    return *modelMatrixCached;
}

glm::mat4 HmlPhysics::Object::generateModelMatrix() const noexcept {
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


bool HmlPhysics::Object::AABB::intersects(const AABB& other) const noexcept {
    return (begin.x       < other.end.x &&
            other.begin.x < end.x       &&
            begin.y       < other.end.y &&
            other.begin.y < end.y       &&
            begin.z       < other.end.z &&
            other.begin.z < end.z);
}

bool HmlPhysics::Object::isSphere() const noexcept { return type == Type::Sphere; }
bool HmlPhysics::Object::isBox()    const noexcept { return type == Type::Box; }

HmlPhysics::Object::Sphere& HmlPhysics::Object::asSphere() noexcept {
    assert(isSphere() && "::> Trying to convert Object to Sphere but it is not one!");
    return *reinterpret_cast<Sphere*>(this);
}
const HmlPhysics::Object::Sphere& HmlPhysics::Object::asSphere() const noexcept {
    assert(isSphere() && "::> Trying to convert Object to Sphere but it is not one!");
    return *reinterpret_cast<const Sphere*>(this);
}
HmlPhysics::Object::Box& HmlPhysics::Object::asBox() noexcept {
    assert(isBox() && "::> Trying to convert Object to Box but it is not one!");
    return *reinterpret_cast<Box*>(this);
}
const HmlPhysics::Object::Box& HmlPhysics::Object::asBox() const noexcept {
    assert(isBox() && "::> Trying to convert Object to Box but it is not one!");
    return *reinterpret_cast<const Box*>(this);
}

HmlPhysics::Object HmlPhysics::Object::createSphere(const glm::vec3& center, float radius) noexcept {
    Object object{Type::Sphere};
    object.position = { center.x, center.y, center.z };
    object.dimensions = { radius };
    return object;
}
HmlPhysics::Object HmlPhysics::Object::createSphere(const glm::vec3& center, float radius,
        float mass, const glm::vec3& velocity, const glm::vec3& angularVelocity) noexcept {
    Object object{Type::Sphere};
    object.position = { center.x, center.y, center.z };
    object.dimensions = { radius };
    const auto rotationalInertiaTensor = object.asSphere().rotationalInertiaTensor(mass);
    object.dynamicProperties = DynamicProperties(mass, rotationalInertiaTensor, velocity, angularVelocity);
    return object;
}
HmlPhysics::Object HmlPhysics::Object::createBox(const glm::vec3& center, const glm::vec3& halfDimensions) noexcept {
    Object object{Type::Box};
    object.position = { center.x, center.y, center.z };
    object.dimensions = { halfDimensions.x, halfDimensions.y, halfDimensions.z };
    return object;
}
HmlPhysics::Object HmlPhysics::Object::createBox(const glm::vec3& center, const glm::vec3& halfDimensions,
        float mass, const glm::vec3& velocity, const glm::vec3& angularVelocity) noexcept {
    Object object{Type::Box};
    object.position = { center.x, center.y, center.z };
    object.dimensions = { halfDimensions.x, halfDimensions.y, halfDimensions.z };
    const auto rotationalInertiaTensor = object.asBox().rotationalInertiaTensor(mass);
    object.dynamicProperties = DynamicProperties(mass, rotationalInertiaTensor, velocity, angularVelocity);
    return object;
}
// ============================================================================
// ======================== Sphere ============================================
// ============================================================================
HmlPhysics::Object::AABB HmlPhysics::Object::Sphere::aabb() const noexcept {
    return AABB{
        .begin = center - glm::vec3{radius},
        .end   = center + glm::vec3{radius},
    };
}

glm::mat3 HmlPhysics::Object::Sphere::rotationalInertiaTensor(float mass) const noexcept {
    const float m = 2.0f / 5.0f * mass * radius * radius;
    return glm::mat3{
        m, 0, 0,
        0, m, 0,
        0, 0, m
    };
}
// ============================================================================
// ======================== Box ===============================================
// ============================================================================
HmlPhysics::Object::Box::OrientationData HmlPhysics::Object::Box::orientationDataNormalized() const noexcept {
    const auto& mm = asObject().modelMatrix();
    return OrientationData{
        .i = glm::normalize(glm::vec3{ mm[0][0], mm[0][1], mm[0][2] }),
        .j = glm::normalize(glm::vec3{ mm[1][0], mm[1][1], mm[1][2] }),
        .k = glm::normalize(glm::vec3{ mm[2][0], mm[2][1], mm[2][2] })
    };
}

HmlPhysics::Object::Box::OrientationData HmlPhysics::Object::Box::orientationDataUnnormalized() const noexcept {
    const auto& mm = asObject().modelMatrix();
    return OrientationData{
        .i = glm::vec3{ mm[0][0], mm[0][1], mm[0][2] },
        .j = glm::vec3{ mm[1][0], mm[1][1], mm[1][2] },
        .k = glm::vec3{ mm[2][0], mm[2][1], mm[2][2] }
    };
}

std::array<glm::vec3, 8> HmlPhysics::Object::Box::toPoints() const noexcept {
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

HmlPhysics::Object::AABB HmlPhysics::Object::Box::aabb() const noexcept {
    const auto& [i, j, k] = orientationDataUnnormalized();
    glm::vec3 min{ std::numeric_limits<float>::max() };
    glm::vec3 max{ std::numeric_limits<float>::lowest() };
    for (float a = -1; a <= 1; a += 2) {
        for (float b = -1; b <= 1; b += 2) {
            for (float c = -1; c <= 1; c += 2) {
                const auto p = a * i + b * j + c * k; // NOTE: +center to make a real point
                min.x = min.x * (1 - (p.x < min.x)) + p.x * (p.x < min.x);
                min.y = min.y * (1 - (p.y < min.y)) + p.y * (p.y < min.y);
                min.z = min.z * (1 - (p.z < min.z)) + p.z * (p.z < min.z);
                max.x = max.x * (1 - (p.x > max.x)) + p.x * (p.x > max.x);
                max.y = max.y * (1 - (p.y > max.y)) + p.y * (p.y > max.y);
                max.z = max.z * (1 - (p.z > max.z)) + p.z * (p.z > max.z);
            }
        }
    }
    return AABB{
        .begin = center + min,
        .end   = center + max,
    };
}

glm::mat3 HmlPhysics::Object::Box::rotationalInertiaTensor(float mass) const noexcept {
    const auto dimSqr = 4.0f * halfDimensions * halfDimensions;
    const float m = mass / 12.0f;
    return glm::mat3{
        m * (dimSqr.y + dimSqr.z), 0, 0,
        0, m * (dimSqr.x + dimSqr.y), 0,
        0, 0, m * (dimSqr.x + dimSqr.z)
    };
}
