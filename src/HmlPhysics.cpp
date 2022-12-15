#include "HmlPhysics.h"


void HmlPhysics::threadFunc() noexcept {
    while (!threadedData.terminate.load()) {
        if (objects.empty()) threadedData.hasNewObjectsToRegister.wait(false); // wait until not false
        const bool hasNewObjectsToRegister = threadedData.hasNewObjectsToRegister.exchange(false);
        if (hasNewObjectsToRegister) {
            const std::lock_guard<std::mutex> lock(threadedData.objectsToRegisterMutex);
            assert((!threadedData.objectsToRegister.empty() || threadedData.terminate.load()) && "Empty register vector even though the flag indicated otherwise");
            for (const auto& object : threadedData.objectsToRegister) {
                internalRegisterObject(object);
            }
            threadedData.objectsToRegister.clear();
        }

        const float check = threadedData.accumulatedDt.load();
        if (check == 0.0f) {
            // We are too fast
            threadedData.accumulatedDt.wait(0.0f); // wait until not 0.0f
            // Adjust our speed
            threadedData.substeps++;
        } else {
            // We are a little slow
            if (threadedData.substeps > 1) threadedData.substeps--;
        }

        float dt = threadedData.accumulatedDt.exchange(0.0f);
        assert(dt > 0.0f && "Returned from wait() with 0.0f");
        if (dt > ThreadedData::MAX_ALLOWED_LAG_SECONDS) {
            // std::cout << ":> VERY SLOW\n";
            // We are VERY slow
            const float extra = dt - ThreadedData::MAX_ALLOWED_LAG_SECONDS;
            dt = ThreadedData::MAX_ALLOWED_LAG_SECONDS;
            threadedData.accumulatedDt += extra;
            // Adjust our speed
            if (threadedData.substeps > 1) threadedData.substeps /= 2;
        }

        for (int i = 0; i < threadedData.substeps; i++) {
            step(dt / threadedData.substeps);
        }

        { // Update the up-to-date modelMatrices array for use by outside world
            const std::lock_guard<std::mutex> lock(threadedData.modelMatricesMutex);
            threadedData.modelMatrices.clear();
            for (const auto& object : objects) threadedData.modelMatrices.push_back(std::make_pair(object.id, object.modelMatrix()));
        }
    }
    if constexpr (LOG_INFO) std::cout << ":> Physics thread terminated!\n";
}


HmlPhysics::HmlPhysics(Mode mode) noexcept : mode(mode) {
    switch (mode) {
        case Mode::SameThread:
            if constexpr (LOG_INFO) std::cout << ":> Starting HmlPhysics in the same thread.\n";
            break;
        case Mode::SameThreadAndHelperThreads:
            if constexpr (LOG_INFO) std::cout << ":> Starting HmlPhysics in the same thread and using thread pool with helper threads.\n";
            break;
        case Mode::AnotherThread:
            if constexpr (LOG_INFO) std::cout << ":> Starting HmlPhysics in another thread.\n";
            break;
        case Mode::AnotherThreadAndHelperThreads:
            if constexpr (LOG_INFO) std::cout << ":> Starting HmlPhysics in another thread and using thread pool with helper threads.\n";
            break;
        default: assert(false && "Unhandled Mode");
    }

    if (hasHelperThreads()) {
        constexpr size_t THREAD_POOL_SIZE = 5;
        threadPool.resize(THREAD_POOL_SIZE);
    }

    if (hasSelfThread()) {
        thread = std::thread(&HmlPhysics::threadFunc, this);
    }
}


HmlPhysics::~HmlPhysics() noexcept {
    if (!hasSelfThread()) return;
    if (!threadedData.terminate.load()) terminate(); // if has not been terminated yet
}


