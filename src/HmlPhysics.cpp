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


void HmlPhysics::updateForDt(float dt) noexcept {
    const uint8_t SUBSTEPS = 1;
    const float subDt = dt / SUBSTEPS;
    for (uint8_t substep = 0; substep < SUBSTEPS; substep++) {
        // Apply velocity onto position change
        for (auto& obj : objects) {
            if (obj.isStationary()) continue;

            obj.position += obj.dynamicProperties->velocity * subDt;
            obj.position += obj.dynamicProperties->velocity * subDt;
        }

        // Check for and handle collisions
        for (size_t i = 0; i < objects.size(); i++) {
            auto& obj1 = objects[i];
            for (size_t j = i + 1; j < objects.size(); j++) {
                auto& obj2 = objects[j];

                if (obj1.isStationary() && obj2.isStationary()) continue;

                process(obj1, obj2);
            }
        }
    }
}
// ============================================================================
HmlPhysics::Id HmlPhysics::registerObject(Object&& object) noexcept {
    objects.push_back(std::move(object));
    const auto id = objects.size();
    ids.push_back(id);
    return id;
}

HmlPhysics::Object& HmlPhysics::getObject(HmlPhysics::Id id) noexcept {
    for (size_t i = 0; i < objects.size(); i++) {
        if (ids[i] == id) return objects[i];
    }
    assert(false && "::> No Object with provided Id found.");
    return objects[0]; // stub
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
