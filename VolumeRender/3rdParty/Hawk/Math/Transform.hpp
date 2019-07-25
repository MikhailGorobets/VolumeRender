#pragma once

#include <Hawk/Math/Functions.hpp>

namespace Hawk {
	namespace Math {

		template<typename T> constexpr auto Rotate(Quaternion<T> const& q, Vec3_tpl<T> const& v) noexcept->Vec3_tpl<T>;
		template<typename T> constexpr auto RotateX(T angle) noexcept->Mat4x4_tpl<T>;
		template<typename T> constexpr auto RotateY(T angle) noexcept->Mat4x4_tpl<T>;
		template<typename T> constexpr auto RotateZ(T angle) noexcept->Mat4x4_tpl<T>;
		template<typename T> constexpr auto Translate(Vec3_tpl<T> const& v)  noexcept->Mat4x4_tpl<T>;
		template<typename T> constexpr auto Translate(T x, T y, T z) noexcept->Mat4x4_tpl<T>;
		template<typename T> constexpr auto Scale(Vec3_tpl<T> const& v)  noexcept->Mat4x4_tpl<T>;
		template<typename T> constexpr auto Scale(T v) noexcept->Mat4x4_tpl<T>;
		template<typename T> constexpr auto Scale(T x, T y, T z) noexcept->Mat4x4_tpl<T>;
		template<typename T> constexpr auto Orthographic(T left, T right, T bottom, T top, T zNear, T zFar) noexcept->Mat4x4_tpl<T>;
		template<typename T> constexpr auto Orthographic(T width, T height, T zNear, T zFar) noexcept->Mat4x4_tpl<T>;
		template<typename T> constexpr auto LookAt(Vec3_tpl<T> const& position, Vec3_tpl<T> const& center, Vec3_tpl<T> const& up) noexcept->Mat4x4_tpl<T>;
		template<typename T> constexpr auto AxisAngle(Vec3_tpl<T> const& v, T alpha)  noexcept->Quaternion<T>;
		
	}
}


namespace Hawk {
	namespace Math {


		template<typename T> 
		[[nodiscard]] ILINE constexpr auto Rotate(Quaternion<T> const& q, Vec3_tpl<T> const& v) noexcept->Vec3_tpl<T> {
			auto const u = Vec3_tpl<T>(q.x, q.y, q.z);
			auto const s = q.w;
			return T(2) * Math::Dot(u, v) * u + (s * s - Math::Dot(u, u)) * v + T(2) * s * Math::Cross(u, v);
		}

		template<typename T>
		[[nodiscard]] ILINE constexpr auto RotateX(T angle) noexcept -> Mat4x4_tpl<T> {
			auto const c = Math::Cos(angle);
			auto const s = Math::Sin(angle);
			auto mat = Mat4x4_tpl<T>{ T{ 1 } };
			mat(1, 1) = c;
			mat(1, 2) =-s;
			mat(2, 1) = s;
			mat(2, 2) = c;			 
			return mat;

		}

		template<typename T>
		[[nodiscard]] ILINE constexpr auto RotateY(T angle) noexcept -> Mat4x4_tpl<T> {
			auto const c = Math::Cos(angle);
			auto const s = Math::Sin(angle);
			auto mat = Mat4x4_tpl<T>(T(1));
			mat(0, 0) = c;
			mat(0, 2) = s;
			mat(2, 0) =-s;
			mat(2, 2) = c;
			return mat;

		}

		template<typename T>
		[[nodiscard]] ILINE constexpr auto RotateZ(T angle) noexcept -> Mat4x4_tpl<T> {
			auto c = Math::Cos(angle);
			auto s = Math::Sin(angle);
			auto mat = Mat4x4_tpl<T>(T(1));
			mat(0, 0) = c;
			mat(0, 1) =-s;
			mat(1, 0) = s;
			mat(1, 1) = c;
			return mat;
		}

		template<typename T>
		[[nodiscard]] ILINE constexpr auto Translate(Vec3_tpl<T> const& v) noexcept -> Mat4x4_tpl<T> {

			auto mat = Mat4x4_tpl<T>(T(1));
			mat(0, 3) = v.x;
			mat(1, 3) = v.y;
			mat(2, 3) = v.z;
			return mat;
		}