void HmlPhysics::terminate() noexcept {
    if (!hasSelfThread()) return;
    if constexpr (LOG_INFO) std::cout << ":> Waiting for physics thread to finish...\n";

    // Do this first so that once we've unblocked the thread it could already see the new status
    threadedData.terminate.store(true);

    // Notify (awake) all places that may be hanged due to waiting
    threadedData.accumulatedDt.store(1.0f); // because we wait for not 0
    threadedData.accumulatedDt.notify_one();
    threadedData.hasNewObjectsToRegister.store(true); // because we wait for not false
    threadedData.hasNewObjectsToRegister.notify_one();

    thread.join();
}
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
    const std::array<Object::Box::OrientationData, 2> orientationDataPacked{ orientationData1, orientationData2 };
    std::array<glm::vec3, 8 * 2> pointsPacked;
    b1.toPointsAt(static_cast<float*>(&pointsPacked[0].x));
    b2.toPointsAt(static_cast<float*>(&pointsPacked[8].x));
    const std::array<glm::vec3, 8>& p1 = *reinterpret_cast<std::array<glm::vec3, 8>*>(&pointsPacked[0].x);
    const std::array<glm::vec3, 8>& p2 = *reinterpret_cast<std::array<glm::vec3, 8>*>(&pointsPacked[8].x);

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



    std::vector<glm::vec3> contactPoints;
    // findContactPointsBoxes(p1, p2, orientationData2, contactPoints);
    // findContactPointsBoxes(p2, p1, orientationData1, contactPoints);
    findContactPointsBoxesAvxFastest(pointsPacked, orientationDataPacked, contactPoints);

    if (contactPoints.empty()) return std::nullopt;
    // if (true) {
    //     std::cout << "=====================================" << '\n';
    //     std::cout << "Dir=" << dir << " minExtent=" << minExtent << '\n';
    //     std::cout << "C1=" << b1.center << " C2=" << b2.center << '\n';
    //     std::cout << "IJK1=" << i1 << " " << j1 << " " << k1 << '\n';
    //     std::cout << "IJK2=" << i2 << " " << j2 << " " << k2 << '\n';
    //     std::cout << "P1:" << '\n';
    //     for (const auto& p : p1) std::cout << p << " ";
    //     std::cout << '\n';
    //     std::cout << "P2:" << '\n';
    //     for (const auto& p : p2) std::cout << p << " ";
    //     std::cout << '\n';
        // std::cout << "CP:" << '\n';
        // for (const auto& cp : contactPoints) std::cout << cp << " ";
        // std::cout << '\n';
    //     std::cout << "=====================================" << '\n';
    // }
    // assert(!contactPoints.empty() && "No contact points!");
    // addContactPointsFromOntoAvx(p1, p2, orientationData2, contactPoints);
    // addContactPointsFromOntoAvx(p2, p1, orientationData1, contactPoints);

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
// ===================== Process ==============================================
// ============================================================================
HmlPhysics::ProcessResult HmlPhysics::process(const Object& obj1, const Object& obj2) noexcept {
    assert(!(obj1.isStationary() && obj2.isStationary()) && "Shouldn't've called process() with both objects being stationary");

    // ================ Resolve intersection ================
    // const auto mark1 = std::chrono::high_resolution_clock::now();
    std::optional<Detection> detectionOpt = std::nullopt;
    if      (obj1.isSphere() && obj2.isSphere()) detectionOpt = detect(obj1.asSphere(), obj2.asSphere());
    else if (obj1.isBox()    && obj2.isSphere()) detectionOpt = detect(obj1.asBox(),    obj2.asSphere());
    else if (obj1.isSphere() && obj2.isBox())    detectionOpt = detect(obj1.asSphere(), obj2.asBox());
    else if (obj1.isBox()    && obj2.isBox())    detectionOpt = detect(obj1.asBox(),    obj2.asBox());
    // const auto mark2 = std::chrono::high_resolution_clock::now();

    if (!detectionOpt) return std::make_pair(ObjectAdjustment{}, ObjectAdjustment{});
    const auto& [dir, extent, contactPoints] = *detectionOpt;

    const bool oneStationary = obj1.isStationary() || obj2.isStationary();
    const auto positionAdjustment = dir * extent * (oneStationary ? 1.0f : 0.5f);

    // ================ Resolve velocities ================
    const auto obj1DP = obj1.dynamicProperties.value_or(Object::DynamicProperties{});
    const auto obj2DP = obj2.dynamicProperties.value_or(Object::DynamicProperties{});
    const auto relativeV = obj2DP.velocity - obj1DP.velocity;
    // if (glm::dot(relativeV, dir) > 0.0f) {
    //     // Objects are already moving apart
    //     return std::make_pair(VelocitiesAdjustment{}, VelocitiesAdjustment{});
    // }
    const auto cp = avg(contactPoints);
    const auto rap = cp - obj1.position;
    const auto rbp = cp - obj2.position;
    const auto a = glm::cross(obj1DP.invRotationalInertiaTensor * glm::cross(rap, dir), rap);
    const auto b = glm::cross(obj2DP.invRotationalInertiaTensor * glm::cross(rbp, dir), rbp);
    // e == 0 --- perfectly inelastic collision
    // e == 1 --- perfectly elastic collision, no kinetic energy is dissipated
    // const float e = std::min(obj1.restitution, obj2.restitution);
    const float e = 0.9f; // restitution
    const float nom = -(1.0f + e) * glm::dot(relativeV, dir);
    const float denom = (obj1DP.invMass + obj2DP.invMass) + glm::dot(a + b, dir);
    const float j = nom / denom;
    const auto impulse = j * dir;

    const float frictionCoeff = 50.0f;
    glm::vec3 friction = frictionCoeff * extent * glm::normalize(relativeV);
    friction = glm::vec3{0};

    // std::cout << "Impulse = " << impulse << '\n';
    // std::cout << "Dir = " << dir << " J = " << j << " denom = " << denom << " nom = " << nom << std::endl;
    // std::cout << "RAP = " << rap << " RBP = " << rbp << " a = " << a << " b = " << b << std::endl;
    // std::cout << "Delta1 = " << -glm::cross(rap, impulse) * obj1DP.invRotationalInertiaTensor << '\n';
    // std::cout << "Delta2 = " << +glm::cross(rbp, impulse) * obj2DP.invRotationalInertiaTensor << '\n';
    // std::cout << "-----------------------------------------------------------" << '\n';

    // const auto mark3 = std::chrono::high_resolution_clock::now();
    // const auto step1Mks = std::chrono::duration_cast<std::chrono::microseconds>(mark2 - mark1).count();
    // const auto step2Mks = std::chrono::duration_cast<std::chrono::microseconds>(mark3 - mark2).count();
    // std::cout << "Detection=" << static_cast<float>(step1Mks)
    //     << "mks. Resolve=" << static_cast<float>(step2Mks)
    //     << "mks. (" << (obj1.isBox() ? "Box":"Sphere") << "--" << (obj2.isBox() ? "Box":"Sphere") << ")\n";
    // if ((obj1.isBox() && obj2.isBox())) {
    //     static float sum = 0.0f;
    //     static int count = 0;
    //     sum += static_cast<float>(step1Mks);
    //     count++;
    //     static int aa = 0;
    //     if (aa++ == 70) {
    //         aa = 0;
    //         std::cout << (sum / static_cast<float>(count)) << "\n";
    //         sum = 0.0f;
    //         count = 0;
    //     }
    // }

    return std::make_pair(
        obj1.isStationary() ? ObjectAdjustment{} : ObjectAdjustment{
            .id              = obj1.id,
            .idOther         = obj2.id,
            .position        = - positionAdjustment,
            .velocity        = - impulse * obj1DP.invMass,
            .angularMomentum = - glm::cross(rap, impulse - friction) * obj1DP.invRotationalInertiaTensor,
        },
        obj2.isStationary() ? ObjectAdjustment{} : ObjectAdjustment{
            .id              = obj2.id,
            .idOther         = obj1.id,
            .position        = + positionAdjustment,
            .velocity        = + impulse * obj2DP.invMass,
            .angularMomentum = + glm::cross(rbp, impulse - friction) * obj2DP.invRotationalInertiaTensor,
        }
    );
}
// ============================================================================
// ===================== Main Update ==========================================
// ============================================================================
// static void assertGood(const std::shared_ptr<HmlPhysics::Object>& object, const char* msg) noexcept {
//     const glm::vec3 start{-55, 45, -55};
//     const glm::vec3 end{ 55, 55 + 100, 55 };
//     // assert(object->position < end && start < object->position);
//     bool good = (object->position < end && start < object->position);
//     if (!object->isStationary()) {
//         good &= object->dynamicProperties->velocity < glm::vec3{150,150,150} &&
//                 object->dynamicProperties->velocity > glm::vec3{-150,-150,-150};
//     }
//     if (!good) {
//         std::cout
//             << "T=" << (object->isBox() ? "Box" : "Sphere")
//             << "  P=" << object->position
//             << "  V=" << object->dynamicProperties->velocity
//             << "  O=" << object->orientation << " = " << quatToAxisAngle(object->orientation)
//             << "  AM=" << object->dynamicProperties->angularMomentum
//             << std::endl;
//             std::cout << "MSG=" << msg << std::endl;
//             assert(false);
//     }
// }


