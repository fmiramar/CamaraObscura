// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Common.hpp"

#include <cmath>

namespace creativereverb {

struct Vec3 {
    double x { 0.0 };
    double y { 0.0 };
    double z { 0.0 };
};

inline double distance(Vec3 first, Vec3 second) noexcept
{
    const double x = first.x - second.x;
    const double y = first.y - second.y;
    const double z = first.z - second.z;
    return std::sqrt(x * x + y * y + z * z);
}

inline Vec3 firstOrderImage(
    Vec3 source, Vec3 room, int wall) noexcept
{
    switch (wall) {
    case 0: source.x = -source.x; break;
    case 1: source.x = 2.0 * room.x - source.x; break;
    case 2: source.y = -source.y; break;
    case 3: source.y = 2.0 * room.y - source.y; break;
    case 4: source.z = -source.z; break;
    default:
        source.z = 2.0 * room.z - source.z;
        break;
    }
    return source;
}

struct GeometryPath {
    float delaySeconds { 0.f };
    float gain { 0.f };
    float pan { 0.f };
    float cutoff { 12000.f };
    int generation { 1 };
};

inline GeometryPath shoeboxFirstOrderPath(
    Vec3 source, Vec3 listener, Vec3 room,
    int wall, double absorption,
    double speedOfSound = 343.0) noexcept
{
    const Vec3 image = firstOrderImage(source, room, wall);
    const double pathDistance =
        std::max(distance(image, listener), 0.01);
    const double reflection =
        std::sqrt(clampValue(
            1.0 - absorption, 0.0, 1.0));
    const double directionX =
        (image.x - listener.x) / pathDistance;
    return {
        static_cast<float>(pathDistance / speedOfSound),
        static_cast<float>(reflection / pathDistance),
        static_cast<float>(clampValue(directionX, -1.0, 1.0)),
        static_cast<float>(
            1000.0 + 17000.0 * reflection),
        1
    };
}

} // namespace creativereverb