		template<typename T>
		[[nodiscard]] ILINE constexpr auto Translate(T x, T y, T z) noexcept -> Mat4x4_tpl<T> {
			return Math::Translate({x, y, z});
		}

		template<typename T>
		[[nodiscard]] ILINE constexpr auto Scale(Vec3_tpl<T> const & v) noexcept -> Mat4x4_tpl<T> {
			auto mat = Mat4x4_tpl<T>(T(1));
			mat(0, 0) = v.x;
			mat(1, 1) = v.y;
			mat(2, 2) = v.z;
			return mat;
		}

		template<typename T>
		[[nodiscard]] ILINE constexpr auto Scale(T x, T y, T z) noexcept -> Mat4x4_tpl<T> {
			return Math::Scale(Math::Vec3(x, y, z));
		}

		template<typename T>
		[[nodiscard]] ILINE constexpr auto Scale(T v) noexcept -> Mat4x4_tpl<T> {
			return Math::Scale(v, v, v);
		}

		
		template<typename T>
		[[nodiscard]] ILINE constexpr auto Perspective(T fov, T aspect, T zNear, T zFar) noexcept -> Mat4x4_tpl<T> {
			auto const ctg = T(1) / Math::Tan(fov / T(2));
			auto mat = Mat4x4_tpl<T>(T(1));
			mat(0, 0) = ctg / aspect;
			mat(1, 1) = ctg;

		//	mat(2, 2) = zFar / (zFar - zNear);
		//	mat(2, 3) = -zNear * zFar / (zFar - zNear);
		//	mat(3, 2) = T(1);

			mat(2, 2) = zFar / (zNear - zFar);
			mat(2, 3) = zNear * zFar / (zNear - zFar);
			mat(3, 2) =-T(1);
			mat(3, 3) = T(0);
			return mat;
		}


		template<typename T>
		[[nodiscard]] ILINE constexpr auto Orthographic(T left, T right, T bottom, T top, T zNear, T zFar) noexcept -> Mat4x4_tpl<T> {

			auto mat = Mat4x4_tpl<T>{ T{ 1 } };
			mat(0, 0) = T(2) / (right - left);
			mat(1, 1) = T(2) / (top - bottom);
			mat(2, 2) = T(1) / (zFar - zNear);

			mat(0, 3) = (left + right) / (left - right);
			mat(1, 3) = (top + bottom) / (bottom - top);
			mat(2, 3) = (zNear) / (zNear - zFar);
			return mat;
		}

		template<typename T>
		[[nodiscard]] ILINE constexpr auto Orthographic(T width, T height, T zNear, T zFar) noexcept -> Mat4x4_tpl<T> {
			auto mat = Mat4x4_tpl<T>{ T{ 1 } };
			mat(0, 0) = T(2) / width;
			mat(1, 1) = T(2) / height;
			mat(2, 2) = T(1) / (zNear - zFar);
			mat(2, 3) = (zNear) / (zNear - zFar);
			return mat;

		}


		template<typename T>
		[[nodiscard]] ILINE constexpr auto LookAt(Vec3_tpl<T> const & position, Vec3_tpl<T> const & center, Vec3_tpl<T> const & up) noexcept -> Mat4x4_tpl<T> {
			auto const z = Math::Normalize(center - position);
			auto const x = Math::Normalize(Cross(up, z));
			auto const y = Math::Cross(z, x);

			return Mat4x4_tpl<T>{
				Math::Vec4(x, Math::Dot(x, position)),
				Math::Vec4(y, Math::Dot(y, position)),
				Math::Vec4(z, Math::Dot(z, position)),
				Math::Vec4(T(0), T(0), T(0), T(1))};

		}

		template<typename T>
		[[nodiscard]] constexpr auto AxisAngle(Vec3_tpl<T> const& v, T alpha) noexcept -> Quaternion<T> {
			return Quaternion<T>(Math::Sin(T{ 0.5 } *alpha) * Math::Normalize(v), Cos(T{ 0.5 } *alpha));
		}


	}
}