void HmlPhysics::updateForDt(float dt) noexcept {
    if (hasSelfThread()) {
        threadedData.accumulatedDt += dt;
        threadedData.accumulatedDt.notify_one();
    } else {
        // const float simulationSpeedFactor = 0.5f;
        const float simulationSpeedFactor = 1.0f;
        const uint8_t SUBSTEPS = 1;
        const float subDt = dt / SUBSTEPS * simulationSpeedFactor;
        for (uint8_t i = 0; i < SUBSTEPS; i++) step(subDt);
    }
}


void HmlPhysics::step(float dt) noexcept {
    auto boundingBoundsBeforeIt = allBoundingBucketsBefore.begin();

    const auto mark1 = std::chrono::high_resolution_clock::now();
    for (auto& object : objects) {
        if (object.isStationary()) continue;
        // ======================== Apply adjustments ========================
        {
            // TODO maybe make per-object to speed up
            for (auto it = adjustments.begin(); it != adjustments.end();) {
                const auto& [id, _otherId, positionAdj, velocityAdj, angularMomentumAdj] = *it;
                if (id == Object::INVALID_ID) {
                    it = adjustments.erase(it);
                    continue;
                }
                if (id == object.id) {
                    assert(!object.isStationary() && "adjusting a stationary object");
                    object.position                           += positionAdj;
                    object.dynamicProperties->velocity        += velocityAdj;
                    object.dynamicProperties->angularMomentum += angularMomentumAdj;
                    it = adjustments.erase(it);
                    // NOTE Don't break because there can be multiple adjustments for a single object
                    continue;
                }

                ++it;
            }
        }
        // ======================== Advance state ========================
        {
            // static constexpr glm::vec3 F {0,-9.8,0};
            // static constexpr glm::vec3 F {0,0,0};
            // static constexpr glm::vec3 cp{1,0,0};
            // static constexpr glm::vec3 Ft{0,0,0};
            const auto F = gravity;

            object.position += dt * object.dynamicProperties->velocity;
            object.dynamicProperties->velocity += dt * F; // NOTE * object.dynamicProperties->invMass;

            // World-space inverse inertia tensor
            const auto I =
                quatToMat3(object.orientation) *
                object.dynamicProperties->invRotationalInertiaTensor *
                quatToMat3(glm::conjugate(object.orientation));
            // const auto I = object.dynamicProperties->invRotationalInertiaTensor;
            const auto angularVelocity = I * object.dynamicProperties->angularMomentum;
            object.orientation += dt * glm::cross(glm::quat(0, angularVelocity), object.orientation);
            // const auto torque = glm::cross(cp, Ft);
            // object.dynamicProperties->angularMomentum += dt * torque;

            // NOTE can be done not every step, but this is too trivial so we don't bother
            object.orientation = glm::normalize(object.orientation);
            // Invalidate modelMatrix (force further recalculation)
            object.modelMatrixCached = std::nullopt;
        }
        // ======================== Reassign ========================
        {
            const auto boundingBucketsBefore = *boundingBoundsBeforeIt;
            const auto boundingBucketsAfter = boundingBucketsForObject(object);
            *boundingBoundsBeforeIt = boundingBucketsAfter; // for use during next iteration
            ++boundingBoundsBeforeIt;
            if (boundingBucketsBefore != boundingBucketsAfter) {
                const auto superset = boundingBucketsSum(boundingBucketsBefore, boundingBucketsAfter);
                for (Bucket::Coord x = superset.first.x; x <= superset.second.x; x++) {
                    for (Bucket::Coord y = superset.first.y; y <= superset.second.y; y++) {
                        for (Bucket::Coord z = superset.first.z; z <= superset.second.z; z++) {
                            const Bucket b{ .x = x, .y = y, .z = z };
                            const bool before = b.isInsideBoundingBuckets(boundingBucketsBefore);
                            const bool after  = b.isInsideBoundingBuckets(boundingBucketsAfter);
                            if (before && !after) { // remove from this bucket
                                removeObjectWithIdFromBucket(object.id, b);
                            } else if (!before && after) { // add to this bucket
                                objectsInBuckets[b].push_back(object.id);
                            }
                        }
                    }
                }
            }
        }
    }

    const auto mark2 = std::chrono::high_resolution_clock::now();
    checkForAndHandleCollisions();
    const auto mark3 = std::chrono::high_resolution_clock::now();

    static int stepTimer = 0;
    if (false && ++stepTimer == 100) {
        stepTimer = 0;
        const auto step1Mks = std::chrono::duration_cast<std::chrono::microseconds>(mark2 - mark1).count();
        const auto step2Mks = std::chrono::duration_cast<std::chrono::microseconds>(mark3 - mark2).count();
        std::cout << "Step 1 = " << static_cast<float>(step1Mks) << " mks\n";
        std::cout << "Step 2 = " << static_cast<float>(step2Mks) << " mks\n";
    }
}


