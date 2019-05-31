#pragma once
#include <Hawk/Common/INumberArray.hpp>

namespace Hawk {
	namespace Math {
		namespace Detail {

			template<typename T, U32 N>
			class Vector final : public INumberArray<T, N> {
				static_assert(std::is_same<T, F32>() || std::is_same<T, F64>() || std::is_same<T, I32>() || std::is_same<T, U32>(), "Invalid scalar type for Vector");
			public:
				constexpr Vector() noexcept = default;
				constexpr Vector(T v) noexcept;
				template <typename... Args> constexpr Vector(typename std::enable_if<sizeof...(Args) + 1 == N, T>::type const& head, Args... tail) noexcept;
			private:
				T m_Data[N];
			};


			template<typename T>
			class Vector<T, 2> final : public INumberArray<T, 2> {
				static_assert(std::is_same<T, F32>() || std::is_same<T, F64>() || std::is_same<T, I32>() || std::is_same<T, U32>(), "Invalid scalar type for Vector");
			public:
				union {
					struct { T x, y; };
					struct { T r, g; };
					struct { T v[2]; };
				};
			public:
				constexpr Vector() noexcept = default;
				constexpr Vector(T v) noexcept;
				template <typename... Args> constexpr Vector(typename std::enable_if<sizeof...(Args) + 1 == 2, T>::type const& head, Args... tail) noexcept;

			};

			template<typename T>
			class Vector<T, 3> final : public INumberArray<T, 3> {
				static_assert(std::is_same<T, F32>() || std::is_same<T, F64>() || std::is_same<T, I32>() || std::is_same<T, U32>(), "Invalid scalar type for Vector");
			public:
				union {
					struct { T x, y, z; };
					struct { T r, g, b; };
					struct { T v[3]; };
				};
			public:
				constexpr Vector() noexcept = default;
				constexpr Vector(T v) noexcept;
				constexpr Vector(Vector<T, 2> const& x, T y) noexcept;
				template <typename... Args> constexpr Vector(typename std::enable_if<sizeof...(Args) + 1 == 3, T>::type const& head, Args... tail) noexcept;

			};

			template<typename T>
			class Vector<T, 4> final : public INumberArray<T, 4> {
				static_assert(std::is_same<T, F32>() || std::is_same<T, F64>() || std::is_same<T, I32>() || std::is_same<T, U32>(), "Invalid scalar type for Vector");
			public:
				union {
					struct { T x, y, z, w; };
					struct { T r, g, b, a; };
					struct { T v[4]; };
				};
			public:
				constexpr Vector() noexcept = default;
				constexpr Vector(T v) noexcept;
				constexpr Vector(Vector<T, 3> const& x, T y)  noexcept;
				constexpr Vector(Vector<T, 2> const& x, T y, T z) noexcept;
				template <typename... Args> constexpr Vector(typename std::enable_if<sizeof...(Args) + 1 == 4, T>::type const& head, Args... tail) noexcept;
			};


			template<typename T, U32 N> constexpr auto operator==(Vector<T, N> const& lhs, Vector<T, N> const& rhs) noexcept->bool;
			template<typename T, U32 N> constexpr auto operator!=(Vector<T, N> const& lhs, Vector<T, N> const& rhs) noexcept->bool;
		
			template<typename T, U32 N> constexpr auto operator+(Vector<T, N> const& lhs, Vector<T, N> const& rhs)  noexcept->Vector<T, N>;
			template<typename T, U32 N> constexpr auto operator-(Vector<T, N> const& lhs, Vector<T, N> const& rhs)  noexcept->Vector<T, N>;
			template<typename T, U32 N> constexpr auto operator*(Vector<T, N> const& lhs, Vector<T, N> const& rhs)  noexcept->Vector<T, N>;
			template<typename T, U32 N> constexpr auto operator/(Vector<T, N> const& lhs, Vector<T, N> const& rhs)  noexcept->Vector<T, N>;

