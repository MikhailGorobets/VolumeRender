#pragma once


#include <Hawk/Math/Functions.hpp>
#include <Hawk/Math/Transform.hpp>
#include <Hawk/Math/Converters.hpp>
#include <Hawk/Math/Geometry.hpp>

namespace Hawk {
	namespace Components {

		class Camera {
		public:
			static constexpr Math::Vec3 LocalForward = { 0.0f, 0.0f,-1.0f };

			static constexpr Math::Vec3 LocalUp      = { 0.0f, 1.0f, 0.0f };

			static constexpr Math::Vec3 LocalRight   = { 1.0f, 0.0f, 0.0f };

			constexpr Camera() noexcept;

			constexpr auto Translate(Math::Vec3 const& v)            noexcept->Camera&;

			constexpr auto Translate(F32 x, F32 y, F32 z)            noexcept->Camera&;

			constexpr auto Rotate(Math::Quat const& q)               noexcept->Camera&;

			constexpr auto Rotate(Math::Vec3 const &axis, F32 angle) noexcept->Camera&;

			constexpr auto Rotate(F32 x, F32 y, F32 z, F32 angle)    noexcept->Camera&;

			constexpr auto SetTranslation(Math::Vec3 const &v)            noexcept->void;

			constexpr auto SetTranslation(F32 x, F32 y, F32 z)            noexcept->void;

			constexpr auto SetRotation(Math::Quat const &q)               noexcept->void;

			constexpr auto SetRotation(Math::Vec3 const& axis, F32 angle) noexcept->void;

			constexpr auto SetRotation(F32 angle, F32 x, F32 y, F32 z)    noexcept->void;
		
			constexpr auto Translation()    const noexcept->Math::Vec3    const&;

			constexpr auto Rotation()       const noexcept->Math::Quat    const&;

			constexpr auto ToMatrix()             noexcept->Math::Mat4x4  const&;

			constexpr auto Forward() const noexcept->Math::Vec3;

			constexpr auto Right()   const noexcept->Math::Vec3;

			constexpr auto Up()      const noexcept->Math::Vec3;

		private:
			bool          m_Dirty;
			Math::Vec3    m_Translation;
			Math::Quat    m_Rotation;
			Math::Mat4x4  m_World;
		};
	}
}

namespace Hawk {
	namespace Components {
		ILINE constexpr Camera::Camera() noexcept : m_Dirty(true), m_Rotation{ 0.0f, 0.0f, 0.0f, 1.0f }, m_Translation{ 0.0f, 0.0f, 0.0f }, m_World{ 1.0f } {}

		ILINE constexpr auto Camera::Translate(Math::Vec3 const & v) noexcept -> Camera & {
			m_Dirty = true;
			m_Translation += v;
			return *this;
		}

		ILINE constexpr auto Camera::Translate(F32 x, F32 y, F32 z) noexcept -> Camera & {
			return this->Translate(Math::Vec3{ x, y, z });
		}

		ILINE constexpr auto Camera::Rotate(Math::Quat const & q) noexcept -> Camera & {
			m_Dirty = true;
			m_Rotation = q * m_Rotation;
			m_Rotation = Math::Normalize(m_Rotation);
			return *this;
		}

		ILINE constexpr auto Camera::Rotate(Math::Vec3 const & axis, F32 angle) noexcept -> Camera & {
			return this->Rotate(Math::AxisAngle(axis, angle));
		}

		ILINE constexpr auto Camera::Rotate(F32 x, F32 y, F32 z, F32 angle) noexcept -> Camera & {
			return this->Rotate(Math::AxisAngle(Math::Vec3{ x, y, z }, angle));
		}

		ILINE constexpr auto Camera::SetTranslation(Math::Vec3 const & v) noexcept -> void {
			m_Dirty = true;
			m_Translation = v;
		}

		ILINE constexpr auto Camera::SetTranslation(F32 x, F32 y, F32 z) noexcept -> void {
			this->SetTranslation(Math::Vec3{ x, y, z });
		}

		ILINE constexpr auto Camera::SetRotation(Math::Quat const & q) noexcept -> void {
			m_Dirty = true;
			m_Rotation = q;
		}

		ILINE constexpr auto Camera::SetRotation(Math::Vec3 const & axis, F32 angle) noexcept -> void {
			m_Dirty = true;
			m_Rotation = Math::AxisAngle(axis, angle);
		}

		ILINE constexpr auto Camera::SetRotation(F32 x, F32 y, F32 z, F32 angle) noexcept -> void {
			m_Dirty = true;
			m_Rotation = Math::AxisAngle(Math::Vec3{ x, y, z }, angle);
		}

		[[nodiscard]] ILINE constexpr auto Camera::Translation() const noexcept -> Math::Vec3 const & {
			return m_Translation;
		}

		[[nodiscard]] ILINE constexpr auto Camera::Rotation() const noexcept -> Math::Quat const & {
			return m_Rotation;
		}

		[[nodiscard]] ILINE constexpr auto Camera::ToMatrix() noexcept -> Math::Mat4x4 const & {
			if (m_Dirty) {
				m_Dirty = false;
				m_World = Math::Convert<Math::Quat, Math::Mat4x4>(Math::Conjugate(m_Rotation)) * Math::Translate(-m_Translation);
			}
			return m_World;
		}

		[[nodiscard]] ILINE constexpr auto Camera::Forward() const noexcept -> Math::Vec3 {
			return Math::Rotate(m_Rotation, Camera::LocalForward);
		}

		[[nodiscard]] ILINE constexpr auto Camera::Right() const noexcept -> Math::Vec3 {
			return Math::Rotate(m_Rotation, Camera::LocalRight);
		}

		[[nodiscard]] ILINE constexpr auto Camera::Up() const noexcept -> Math::Vec3 {
			return Math::Rotate(m_Rotation, Camera::LocalUp);
		}
	}
}