void HmlPhysics::checkForAndHandleCollisions() noexcept {
    assert(adjustments.empty()); // because it has been exhausted during adjustment application traversal

    const auto traverseBucket = [this](
            std::span<const Object::Id> ids,
            Adjustments& adjustments) {
        for (size_t i = 0; i < ids.size(); i++) {
            const auto& obj1 = objects[objectIndexFromId[ids[i]]];

            // For each object, test it against all other objects in current bucket
            for (size_t j = i + 1; j < ids.size(); j++) {
                const auto& obj2 = objects[objectIndexFromId[ids[j]]];
                if (obj1.isStationary() && obj2.isStationary()) continue;

                { // To prevent processing a pair if it has been processed already
                    ObjectAdjustment fakeAdj1;
                    fakeAdj1.id      = obj1.id;
                    fakeAdj1.idOther = obj2.id;
                    ObjectAdjustment fakeAdj2;
                    fakeAdj2.id      = obj2.id;
                    fakeAdj2.idOther = obj1.id;
                    assert(((obj1.isStationary() || obj2.isStationary()) || // at least one static, or else
                        (!(adjustments.contains(fakeAdj1) ^ adjustments.contains(fakeAdj2)))) && // either both present or both not present
                        "Not mirrored adjustments for dynamic objects");
                    if (adjustments.contains(fakeAdj1) || adjustments.contains(fakeAdj2)) continue;
                }

                const auto [adj1, adj2] = process(obj1, obj2);
                adjustments.insert(adj1);
                adjustments.insert(adj2);
            }
        }
    };

    if (hasHelperThreads()) {
        // Get rid of empty buckets to improve traversal speed
        for (auto it = objectsInBuckets.begin(); it != objectsInBuckets.end();) {
            const auto& [_bucket, objects] = *it;
            if (objects.empty()) {
                it = objectsInBuckets.erase(it);
                continue;
            }
            ++it;
        }

        // Divide the work between threads and launch them
        const size_t poolSize = threadPool.size();
        const auto count = objectsInBuckets.size();
        const auto chunk = count / threadPool.size();
        std::vector<std::future<Adjustments>> results(poolSize);
        for (size_t threadIndex = 0; threadIndex < poolSize; threadIndex++) {
            const bool lastThread = threadIndex + 1 == poolSize;
            const auto startIt = std::next(objectsInBuckets.cbegin(), threadIndex * chunk);
            const auto endIt = lastThread ? objectsInBuckets.cend() : std::next(objectsInBuckets.cbegin(), (threadIndex + 1) * chunk);
            results[threadIndex] = threadPool.push([this, startIt, endIt, &traverseBucket](int){
                Adjustments adjustments;
                for (auto it = startIt; it != endIt; ++it) {
                    const auto& [_bucket, ids] = *it;
                    traverseBucket(ids, adjustments);
                }
                return adjustments;
            });
        }

        // Wait on results and collect sub-adjustments
        for (size_t threadIndex = 0; threadIndex < poolSize; threadIndex++) {
            adjustments.merge(results[threadIndex].get());
        }
    } else {
        for (auto it = objectsInBuckets.begin(); it != objectsInBuckets.end();) {
            const auto& [_bucket, ids] = *it;
            if (ids.empty()) {
                it = objectsInBuckets.erase(it);
                continue;
            }

            traverseBucket(ids, adjustments);
            ++it;
        }
    }
}
// ============================================================================
// =================== Register/remove/get ====================================
// ============================================================================
void HmlPhysics::internalRegisterObject(const Object& object) noexcept {
    const auto bounds = boundingBucketsForObject(object);
    for (Bucket::Coord x = bounds.first.x; x <= bounds.second.x; x++) {
        for (Bucket::Coord y = bounds.first.y; y <= bounds.second.y; y++) {
            for (Bucket::Coord z = bounds.first.z; z <= bounds.second.z; z++) {
                const Bucket bucket{ .x = x, .y = y, .z = z };
                objectsInBuckets[bucket].push_back(object.id);
            }
        }
    }

    if (!object.isStationary()) allBoundingBucketsBefore.push_back(bounds);
    objectIndexFromId[object.id] = objects.size();
    objects.push_back(object);
}