			template<typename T, U32 N> constexpr auto operator-(Vector<T, N> const& rhs)        noexcept->Vector<T, N>;
			template<typename T, U32 N> constexpr auto operator*(Vector<T, N> const& lhs, T rhs) noexcept->Vector<T, N>;
			template<typename T, U32 N> constexpr auto operator*(T lhs, Vector<T, N> const& rhs) noexcept->Vector<T, N>;
			template<typename T, U32 N> constexpr auto operator/(Vector<T, N> const& lhs, T rhs) noexcept->Vector<T, N>;

			template<typename T, U32 N> constexpr auto operator+=(Vector<T, N>& lhs, Vector<T, N> const& rhs)  noexcept->Vector<T, N>&;
			template<typename T, U32 N> constexpr auto operator-=(Vector<T, N>& lhs, Vector<T, N> const& rhs)  noexcept->Vector<T, N>&;
			template<typename T, U32 N> constexpr auto operator*=(Vector<T, N>& lhs, Vector<T, N> const& rhs)  noexcept->Vector<T, N>&;
			template<typename T, U32 N> constexpr auto operator/=(Vector<T, N>& lhs, Vector<T, N> const& rhs)  noexcept->Vector<T, N>&;

			template<typename T, U32 N> constexpr auto operator*=(Vector<T, N>& lhs, T rhs)  noexcept->Vector<T, N>&;
			template<typename T, U32 N> constexpr auto operator/=(Vector<T, N>& lhs, T rhs)  noexcept->Vector<T, N>&;

		}
	}
}


namespace Hawk {
	namespace Math {
		namespace Detail {

			template<typename T, U32 N>
			template<typename ...Args>
			ILINE constexpr Vector<T, N>::Vector(typename std::enable_if<sizeof...(Args) + 1 == N, T>::type const& head, Args ...tail) noexcept : m_Data{ head, T{ tail }... } {}

			template<typename T, U32 N>
			ILINE constexpr Vector<T, N>::Vector(T v)  noexcept {
				for (auto& e : *this) e = v;
			}

			template<typename T>
			ILINE constexpr Vector<T, 2>::Vector(T v)   noexcept : x{ v }, y{ v } {}
			

			template<typename T>
			ILINE constexpr Vector<T, 3>::Vector(T v)   noexcept : x{ v }, y{ v }, z{ v } {}
			

			template<typename T>
			ILINE constexpr Vector<T, 3>::Vector(Vector<T, 2> const & x, T y) noexcept : x{ x.x }, y{ x.y }, z{ y } {}

			template<typename T>
			ILINE constexpr Vector<T, 4>::Vector(Vector<T, 3> const & x, T y) noexcept : x{ x.x }, y{ x.y }, z{ x.z }, w{ y } {}

			template<typename T>
			ILINE constexpr Vector<T, 4>::Vector(Vector<T, 2> const & x, T y, T z) noexcept : x{ x.x }, y{ x.y }, z{ y }, w{ z } {}

			template<typename T>
			ILINE constexpr Vector<T, 4>::Vector(T v)  noexcept : x { v }, y{ v }, z{ v }, w{ v }  {}
			
			template<typename T>
			template<typename ...Args>
			ILINE constexpr Vector<T, 2>::Vector(typename std::enable_if<sizeof...(Args) + 1 == 2, T>::type const& head, Args ...tail) noexcept : v{ head,  T{ tail }... } {}

			template<typename T>
			template<typename ...Args>
			ILINE constexpr Vector<T, 3>::Vector(typename std::enable_if<sizeof...(Args) + 1 == 3, T>::type const& head, Args ...tail) noexcept : v{ head,  T{ tail }... } {}

			template<typename T>
			template<typename ...Args>
			ILINE constexpr Vector<T, 4>::Vector(typename std::enable_if<sizeof...(Args) + 1 == 4, T>::type const& head, Args ...tail) noexcept : v{ head,  T{ tail }... } {}

			template<typename T, U32 N>
			[[nodiscard]] ILINE constexpr auto operator==(Vector<T, N> const& lhs, Vector<T, N> const& rhs) noexcept -> bool {
				for (auto index = 0u; index < N; index++)
					if (!(std::abs(lhs[index] - rhs[index]) <= std::numeric_limits<T>::epsilon()))
						return false;
				return true;
			}

