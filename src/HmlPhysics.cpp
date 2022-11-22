#include "HmlPhysics.h"


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
    static const auto pointsFor = [](const Object::Box& b){
        assert(b.modelMatrixCached);
        const auto& modelMatrix = *(b.modelMatrixCached);
        glm::vec3 i1{ modelMatrix[0][0], modelMatrix[0][1], modelMatrix[0][2] };
        glm::vec3 j1{ modelMatrix[1][0], modelMatrix[1][1], modelMatrix[1][2] };
        glm::vec3 k1{ modelMatrix[2][0], modelMatrix[2][1], modelMatrix[2][2] };

        int index = 0;
        const glm::vec3 center{ modelMatrix [3][0], modelMatrix[3][1], modelMatrix[3][2] };
        std::array<glm::vec3, 8> ps;
        for (float i = -1; i <= 1; i += 2) {
            for (float j = -1; j <= 1; j += 2) {
                for (float k = -1; k <= 1; k += 2) {
                    ps[index++] = (i * i1 + j * j1 + k * k1) + center;
                }
            }
        }
        assert(index == 8);

        return ps;
    };
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
        assert(false && "Unexpected Simplex size in nextSimplex()");
        return false;
    };

    const glm::vec3 initialDir{1, 0, 0};
    const auto ps1 = pointsFor(b1);
    const auto ps2 = pointsFor(b2);
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

        const auto& modelMatrix = *(b1.modelMatrixCached);
        // const glm::vec3 center{ modelMatrix [3][0], modelMatrix[3][1], modelMatrix[3][2] };
        glm::vec3 i1{ modelMatrix[0][0], modelMatrix[0][1], modelMatrix[0][2] };
        glm::vec3 j1{ modelMatrix[1][0], modelMatrix[1][1], modelMatrix[1][2] };
        glm::vec3 k1{ modelMatrix[2][0], modelMatrix[2][1], modelMatrix[2][2] };
        // const glm::vec3 halfDimensions{i1, j1, k1};
            // const auto a = glm::dot(b1.halfDimensions, detOpt->dir);
            const auto a = glm::dot(i1, detOpt->dir) + glm::dot(j1, detOpt->dir) + glm::dot(k1, detOpt->dir);

            // detOpt->contactPoints = { b1.center + (a + detOpt->extent * 0.5f) * detOpt->dir};
    static const auto avg = [](std::span<const glm::vec3> vs){
        glm::vec3 sum{0};
        for (const auto& v : vs) sum += v;
        return sum / static_cast<float>(vs.size());
    };
            detOpt->contactPoints = { avg(std::span(simplex.begin(), simplex.end())) };
            return detOpt;
        }
    }

    return std::nullopt;
}


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
    // const auto rap = glm::vec3{1,0,0};
    // const auto rbp = glm::vec3{-1,0,0};
    const glm::vec3 center1{ obj1.modelMatrix()[3][0], obj1.modelMatrix()[3][1], obj1.modelMatrix()[3][2] };
    const glm::vec3 center2{ obj2.modelMatrix()[3][0], obj2.modelMatrix()[3][1], obj2.modelMatrix()[3][2] };
    const auto rap = avg(contactPoints) - center1;
    const auto rbp = avg(contactPoints) - center2;
    // std::cout << "Avg=" << avg(contactPoints) << '\n';
    // std::cout << "RAP=" << rap << '\n';
    // std::cout << "RBP=" << rbp << '\n';
    // std::cout << "dir=" << dir << '\n';
    const auto a = glm::cross(obj1DP.invRotationalInertiaTensor * glm::cross(rap, dir), dir);
    const auto b = glm::cross(obj2DP.invRotationalInertiaTensor * glm::cross(rbp, dir), dir);
    denom += glm::dot(a + b, dir);
    // std::cout << "+denom=" << glm::dot(a + b, dir) << '\n';
    const float j = nom / denom;
    // const float j = -(1.0f + e) * glm::dot(relativeV, dir) / (obj1DP.invMass + obj2DP.invMass);
    const auto impulse = j * dir;
    if (!obj1.isStationary()) {
        obj1.dynamicProperties->velocity -= impulse * obj1DP.invMass;
        // obj1.dynamicProperties->angularMomentum -= glm::cross(impulse, rap) * obj1DP.invRotationalInertiaTensor;
        obj1.dynamicProperties->angularMomentum -= glm::cross(rap, impulse) * obj1DP.invRotationalInertiaTensor;
        // std::cout << "1=" << glm::cross(impulse, rap) * obj1DP.invRotationalInertiaTensor << '\n';
    }
    if (!obj2.isStationary()) {
        obj2.dynamicProperties->velocity += impulse * obj2DP.invMass;
        // obj2.dynamicProperties->angularMomentum += glm::cross(impulse, rbp) * obj2DP.invRotationalInertiaTensor;
        obj2.dynamicProperties->angularMomentum += glm::cross(rbp, impulse) * obj2DP.invRotationalInertiaTensor;
        // std::cout << "2=" << glm::cross(impulse, rbp) * obj2DP.invRotationalInertiaTensor << '\n';
    }
}