HmlPhysics::Object::Id HmlPhysics::registerObject(Object&& object) noexcept {
    assert(object.id == Object::INVALID_ID && "Trying to register an object with an already-set id");

    // NOTE Is probably thread-safe because we only do this (interaction with IDs)
    // from the main thread and never from the internal thread
    const auto id = Object::generateId();
    object.id = id;

    if (hasSelfThread()) {
        // NOTE It is unlikely that internal thread will be using objectsToRegister
        // for long, so we don't mind waiting a little (if at all) to acquire the lock.
        const std::lock_guard<std::mutex> lock(threadedData.objectsToRegisterMutex);
        threadedData.objectsToRegister.push_back(std::move(object));
        threadedData.hasNewObjectsToRegister.store(true);
        threadedData.hasNewObjectsToRegister.notify_one();
    } else {
        internalRegisterObject(std::move(object));
    }

    return id;
}


void HmlPhysics::removeObjectWithIdFromBucket(Object::Id id, const Bucket& bucket) noexcept {
    auto& idsInBucket = objectsInBuckets[bucket];
    const auto size = idsInBucket.size();
    for (size_t i = 0; i < size; i++) {
        if (idsInBucket[i] == id) {
            // NOTE we don't check if this is the same object to remove the unnesessary if
            // NOTE swap, but with fewer steps because we'll remove the current object anyway
            idsInBucket[i] = idsInBucket[size - 1];
            idsInBucket.pop_back();
            return;
        }
    }
    std::cout << size << '\n';
    std::cout << bucket.x << " " << bucket.y << " " << bucket.z << std::endl;
    const auto& object = objects[objectIndexFromId[id]];
    std::cout << object.position << std::endl;
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


std::vector<std::pair<HmlPhysics::Object::Id, glm::mat4>> HmlPhysics::getModelMatrices() noexcept {
    if (hasSelfThread()) {
        const std::lock_guard<std::mutex> lock(threadedData.modelMatricesMutex);
        const std::vector<std::pair<Object::Id, glm::mat4>> copy = threadedData.modelMatrices;
        return copy;
    } else {
        std::vector<std::pair<Object::Id, glm::mat4>> modelMatrices;
        modelMatrices.reserve(objects.size());
        for (const auto& object : objects) modelMatrices.push_back(std::make_pair(object.id, object.modelMatrix()));
        return modelMatrices;
    }

}


void HmlPhysics::setGravity(const glm::vec3& newGravity) noexcept {
    gravity = newGravity;
}
// ============================================================================
// =================== BoundingBuckets ========================================
// ============================================================================
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
    for (const auto& [bucket, ids] : objectsInBuckets) {
        const auto size = ids.size();
        countOfBucketsWithSize[size]++;
    }

    uint32_t min = std::numeric_limits<uint32_t>::max();
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


std::optional<HmlPhysics::ThreadedStats> HmlPhysics::getThreadedStats() const noexcept {
    if (!hasSelfThread()) return std::nullopt;

    return { ThreadedStats {
        .substeps = threadedData.substeps
    }};
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

void HmlPhysics::Object::Box::toPointsAt(float* ptr) const noexcept {
    const auto& [i, j, k] = orientationDataUnnormalized();
    size_t indexP = 0;
    for (float a = -1; a <= 1; a += 2) {
        for (float b = -1; b <= 1; b += 2) {
            for (float c = -1; c <= 1; c += 2) {
                const auto p = (a * i + b * j + c * k) + center;
                ptr[3 * indexP + 0] = p.x;
                ptr[3 * indexP + 1] = p.y;
                ptr[3 * indexP + 2] = p.z;
                indexP++;
            }
        }
    }
    assert(indexP == 8);
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
// ============================================================================
// ======================== Box ===============================================
// ============================================================================
void HmlPhysics::findContactPointsBoxes(
        std::span<const glm::vec3> psFrom,
        std::span<const glm::vec3> psOnto,
        const Object::Box::OrientationData& orientationDataOnto,
        std::vector<glm::vec3>& contactPoints) noexcept {
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
}


void HmlPhysics::findContactPointsBoxesAvxCompact(
        std::span<const glm::vec3> psFrom,
        std::span<const glm::vec3> psOnto,
        const Object::Box::OrientationData& orientationDataOnto,
        std::vector<glm::vec3>& contactPoints) noexcept {
    const int c = 3;
    const int DC = c*0; // don't care
    // Input
    alignas(32) static const __m256i indicesK = _mm256_set_epi32(c*0, c*1, c*2, c*3, c*5, c*6, DC, DC);
    alignas(32) static const __m256i indicesL = _mm256_set_epi32(c*2, c*0, c*3, c*1, c*7, c*7, DC, DC);
    alignas(32) static const __m256i indicesM = _mm256_set_epi32(c*4, c*5, c*6, c*7, c*4, c*4, DC, DC);
    alignas(32) const hml::vec3_256 edgeSetK = hml::gather_from_vec3(static_cast<const float*>(&psFrom[0].x), indicesK);
    alignas(32) const hml::vec3_256 edgeSetL = hml::gather_from_vec3(static_cast<const float*>(&psFrom[0].x), indicesL);
    alignas(32) const hml::vec3_256 edgeSetM = hml::gather_from_vec3(static_cast<const float*>(&psFrom[0].x), indicesM);
    alignas(32) static const __m256i indicesA = _mm256_set_epi32(c*0, c*4, c*0, c*2, c*0, c*1, DC, DC);
    alignas(32) static const __m256i indicesB = _mm256_set_epi32(c*1, c*5, c*1, c*3, c*2, c*3, DC, DC);
    alignas(32) static const __m256i indicesC = _mm256_set_epi32(c*2, c*6, c*4, c*6, c*4, c*5, DC, DC);
    alignas(32) static const __m256i indicesDir = _mm256_set_epi32(c*0, c*0, c*1, c*1, c*2, c*2, DC, DC);
    alignas(32) hml::vec3_256 planePointA = hml::gather_from_vec3(static_cast<const float*>(&psOnto[0].x), indicesA);
    alignas(32) hml::vec3_256 planePointB = hml::gather_from_vec3(static_cast<const float*>(&psOnto[0].x), indicesB);
    alignas(32) hml::vec3_256 planePointC = hml::gather_from_vec3(static_cast<const float*>(&psOnto[0].x), indicesC);
    alignas(32) hml::vec3_256 planeDir = hml::gather_from_vec3(static_cast<const float*>(&orientationDataOnto.i.x), indicesDir);
    // Output
    alignas(32) bool foundIntersection1[8];
    alignas(32) float I_xs1[8];
    alignas(32) float I_ys1[8];
    alignas(32) float I_zs1[8];
    alignas(32) bool foundIntersection2[8];
    alignas(32) float I_xs2[8];
    alignas(32) float I_ys2[8];
    alignas(32) float I_zs2[8];

    for (size_t f = 0; f < 6; f++) {
        edgeFaceIntersection6Comp(edgeSetK, edgeSetL,
                planePointA, planePointB, planePointC, planeDir,
                foundIntersection1 + 0, I_xs1 + 0, I_ys1 + 0, I_zs1 + 0);
        edgeFaceIntersection6Comp(edgeSetK, edgeSetM,
                planePointA, planePointB, planePointC, planeDir,
                foundIntersection2, I_xs2, I_ys2, I_zs2);
        // Rotate faces
        if (f < 5) {
            planePointA = rotl_6(planePointA);
            planePointB = rotl_6(planePointB);
            planePointC = rotl_6(planePointC);
            planeDir    = rotl_6(planeDir);
        }
        // Process result
        for (size_t e = 2; e < 8; e++) {
            const glm::vec3 p{ I_xs1[e], I_ys1[e], I_zs1[e] };
            if (foundIntersection1[e]) contactPoints.emplace_back(p);
        }
        for (size_t e = 2; e < 8; e++) {
            const glm::vec3 p{ I_xs2[e], I_ys2[e], I_zs2[e] };
            if (foundIntersection2[e]) contactPoints.emplace_back(p);
        }
    }
}


void HmlPhysics::findContactPointsBoxesAvxFastest(
        const std::array<glm::vec3, 2 * 8>& psPacked, // for A and B tightly packed
        const std::array<Object::Box::OrientationData, 2>& orientationDataPacked, // for A and B tightly packed
        std::vector<glm::vec3>& contactPoints) noexcept {
    // Output
    constexpr size_t TOTAL = 2 * 6 * 12; // both ways * faces * edges
    alignas(32) float foundIntersection[TOTAL];
    alignas(32) float I_xs[TOTAL];
    alignas(32) float I_ys[TOTAL];
    alignas(32) float I_zs[TOTAL];

    static const std::array<std::array<size_t, 4>, 6> faces{
        std::array<size_t, 4>{0,1,2,3}, std::array<size_t, 4>{4,5,6,7},
        std::array<size_t, 4>{0,1,4,5}, std::array<size_t, 4>{2,3,6,7},
        std::array<size_t, 4>{0,2,4,6}, std::array<size_t, 4>{1,3,5,7}
    };

    const int c = 3; // floats in vec3
    // Edges
    alignas(32) static const __m256i indicesA1 = _mm256_set_epi32(c*0, c*2, c*0, c*1, c*4, c*6, c*4, c*5);
    alignas(32) static const __m256i indicesB1 = _mm256_set_epi32(c*1, c*3, c*2, c*3, c*5, c*7, c*6, c*7);
    alignas(32) static const __m256i indicesA2 = _mm256_set_epi32(c*0, c*1, c*2, c*3, c*(8+0), c*(8+1), c*(8+2), c*(8+3));
    alignas(32) static const __m256i indicesB2 = _mm256_set_epi32(c*4, c*5, c*6, c*7, c*(8+4), c*(8+5), c*(8+6), c*(8+7));
    alignas(32) const hml::vec3_256 edgePointA1 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesA1);
    alignas(32) const hml::vec3_256 edgePointB1 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesB1);
    alignas(32) const hml::vec3_256 edgePointA2 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesA2);
    alignas(32) const hml::vec3_256 edgePointB2 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesB2);
    alignas(32) const hml::vec3_256 edgePointA3 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[8].x), indicesA1);
    alignas(32) const hml::vec3_256 edgePointB3 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[8].x), indicesB1);

    for (size_t f = 0; f < 6; f++) {
        // Faces
        const auto [iA, iB, iC, _iD] = faces[f];
        alignas(32) const __m256i indicesA1 = _mm256_set_epi32(c*(8+iA), c*(8+iA), c*(8+iA), c*(8+iA), c*(8+iA), c*(8+iA), c*(8+iA), c*(8+iA)); // all from obj2
        alignas(32) const __m256i indicesA2 = _mm256_set_epi32(c*(8+iA), c*(8+iA), c*(8+iA), c*(8+iA), c*(0+iA), c*(0+iA), c*(0+iA), c*(0+iA)); // half obj1, half obj2
        alignas(32) const __m256i indicesA3 = _mm256_set_epi32(c*(0+iA), c*(0+iA), c*(0+iA), c*(0+iA), c*(0+iA), c*(0+iA), c*(0+iA), c*(0+iA)); // all from obj1
        alignas(32) const __m256i indicesB1 = _mm256_set_epi32(c*(8+iB), c*(8+iB), c*(8+iB), c*(8+iB), c*(8+iB), c*(8+iB), c*(8+iB), c*(8+iB));
        alignas(32) const __m256i indicesB2 = _mm256_set_epi32(c*(8+iB), c*(8+iB), c*(8+iB), c*(8+iB), c*(0+iB), c*(0+iB), c*(0+iB), c*(0+iB));
        alignas(32) const __m256i indicesB3 = _mm256_set_epi32(c*(0+iB), c*(0+iB), c*(0+iB), c*(0+iB), c*(0+iB), c*(0+iB), c*(0+iB), c*(0+iB));
        alignas(32) const __m256i indicesC1 = _mm256_set_epi32(c*(8+iC), c*(8+iC), c*(8+iC), c*(8+iC), c*(8+iC), c*(8+iC), c*(8+iC), c*(8+iC));
        alignas(32) const __m256i indicesC2 = _mm256_set_epi32(c*(8+iC), c*(8+iC), c*(8+iC), c*(8+iC), c*(0+iC), c*(0+iC), c*(0+iC), c*(0+iC));
        alignas(32) const __m256i indicesC3 = _mm256_set_epi32(c*(0+iC), c*(0+iC), c*(0+iC), c*(0+iC), c*(0+iC), c*(0+iC), c*(0+iC), c*(0+iC));
        alignas(32) const hml::vec3_256 planePointA1 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesA1);
        alignas(32) const hml::vec3_256 planePointA2 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesA2);
        alignas(32) const hml::vec3_256 planePointA3 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesA3);
        alignas(32) const hml::vec3_256 planePointB1 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesB1);
        alignas(32) const hml::vec3_256 planePointB2 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesB2);
        alignas(32) const hml::vec3_256 planePointB3 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesB3);
        alignas(32) const hml::vec3_256 planePointC1 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesC1);
        alignas(32) const hml::vec3_256 planePointC2 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesC2);
        alignas(32) const hml::vec3_256 planePointC3 = hml::gather_from_vec3(static_cast<const float*>(&psPacked[0].x), indicesC3);
        alignas(32) static const __m256i indicesDir1 = _mm256_set_epi32(c*(3), c*(3), c*(3), c*(3), c*(3), c*(3), c*(3), c*(3));
        alignas(32) static const __m256i indicesDir2 = _mm256_set_epi32(c*(3), c*(3), c*(3), c*(3), c*(0), c*(0), c*(0), c*(0));
        alignas(32) static const __m256i indicesDir3 = _mm256_set_epi32(c*(0), c*(0), c*(0), c*(0), c*(0), c*(0), c*(0), c*(0));
        alignas(32) hml::vec3_256 planeDir1;
        alignas(32) hml::vec3_256 planeDir2;
        alignas(32) hml::vec3_256 planeDir3;
        switch (f / 2) {
            case 0:
                planeDir1 = hml::gather_from_vec3(static_cast<const float*>(&orientationDataPacked[0].i.x), indicesDir1);
                planeDir2 = hml::gather_from_vec3(static_cast<const float*>(&orientationDataPacked[0].i.x), indicesDir2);
                planeDir3 = hml::gather_from_vec3(static_cast<const float*>(&orientationDataPacked[0].i.x), indicesDir3);
                break;
            case 1:
                planeDir1 = hml::gather_from_vec3(static_cast<const float*>(&orientationDataPacked[0].j.x), indicesDir1);
                planeDir2 = hml::gather_from_vec3(static_cast<const float*>(&orientationDataPacked[0].j.x), indicesDir2);
                planeDir3 = hml::gather_from_vec3(static_cast<const float*>(&orientationDataPacked[0].j.x), indicesDir3);
                break;
            case 2:
                planeDir1 = hml::gather_from_vec3(static_cast<const float*>(&orientationDataPacked[0].k.x), indicesDir1);
                planeDir2 = hml::gather_from_vec3(static_cast<const float*>(&orientationDataPacked[0].k.x), indicesDir2);
                planeDir3 = hml::gather_from_vec3(static_cast<const float*>(&orientationDataPacked[0].k.x), indicesDir3);
                break;
            default: assert(false);
        }

        const size_t S = 24 * f;
        edgeFaceIntersection8vec3(edgePointA1, edgePointB1,
            planePointA1, planePointB1, planePointC1, planeDir1,
            foundIntersection + S + 0, I_xs + S + 0, I_ys + S + 0, I_zs + S + 0);
        edgeFaceIntersection8vec3(edgePointA2, edgePointB2,
            planePointA2, planePointB2, planePointC2, planeDir2,
            foundIntersection + S + 8, I_xs + S + 8, I_ys + S + 8, I_zs + S + 8);
        edgeFaceIntersection8vec3(edgePointA3, edgePointB3,
            planePointA3, planePointB3, planePointC3, planeDir3,
            foundIntersection + S + 16, I_xs + S + 16, I_ys + S + 16, I_zs + S + 16);
    }

    // Process result
    for (size_t e = 0; e < TOTAL; e++) {
        if (foundIntersection[e]) contactPoints.emplace_back(I_xs[e], I_ys[e], I_zs[e]);
    }
}


