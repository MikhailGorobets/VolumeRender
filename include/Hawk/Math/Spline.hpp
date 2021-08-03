#pragma once

#include <Hawk/Common/Defines.hpp>

namespace Hawk {

	namespace Math {
		namespace Spline {

			template<typename T> constexpr auto Cubic(T const& v1, T const& v2, T const& v3, T const& v4, typename T::value_type const& s) noexcept->T;
		
		}

	}
}

namespace Hawk {
	namespace Math {
		namespace Spline {

			template<typename T>
			[[nodiscard]] ILINE constexpr auto Cubic(T const& v1, T const& v2, T const& v3, T const& v4, typename T::value_type const& s) noexcept->T {
				return ((v1 * s + v2) * s + v3) * s + v4;
			}

		}
		

}