/*
 * MIT License
 *
 * Copyright(c) 2021 Mikhail Gorobets
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this softwareand associated documentation files(the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions :
 *
 * The above copyright noticeand this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#pragma once
#include <Hawk/Math/Functions.hpp>
#include <array>

namespace Hawk::Math {
    class Plane {
    public:
        constexpr Plane() = default;
        constexpr Plane(Math::Vec3 const& normal, F32 offset);
        constexpr Plane(Math::Vec4 const& v);
        constexpr Plane(F32 a, F32 b, F32 c, F32 d);
        constexpr Plane(Math::Vec3 const& a, Math::Vec3 const& b, Math::Vec3 const& c);
    public:
        Math::Vec3 Normal = Math::Vec3(0.0f, 1.0f, 0.0f);
        F32        Offset = 0.0f;
    };

    class Ray {
    public:
        constexpr Ray() = default;
        constexpr Ray(Math::Vec3 position, Math::Vec3 direction);
    public:
        Math::Vec3 Position = Math::Vec3(0.0f, 0.0f, 0.0f);
        Math::Vec3 Direction = Math::Vec3(0.0f, 1.0f, 0.0f);

    };

    class Sphere {
    public:
        constexpr Sphere() = default;
        constexpr Sphere(Math::Vec3 const& center, F32 radius) noexcept;
        constexpr Sphere(F32 x, F32 y, F32 z, F32 radius)      noexcept;
    public:
        Math::Vec3 Center = Math::Vec3(0.0f, 0.0f, 0.0f);
        F32        Radius = 1.0f;
    };

    class Box {
    public:
        constexpr Box() = default;
        constexpr Box(Math::Vec3 const& min, Math::Vec3 const& max) noexcept;
    public:
        Math::Vec3 Min = Math::Vec3(-0.5f, -0.5f, -0.5f);
        Math::Vec3 Max = Math::Vec3(+0.5f, +0.5f, +0.5f);

    };

    class Frustum {
    public:
        enum Section { Near, Far, Left, Right, Top, Bottom };
    public:
        constexpr Frustum() = default;
        constexpr Frustum(Math::Mat4x4 const& proj) noexcept;
        constexpr auto SetPlane(Frustum::Section section, Math::Plane const& plane) noexcept -> void;
        constexpr auto Plane(Frustum::Section section)   const noexcept -> Math::Plane const&;

    private:
        constexpr auto GenerateFromPerspective(F32 hTan, F32 vTan, F32 zNear, F32 zFar)                        noexcept -> void;
        constexpr auto GenerateFromOrthographic(F32 left, F32 right, F32 top, F32 bottom, F32 zNear, F32 zFar) noexcept -> void;
    private:
        Math::Plane m_Planes[6];
    };

    constexpr auto operator*(Mat4x4 const& lhs, Plane const& rhs)   noexcept -> Plane;
    constexpr auto operator*(Mat4x4 const& lhs, Ray const& rhs)     noexcept -> Ray;
    constexpr auto operator*(Mat4x4 const& lhs, Sphere const& rhs)  noexcept -> Sphere;
    constexpr auto operator*(Mat4x4 const& lhs, Box const& rhs)     noexcept -> Box;
    constexpr auto operator*(Mat4x4 const& lhs, Frustum const& rhs) noexcept -> Frustum;

    constexpr auto Intersects(Frustum const& lhs, Sphere const& rhs) noexcept -> bool;
    constexpr auto Intersects(Frustum const& lhs, Box    const& rhs) noexcept -> bool;
}

namespace Hawk::Math {

    ILINE constexpr Plane::Plane(Math::Vec3 const& normal, F32 offset) : Normal(normal), Offset(offset) {}

    ILINE constexpr Plane::Plane(Math::Vec4 const& v) : Normal(v.x, v.y, v.z), Offset(v.w) {}

    ILINE constexpr Plane::Plane(F32 a, F32 b, F32 c, F32 d) : Plane(Math::Vec3(a, b, c), d) {}

    ILINE constexpr Plane::Plane(Math::Vec3 const& a, Math::Vec3 const& b, Math::Vec3 const& c) {
        Normal = Math::Normalize(Math::Cross(b - a, c - a));
        Offset = -Math::Dot(Normal, a);
    }

    ILINE constexpr Ray::Ray(Math::Vec3 position, Math::Vec3 direction) : Position(position), Direction(direction) {}

    ILINE constexpr Sphere::Sphere(Math::Vec3 const& center, F32 radius) noexcept : Center(center), Radius(radius) {}

    ILINE constexpr Sphere::Sphere(F32 x, F32 y, F32 z, F32 radius) noexcept : Center(x, y, z), Radius(radius) {}

    ILINE constexpr Box::Box(Math::Vec3 const& min, Math::Vec3 const& max) noexcept : Min(min), Max(max) {}

    ILINE constexpr Frustum::Frustum(Math::Mat4x4 const& proj) noexcept {
        auto const rcpXX = 1.0f / proj(0, 0);
        auto const rcpYY = 1.0f / proj(1, 1);
        auto const rcpZZ = 1.0f / proj(2, 2);

        if (proj(3, 0) == 0.0f && proj(3, 1) == 0.0f && proj(3, 2) == 0.0f && proj(3, 3) == 1.0f) {
            auto const left = (-1.0f - proj(0, 3)) * rcpXX;
            auto const right = (1.0f - proj(0, 3)) * rcpXX;
            auto const top = (1.0f - proj(1, 3)) * rcpYY;
            auto const bottom = (-1.0f - proj(1, 3)) * rcpYY;
            auto const front = (0.0f - proj(2, 3)) * rcpZZ;
            auto const back = (1.0f - proj(2, 3)) * rcpZZ;

            if (front < back)
                GenerateFromOrthographic(left, right, top, bottom, front, back);
            else
                GenerateFromOrthographic(left, right, top, bottom, back, front);
        } else {

            if (rcpZZ > 0.0f) {
                auto const farClip = proj(2, 3) * rcpZZ;
                auto const nearClip = farClip / (rcpZZ + 1.0f);
                GenerateFromPerspective(rcpXX, rcpXX, farClip, nearClip);
            } else {
                auto const nearClip = proj(2, 3) * rcpZZ;
                auto const farClip = nearClip / (rcpZZ + 1.0f);
                GenerateFromPerspective(rcpXX, rcpXX, farClip, nearClip);
            }
        }
    }

    ILINE constexpr auto Frustum::SetPlane(Frustum::Section section, Math::Plane const& plane) noexcept -> void {
        m_Planes[static_cast<uint8_t>(section)] = plane;
    }

    [[nodiscard]] ILINE constexpr auto Frustum::Plane(Frustum::Section section) const noexcept -> Math::Plane const& {
        return m_Planes[static_cast<uint8_t>(section)];
    }


    [[nodiscard]] ILINE constexpr auto Intersects(Frustum const& lhs, Sphere const& rhs) noexcept -> bool {

        for (auto index = 0; index < 6; index++) {
            auto const& e = lhs.Plane(static_cast<Math::Frustum::Section>(index));
            if (Math::Dot(e.Normal, rhs.Center) + e.Offset <= -rhs.Radius)
                return false;
        }

        return true;
    }

    [[nodiscard]] ILINE constexpr auto Intersects(Frustum const& lhs, Box const& rhs)  noexcept -> bool {

        for (auto index = 0; index < 6; index++) {
            auto const& e = lhs.Plane(static_cast<Math::Frustum::Section>(index));
            auto const d = Math::Max(rhs.Min.x * e.Normal.x, rhs.Max.x * e.Normal.x) +
                Math::Max(rhs.Min.y * e.Normal.y, rhs.Max.y * e.Normal.y) +
                Math::Max(rhs.Min.z * e.Normal.z, rhs.Max.z * e.Normal.z) + e.Offset;
            if (d < 0)
                return false;
        }
        return true;
    }

    ILINE constexpr auto Frustum::GenerateFromPerspective(F32 hTan, F32 vTan, F32 zNear, F32 zFar) noexcept -> void {
        auto const nearX = hTan * zNear;
        auto const nearY = vTan * zNear;
        auto const farX = hTan * zFar;
        auto const farY = vTan * zFar;


        auto const nHx = 1.0f / Math::Sqrt(1.0f + hTan * hTan);
        auto const nHz = -nHx * hTan;
        auto const nVy = 1.0f / Math::Sqrt(1.0f + vTan * vTan);
        auto const nVz = -nVy * vTan;

        m_Planes[Section::Near] = Math::Plane(0.0f, 0.0f, -1.0f, -zNear);
        m_Planes[Section::Far] = Math::Plane(0.0f, 0.0f, 1.0f, zFar);
        m_Planes[Section::Left] = Math::Plane(nHx, 0.0f, nHz, 0.0f);
        m_Planes[Section::Right] = Math::Plane(-nHx, 0.0f, nHz, 0.0f);
        m_Planes[Section::Top] = Math::Plane(0.0f, -nVy, nVz, 0.0f);
        m_Planes[Section::Bottom] = Math::Plane(0.0f, nVy, nVz, 0.0f);
    }

    ILINE constexpr auto Frustum::GenerateFromOrthographic(F32 left, F32 right, F32 top, F32 bottom, F32 front, F32 back) noexcept -> void {
        m_Planes[Section::Near] = Math::Plane(+0.0f, +0.0f, -1.0f, -front);
        m_Planes[Section::Far] = Math::Plane(+0.0f, +0.0f, +1.0f, +back);
        m_Planes[Section::Left] = Math::Plane(+1.0f, +0.0f, +0.0f, -left);
        m_Planes[Section::Right] = Math::Plane(-1.0f, +0.0f, +0.0f, +right);
        m_Planes[Section::Top] = Math::Plane(+0.0f, -1.0f, +0.0f, +bottom);
        m_Planes[Section::Bottom] = Math::Plane(+0.0f, +1.0f, +0.0f, -top);

    }

    ILINE constexpr auto Hawk::Math::operator*(Mat4x4 const& lhs, Plane const& rhs) noexcept -> Plane {
        return Math::Transpose(Math::Inverse(lhs)) * Math::Vec4(rhs.Normal, rhs.Offset);
    }

    ILINE constexpr auto operator*(Mat4x4 const& lhs, Ray const& rhs) noexcept -> Ray {
        return Ray();
    }

    ILINE constexpr auto operator*(Mat4x4 const& lhs, Sphere const& rhs) noexcept -> Sphere {
        auto const center = lhs * Hawk::Math::Vec4(rhs.Center, 1.0f);
        return Sphere(center.x, center.y, center.z, rhs.Radius);
    }

    ILINE constexpr auto operator*(Mat4x4 const& lhs, Box const& rhs) noexcept -> Box {
        auto const min = lhs * Hawk::Math::Vec4(rhs.Min, 1.0f);
        auto const max = lhs * Hawk::Math::Vec4(rhs.Max, 1.0f);
        return Box(Math::Vec3(min.x, min.y, min.z), Math::Vec3(max.x, max.y, max.z));
    }

    ILINE constexpr auto operator*(Mat4x4 const& lhs, Frustum const& rhs) noexcept -> Frustum {

        auto result = Frustum{};
        auto const trans = Math::Transpose(Math::Inverse(lhs));
        for (auto index = 0; index < 6; index++) {
            auto const& e = rhs.Plane(static_cast<Math::Frustum::Section>(index));
            result.SetPlane(static_cast<Math::Frustum::Section>(index), Math::Plane(trans * Math::Vec4(e.Normal, e.Offset)));
        }
        return result;
    }
}