void HmlPhysics::edgeFaceIntersection6Comp(
        const hml::vec3_256& edgePointA,
        const hml::vec3_256& edgePointB,
        const hml::vec3_256& planePointA,
        const hml::vec3_256& planePointB,
        const hml::vec3_256& planePointC,
        const hml::vec3_256& planeDir,
        bool foundIntersection_ptr[8],
        float I_xs_ptr[8],
        float I_ys_ptr[8],
        float I_zs_ptr[8]
        ) noexcept {
    alignas(32) const auto edgeDir = normalize(edgePointB - edgePointA);
    alignas(32) const auto dot = hml::dot(edgeDir, planeDir);
    alignas(32) const auto tPs = hml::dot(planePointA, planeDir);
    alignas(32) const auto tAs = hml::dot(edgePointA, planeDir);
    alignas(32) const auto tBs = hml::dot(edgePointB, planeDir);
    alignas(32) const auto ts = (tPs - tAs) / dot;
    alignas(32) const auto I = edgePointA + edgeDir * hml::vec3_256(ts);

    alignas(32) const auto AB = planePointB - planePointA;
    alignas(32) const auto AC = planePointC - planePointA;
    alignas(32) const auto AP = I           - planePointA;
    alignas(32) const auto projAB = hml::dot(AP, AB);
    alignas(32) const auto projAC = hml::dot(AP, AC);
    alignas(32) const auto maxAB = hml::dot(AB, AB);
    alignas(32) const auto maxAC = hml::dot(AC, AC);

    alignas(32) static const __m256 ZERO = _mm256_setzero_ps();
    // Test if !=0 than good
    __m256 valid = _mm256_cmp_ps(dot, ZERO, _CMP_NEQ_UQ);
    // Test if (tp - t1) * (t2 - tp) >= 0 than good
    valid = _mm256_and_ps(valid, _mm256_cmp_ps(_mm256_mul_ps(_mm256_sub_ps(tPs, tAs), _mm256_sub_ps(tBs, tPs)), ZERO, _CMP_GE_OQ));
    // Test if inside the rectangle than good
    valid = _mm256_and_ps(valid, _mm256_cmp_ps(projAB, ZERO, _CMP_GE_OQ));
    valid = _mm256_and_ps(valid, _mm256_cmp_ps(projAB, maxAB, _CMP_LE_OQ));
    valid = _mm256_and_ps(valid, _mm256_cmp_ps(projAC, ZERO, _CMP_GE_OQ));
    valid = _mm256_and_ps(valid, _mm256_cmp_ps(projAC, maxAC, _CMP_LE_OQ));

    // Prepare and store result
    I.store(I_xs_ptr, I_ys_ptr, I_zs_ptr);
    alignas(32) float valid_ptr[8];
    _mm256_store_ps(valid_ptr, valid);
    foundIntersection_ptr[0] = valid_ptr[0] != 0.0f;
    foundIntersection_ptr[1] = valid_ptr[1] != 0.0f;
    foundIntersection_ptr[2] = valid_ptr[2] != 0.0f;
    foundIntersection_ptr[3] = valid_ptr[3] != 0.0f;
    foundIntersection_ptr[4] = valid_ptr[4] != 0.0f;
    foundIntersection_ptr[5] = valid_ptr[5] != 0.0f;
    foundIntersection_ptr[6] = valid_ptr[6] != 0.0f;
    foundIntersection_ptr[7] = valid_ptr[7] != 0.0f;
}