void HmlPhysics::process(Object& obj1, Object& obj2) noexcept {
    std::optional<Detection> detectionOpt = std::nullopt;
    if      (obj1.isSphere() && obj2.isSphere()) detectionOpt = detect(obj1.asSphere(), obj2.asSphere());
    else if (obj1.isBox()    && obj2.isSphere()) detectionOpt = detect(obj1.asBox(),    obj2.asSphere());
    else if (obj1.isSphere() && obj2.isBox())    detectionOpt = detect(obj1.asSphere(), obj2.asBox());
    else if (obj1.isBox()    && obj2.isBox())    detectionOpt = detect(obj1.asBox(),    obj2.asBox());

    // if (obj1.isBox()    && obj2.isBox()) {
    //     const auto resOpt = gjk(obj1.asBox(), obj2.asBox());
    //     if (resOpt) {
    //         std::cout << "---- DETECT ----\n";
    //         std::cout << obj1.asBox().center << " " << obj2.asBox().center << '\n';
    //         std::cout << "dir=" << resOpt->dir << '\n';
    //         std::cout << "extent=" << resOpt->extent << '\n';
    //         std::cout << "CP=" << '\n';
    //         for (const auto& cp : resOpt->contactPoints) std::cout << cp << ";";
    //         // assert(!contactPoints.empty() && "No contact points");
    //         std::cout << "\n=========" << '\n';
    //     }
    //     else std::cout << "-- NOTHING --\n";
    // }

    if (!detectionOpt) return;
    const auto& [dir, extent, _contactPoints] = *detectionOpt;

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

    const glm::vec3 F {0,0,0};
    // const glm::vec3 cp{1,0,0};
    // const glm::vec3 Ft{0,0,0};

    const uint8_t SUBSTEPS = 1;
    const float subDt = dt / SUBSTEPS;
    static int stepTimer = 0;
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
            // const auto r = cp;
            // const auto torque = glm::cross(r, Ft);
            // object->dynamicProperties->angularMomentum += subDt * torque;

            object->orientation = glm::normalize(object->orientation); // TODO not every step

            // Invalidate modelMatrix (force further recalculation)
            object->modelMatrixCached = object->generateModelMatrix();
        }
        const auto mark2 = std::chrono::high_resolution_clock::now();

        // Check for and handle collisions
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
                    process(*obj1, *obj2);
                }
            }

            ++it;
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
// Returns dir from b1 towards b2
std::optional<HmlPhysics::Detection> HmlPhysics::detectOrientedBoxesWithSat(const Object::Box& b1, const Object::Box& b2) noexcept {
    assert(b1.modelMatrixCached);
    assert(b2.modelMatrixCached);
    const auto& modelMatrix1 = *(b1.modelMatrixCached);
    const auto& modelMatrix2 = *(b2.modelMatrixCached);
    glm::vec3 i1{ modelMatrix1[0][0], modelMatrix1[0][1], modelMatrix1[0][2] };
    glm::vec3 j1{ modelMatrix1[1][0], modelMatrix1[1][1], modelMatrix1[1][2] };
    glm::vec3 k1{ modelMatrix1[2][0], modelMatrix1[2][1], modelMatrix1[2][2] };
    glm::vec3 i2{ modelMatrix2[0][0], modelMatrix2[0][1], modelMatrix2[0][2] };
    glm::vec3 j2{ modelMatrix2[1][0], modelMatrix2[1][1], modelMatrix2[1][2] };
    glm::vec3 k2{ modelMatrix2[2][0], modelMatrix2[2][1], modelMatrix2[2][2] };

    int index;
    const glm::vec3 center1{ modelMatrix1[3][0], modelMatrix1[3][1], modelMatrix1[3][2] };
    std::array<glm::vec3, 8> p1;
    index = 0;
    for (float i = -1; i <= 1; i += 2) {
        for (float j = -1; j <= 1; j += 2) {
            for (float k = -1; k <= 1; k += 2) {
                p1[index++] = (i * i1 + j * j1 + k * k1) + center1;
            }
        }
    }
    assert(index == 8);

    const glm::vec3 center2{ modelMatrix2[3][0], modelMatrix2[3][1], modelMatrix2[3][2] };
    std::array<glm::vec3, 8> p2;
    index = 0;
    for (float i = -1; i <= 1; i += 2) {
        for (float j = -1; j <= 1; j += 2) {
            for (float k = -1; k <= 1; k += 2) {
                p2[index++] = (i * i2 + j * j2 + k * k2) + center2;
            }
        }
    }
    assert(index == 8);

    std::vector<glm::vec3> axes = {
        i1, j1, k1, i2, j2, k2,
        // glm::cross(i1, i2), glm::cross(i1, j2), glm::cross(i1, k2),
        // glm::cross(j1, i2), glm::cross(j1, j2), glm::cross(j1, k2),
        // glm::cross(k1, i2), glm::cross(k1, j2), glm::cross(k1, k2)
    };
    for (auto& a : axes) a = glm::normalize(a);
    glm::vec3 dir;
    float minExtent = std::numeric_limits<float>::max();
    std::vector<std::pair<float, float>> minMaxPerAxis;
    for (const auto& axis : axes) {
        float min1 = std::numeric_limits<float>::max();
        float max1 = std::numeric_limits<float>::lowest();
        for (const auto& p : p1) {
            const auto proj = glm::dot(p, axis);
            if (proj < min1) min1 = proj;
            if (proj > max1) max1 = proj;
        }

        float min2 = std::numeric_limits<float>::max();
        float max2 = std::numeric_limits<float>::lowest();
        for (const auto& p : p2) {
            const auto proj = glm::dot(p, axis);
            if (proj < min2) min2 = proj;
            if (proj > max2) max2 = proj;
        }

        if (!(min1 < max2 && min2 < max1)) return std::nullopt;

        const auto extent = std::min(max1 - min2, max2 - min1);
        if (extent < minExtent) {
            minExtent = extent;
            dir = axis;
        }
        const float max = std::min(max1, max2);
        const float min = std::max(min1, min2);
        minMaxPerAxis.push_back(std::make_pair(min, max));
    }

    // Find contact points
    std::vector<glm::vec3> contactPoints;
    for (const auto& p : p1) {
        int index = 0;
        bool all = true;
        for (const auto& axis : axes) {
            const auto proj = glm::dot(p, axis);
            const auto& [min, max] = minMaxPerAxis[index++];
            if (min <= proj && proj <= max) {} else { all = false; break; }
        }
        if (all) contactPoints.push_back(p);
    }
    for (const auto& p : p2) {
        int index = 0;
        bool all = true;
        for (const auto& axis : axes) {
            const auto proj = glm::dot(p, axis);
            const auto& [min, max] = minMaxPerAxis[index++];
            if (min <= proj && proj <= max) {} else { all = false; break; }
        }
        if (all) contactPoints.push_back(p);
    }

    const auto centers = center2 - center1;
    const bool aligned = glm::dot(centers, dir) >= 0.0f;
    if (!aligned) dir *= -1;

std::cout << "======================" << '\n';
std::cout << "DETECT" << '\n';
std::cout << "=========" << '\n';
std::cout << "C1 = " << center1 << " C2= " << center2 << "\n";
std::cout << "=========" << '\n';
std::cout << "P1:" << '\n';
for (const auto& p : p1) {
    std::cout << p << '\n';
}
std::cout << "P2:" << '\n';
for (const auto& p : p2) {
    std::cout << p << '\n';
}
std::cout << "=========" << '\n';
std::cout << "Dir = " << dir << "  MinEx=" << minExtent << '\n';
std::cout << "B1=" << i1 << " " << j1 << " " << k1 << '\n';
std::cout << "B2=" << i2 << " " << j2 << " " << k2 << '\n';
std::cout << "====CP=====" << '\n';
for (const auto& cp : contactPoints) std::cout << cp << ";";
assert(!contactPoints.empty() && "No contact points");
std::cout << "=========" << '\n';
std::cout << std::endl;


    return { Detection{
        .dir = dir,
        .extent = minExtent,
        .contactPoints = contactPoints,
    }};
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