			template<typename T, U32 N>
			[[nodiscard]] ILINE constexpr auto operator!=(Vector<T, N> const& lhs, Vector<T, N> const& rhs) noexcept -> bool {
				return !(lhs == rhs);
			}

			template<typename T, U32 N>
			[[nodiscard]] ILINE constexpr auto operator+(Vector<T, N> const& lhs, Vector<T, N> const& rhs) noexcept-> Vector<T, N> {
				auto result = Vector<T, N>{};
				for (auto index = 0u; index < N; index++)
					result[index] = lhs[index] + rhs[index];
				return result;
			}

			template<typename T, U32 N>
			[[nodiscard]] ILINE constexpr auto operator*(Vector<T, N> const & lhs, Vector<T, N> const & rhs) noexcept -> Vector<T, N> {
				auto result = Vector<T, N>{};
				for(auto index = 0; index < N; index++)
					result[index] = lhs[index] * rhs[index];
				return result;
			}

			template<typename T, U32 N>
			[[nodiscard]] ILINE constexpr auto operator/(Vector<T, N> const & lhs, Vector<T, N> const & rhs) noexcept -> Vector<T, N> {
				auto result = Vector<T, N>{};
				for (auto index = 0; index < N; index++)
					result[index] = lhs[index] / rhs[index];
				return result;
			}


			template<typename T, U32 N>
			[[nodiscard]] ILINE constexpr auto operator-(Vector<T, N> const& lhs, Vector<T, N> const& rhs) noexcept-> Vector<T, N> {
				auto result = Vector<T, N>{};
				for (auto index = 0u; index < N; index++)
					result[index] = lhs[index] - rhs[index];
				return result;
			}

			template<typename T, U32 N>
			[[nodiscard]] ILINE constexpr auto operator-(Vector<T, N> const & rhs) noexcept -> Vector<T, N> {
				auto result = Vector<T, N>{};
				for (auto index = 0u; index < N; index++)
					result[index] = -rhs[index];
				return result;
			}

			template<typename T, U32 N>
			[[nodiscard]] ILINE constexpr auto operator*(T lhs, Vector<T, N> const& rhs) noexcept-> Vector<T, N> {
				auto result = Vector<T, N>{};
				for (auto index = 0; index < N; index++)
					result[index] = lhs * rhs[index];
				return result;
			}

			template<typename T, U32 N>
			[[nodiscard]] ILINE constexpr auto operator*(Vector<T, N> const& lhs, T rhs) noexcept-> Vector<T, N> {
				return  rhs * lhs;
			}

			template<typename T, U32 N>
			[[nodiscard]] ILINE constexpr auto operator/(Vector<T, N> const & lhs, T rhs) noexcept -> Vector<T, N> {
				return (T{ 1 } / rhs) * lhs;
			}

			template<typename T, U32 N>
			ILINE constexpr auto operator+=(Vector<T, N>& lhs, Vector<T, N> const & rhs) noexcept -> Vector<T, N>& {
				lhs = lhs + rhs;
				return lhs;
			}

			template<typename T, U32 N>
			ILINE constexpr auto operator-=(Vector<T, N>& lhs, Vector<T, N> const & rhs) noexcept -> Vector<T, N>& {
				lhs = lhs - rhs;
				return lhs;
			}

			template<typename T, U32 N>
			ILINE constexpr auto operator*=(Vector<T, N>& lhs, Vector<T, N> const & rhs) noexcept -> Vector<T, N>& {
				lhs = lhs * rhs;
				return lhs;
			}

			template<typename T, U32 N>
			ILINE constexpr auto operator/=(Vector<T, N>& lhs, Vector<T, N> const & rhs) noexcept -> Vector<T, N>& {
				lhs = lhs / rhs;
				return lhs;
			}

			template<typename T, U32 N>
			ILINE constexpr auto operator*=(Vector<T, N>& lhs, T rhs) noexcept -> Vector<T, N>& {
				lhs = lhs * rhs;
				return lhs;
			}

			template<typename T, U32 N>
			ILINE constexpr auto operator/=(Vector<T, N>& lhs, T rhs) noexcept -> Vector<T, N>& {
				lhs = lhs / rhs;
				return lhs;
			}

		}
	}
}