#pragma once

#include <Hawk/Math/Functions.hpp>
#include <Hawk/Math/Geometry.hpp>

namespace Hawk {

	namespace Math {


		template<typename Src, typename Dst> constexpr auto Convert(Src const& value) noexcept->Dst;
		template<> constexpr auto Convert<Quat, Mat4x4>(Quat const& q)   noexcept->Mat4x4;
		template<> constexpr auto Convert<Quat, Mat3x3>(Quat const& q)   noexcept->Mat3x3;
		template<> constexpr auto Convert<Plane, Vec4>(Plane const& p)   noexcept->Vec4;
		template<> constexpr auto Convert<Sphere, Vec4>(Sphere const& p) noexcept->Vec4;
	}

}


namespace Hawk {
	namespace Math {

		template<>
		[[nodiscard]] ILINE constexpr auto Convert<Quat, Mat4x4>(Quat const& q) noexcept -> Mat4x4 {

			auto mat = Mat4x4(1.0f);

			auto const dxw = 2.0f * q.x * q.w;
			auto const dyw = 2.0f * q.y * q.w;
			auto const dzw = 2.0f * q.z * q.w;

			auto const dxy = 2.0f * q.x * q.y;
			auto const dxz = 2.0f * q.x * q.z;
			auto const dyz = 2.0f * q.y * q.z;

			auto const dxx = 2.0f * q.x * q.x;
			auto const dyy = 2.0f * q.y * q.y;
			auto const dzz = 2.0f * q.z * q.z;

			mat(0, 0) = 1.0f - dyy - dzz;
			mat(0, 1) = dxy - dzw;
			mat(0, 2) = dxz + dyw;

			mat(1, 0) = dxy + dzw;
			mat(1, 1) = 1.0f - dxx - dzz;
			mat(1, 2) = dyz - dxw;

			mat(2, 0) = dxz - dyw;
			mat(2, 1) = dyz + dxw;
			mat(2, 2) = 1.0f - dxx - dyy;
			return mat;

		}

		template<>
		[[nodiscard]] ILINE constexpr auto Convert<Quat, Mat3x3>(Quat const& q) noexcept -> Mat3x3 {

			auto mat = Mat3x3(1.0f);

			auto const dxw = 2.0f * q.x * q.w;
			auto const dyw = 2.0f * q.y * q.w;
			auto const dzw = 2.0f * q.z * q.w;

			auto const dxy = 2.0f * q.x * q.y;
			auto const dxz = 2.0f * q.x * q.z;
			auto const dyz = 2.0f * q.y * q.z;

			auto const dxx = 2.0f * q.x * q.x;
			auto const dyy = 2.0f * q.y * q.y;
			auto const dzz = 2.0f * q.z * q.z;

			mat(0, 0) = 1.0f - dyy - dzz;
			mat(0, 1) = dxy - dzw;
			mat(0, 2) = dxz + dyw;

			mat(1, 0) = dxy + dzw;
			mat(1, 1) = 1.0f - dxx - dzz;
			mat(1, 2) = dyz - dxw;

			mat(2, 0) = dxz - dyw;
			mat(2, 1) = dyz + dxw;
			mat(2, 2) = 1.0f - dxx - dyy;
			return mat;

		}


		template<> 
		[[nodiscard]] ILINE constexpr auto Convert<Plane, Vec4>(Plane const& p) noexcept -> Vec4 {
			return Vec4(p.Normal, p.Offset);
		}


	}

}