void HmlPhysics::edgeFaceIntersection8vec3(
        const hml::vec3_256& edgePointA,
        const hml::vec3_256& edgePointB,
        const hml::vec3_256& planePointA,
        const hml::vec3_256& planePointB,
        const hml::vec3_256& planePointC,
        const hml::vec3_256& planeDir,
        float foundIntersection_ptr[8],
        float I_xs_ptr[8],
        float I_ys_ptr[8],
        float I_zs_ptr[8]
        ) noexcept {
    alignas(32) const auto edgeDir = normalize(edgePointB - edgePointA);
    alignas(32) const auto dot = hml::dot(edgeDir, planeDir);
    alignas(32) const auto tPs = hml::dot(planePointA, planeDir);
    alignas(32) const auto tAs = hml::dot(edgePointA, planeDir);
    alignas(32) const auto tBs = hml::dot(edgePointB, planeDir);
    alignas(32) const auto ts = (tPs - tAs) / dot;
    alignas(32) const auto I = edgePointA + edgeDir * hml::vec3_256(ts);

    alignas(32) const auto AB = planePointB - planePointA;
    alignas(32) const auto AC = planePointC - planePointA;
    alignas(32) const auto AP = I           - planePointA;
    alignas(32) const auto projAB = hml::dot(AP, AB);
    alignas(32) const auto projAC = hml::dot(AP, AC);
    alignas(32) const auto maxAB = hml::dot(AB, AB);
    alignas(32) const auto maxAC = hml::dot(AC, AC);

    alignas(32) static const __m256 ZERO = _mm256_setzero_ps();
    // Test if !=0 than good
    __m256 valid = _mm256_cmp_ps(dot, ZERO, _CMP_NEQ_UQ);
    // Test if (tp - t1) * (t2 - tp) >= 0 than good
    valid = _mm256_and_ps(valid, _mm256_cmp_ps(_mm256_mul_ps(_mm256_sub_ps(tPs, tAs), _mm256_sub_ps(tBs, tPs)), ZERO, _CMP_GE_OQ));
    // Test if inside the rectangle than good
    valid = _mm256_and_ps(valid, _mm256_cmp_ps(projAB, ZERO, _CMP_GE_OQ));
    valid = _mm256_and_ps(valid, _mm256_cmp_ps(projAB, maxAB, _CMP_LE_OQ));
    valid = _mm256_and_ps(valid, _mm256_cmp_ps(projAC, ZERO, _CMP_GE_OQ));
    valid = _mm256_and_ps(valid, _mm256_cmp_ps(projAC, maxAC, _CMP_LE_OQ));

    // Store result
    I.store(I_xs_ptr, I_ys_ptr, I_zs_ptr);
    hml::store(foundIntersection_ptr, valid);
}
