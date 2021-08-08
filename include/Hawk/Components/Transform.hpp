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
#include <Hawk/Math/Transform.hpp>

namespace Hawk::Components {
    class Transform {
    public:
        constexpr Transform();
        constexpr Transform(Math::Mat4x4 const& m);
        constexpr auto Translate(Math::Vec3 const& v) noexcept->Transform&;
        constexpr auto Translate(F32 x, F32 y, F32 z) noexcept->Transform&;

        constexpr auto Scale(Math::Vec3 const& v) noexcept->Transform&;
        constexpr auto Scale(F32 x, F32 y, F32 z) noexcept->Transform&;
        constexpr auto Scale(F32 factor)          noexcept->Transform&;

        constexpr auto Rotate(Math::Quat const& q)               noexcept->Transform&;
        constexpr auto Rotate(Math::Vec3 const& axis, F32 angle) noexcept->Transform&;
        constexpr auto Rotate(F32 x, F32 y, F32 z, F32 angle)    noexcept->Transform&;

        constexpr auto Grow(Math::Vec3 const& v) noexcept->Transform&;
        constexpr auto Grow(F32 x, F32 y, F32 z) noexcept->Transform&;
        constexpr auto Grow(F32 factor)          noexcept->Transform&;

        constexpr auto SetTranslation(Math::Vec3 const& t) noexcept -> void;
        constexpr auto SetTranslation(F32 x, F32 y, F32 z) noexcept -> void;

        constexpr auto SetScale(Math::Vec3 const& s) noexcept -> void;
        constexpr auto SetScale(F32 x, F32 y, F32 z) noexcept -> void;
        constexpr auto SetScale(F32 facor)           noexcept -> void;

        constexpr auto SetRotation(Math::Quat const& q)               noexcept -> void;
        constexpr auto SetRotation(Math::Vec3 const& axis, F32 angle) noexcept -> void;
        constexpr auto SetRotation(F32 x, F32 y, F32 z, F32 angle)    noexcept -> void;

        constexpr auto SetMatrix(Math::Mat4x4 const& m) noexcept -> void;

        constexpr auto Translation() const noexcept->Math::Vec3   const&;
        constexpr auto Scale()       const noexcept->Math::Vec3   const&;
        constexpr auto Rotation()    const noexcept->Math::Quat   const&;
        constexpr auto ToMatrix()          noexcept->Math::Mat4x4 const&;

    private:
        bool          m_Dirty;
        Math::Vec3    m_Translation;
        Math::Vec3    m_Scale;
        Math::Quat    m_Rotation;
        Math::Mat4x4  m_Model;
    };
}

namespace Hawk::Components {
    ILINE constexpr Transform::Transform()
        : m_Dirty(true)
        , m_Translation(0.0f, 0.0f, 0.0f)
        , m_Scale(1.0f, 1.0f, 1.0f)
        , m_Rotation(0.0f, 0.0f, 0.0f, 1.0f)
        , m_Model(1.0f) {}

    ILINE constexpr Transform::Transform(Math::Mat4x4 const& m) : Transform() {
        m_Dirty = false;
        m_Model = m;
    }

    ILINE constexpr auto Transform::Translate(Math::Vec3 const& v) noexcept -> Transform& {
        m_Dirty = true;
        m_Translation += v;
        return *this;
    }

    ILINE constexpr auto Transform::Translate(F32 x, F32 y, F32 z) noexcept -> Transform& {
        return this->Translate(Math::Vec3{x, y, z});
    }

    ILINE constexpr auto Transform::Scale(Math::Vec3 const& v) noexcept -> Transform& {
        m_Dirty = true;
        m_Scale *= v;
        return *this;
    }

    ILINE constexpr auto Transform::Scale(F32 x, F32 y, F32 z) noexcept -> Transform& {
        return this->Scale(Math::Vec3{x, y, z});
    }

    ILINE constexpr auto Transform::Scale(F32 factor) noexcept -> Transform& {
        return this->Scale(Math::Vec3{factor, factor, factor});
    }

    ILINE constexpr auto Transform::Rotate(Math::Quat const& q) noexcept -> Transform& {
        m_Dirty = true;
        m_Rotation = q * m_Rotation;
        return *this;
    }

    ILINE constexpr auto Transform::Rotate(Math::Vec3 const& axis, F32 angle) noexcept -> Transform& {
        return this->Rotate(Math::AxisAngle(axis, angle));
    }

    ILINE constexpr auto Transform::Rotate(F32 x, F32 y, F32 z, F32 angle) noexcept -> Transform& {
        return this->Rotate(Math::AxisAngle(Math::Vec3{x, y, z}, angle));
    }

    ILINE constexpr auto Transform::Grow(Math::Vec3 const& v) noexcept -> Transform& {
        m_Dirty = true;
        m_Scale += v;
        return *this;
    }

    ILINE constexpr auto Transform::Grow(F32 x, F32 y, F32 z) noexcept -> Transform& {
        return this->Grow(Math::Vec3{x, y, z});
    }

    ILINE constexpr auto Transform::Grow(F32 factor) noexcept -> Transform& {
        return this->Grow(Math::Vec3{factor, factor, factor});
    }

    ILINE constexpr auto Transform::SetTranslation(Math::Vec3 const& t) noexcept -> void {
        m_Dirty = true;
        m_Translation = t;
    }

    ILINE constexpr auto Transform::SetTranslation(F32 x, F32 y, F32 z) noexcept -> void {
        this->SetTranslation(Math::Vec3{x, y, z});
    }

    ILINE constexpr auto Transform::SetScale(Math::Vec3 const& s) noexcept -> void {
        m_Dirty = true;
        m_Scale = s;
    }

    ILINE constexpr auto Transform::SetScale(F32 x, F32 y, F32 z) noexcept -> void {
        this->SetScale(Math::Vec3{x, y, z});
    }

    ILINE constexpr auto Transform::SetScale(F32 factor) noexcept -> void {
        this->SetScale(Math::Vec3{factor, factor, factor});
    }

    ILINE constexpr auto Transform::SetRotation(Math::Quat const& q) noexcept -> void {
        m_Dirty = true;
        m_Rotation = q;
    }

    ILINE constexpr auto Transform::SetRotation(Math::Vec3 const& axis, F32 angle) noexcept -> void {
        m_Dirty = true;
        m_Rotation = Math::AxisAngle(axis, angle);
    }

    ILINE constexpr auto Transform::SetRotation(F32 x, F32 y, F32 z, F32 angle) noexcept -> void {
        m_Dirty = true;
        m_Rotation = Math::AxisAngle(Math::Vec3{x, y, z}, angle);
    }

    ILINE constexpr auto Transform::SetMatrix(Math::Mat4x4 const& mat) noexcept -> void {
        m_Dirty = false;
        m_Model = mat;
    }

    [[nodiscard]] ILINE constexpr auto Transform::Translation() const noexcept -> Math::Vec3 const& {
        return m_Translation;
    }

    [[nodiscard]] ILINE constexpr auto Transform::Scale() const noexcept -> Math::Vec3 const& {
        return m_Scale;
    }

    [[nodiscard]] ILINE constexpr auto Transform::Rotation() const noexcept -> Math::Quat const& {
        return m_Rotation;
    }

    [[nodiscard]] ILINE constexpr auto Transform::ToMatrix() noexcept -> Math::Mat4x4 const& {

        if (m_Dirty) {
            m_Dirty = false;
            m_Model = Math::Translate(m_Translation) * Math::Scale(m_Scale) * Math::Convert<Math::Quat, Math::Mat4x4>(m_Rotation);
        }
        return m_Model;
    }
}