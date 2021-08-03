#pragma once

#include <Hawk/Common/Defines.hpp>

namespace Hawk {

	template<typename T, U32 N>
	struct INumberArray {
		constexpr auto operator[](U32 index) const noexcept->T const&;
		constexpr auto operator[](U32 index)       noexcept->T&;

		constexpr auto begin() const noexcept->const T*;
		constexpr auto begin()       noexcept->T*;

		constexpr auto end()   const noexcept->const T*;
		constexpr auto end()         noexcept->T*;

		constexpr auto data()  const noexcept->const T*;
		constexpr auto data()	     noexcept->T*;
	};
		

}

namespace Hawk {

	
	template<typename T, U32 N>
	[[nodiscard]] ILINE constexpr auto INumberArray<T, N>::operator[](U32 index) const noexcept -> T const & {
		assert(index < N);
		return this->begin()[index];
	}

	template<typename T, U32 N>
	[[nodiscard]] ILINE constexpr auto INumberArray<T, N>::operator[](U32 index) noexcept -> T & {
		assert(index < N);
		return this->begin()[index];
	}

	template<typename T, U32 N>
	[[nodiscard]] ILINE constexpr auto INumberArray<T, N>::begin() const noexcept -> const T * {
		return reinterpret_cast<const T*>(this);
	}

	template<typename T, U32 N>
	[[nodiscard]] ILINE constexpr auto INumberArray<T, N>::begin() noexcept -> T * {
		return reinterpret_cast<T*>(this);
	}

	template<typename T, U32 N>
	[[nodiscard]] ILINE constexpr auto INumberArray<T, N>::end() const noexcept -> const T * {
		return this->begin() + N;
	}

	template<typename T, U32 N>
	[[nodiscard]] ILINE constexpr auto INumberArray<T, N>::end() noexcept -> T * {
		return this->begin() + N;
	}

	template<typename T, U32 N>
	[[nodiscard]] ILINE constexpr auto INumberArray<T, N>::data() const noexcept -> const T * {
		return reinterpret_cast<T*>(this);
	}

	template<typename T, U32 N>
	[[nodiscard]] ILINE constexpr auto INumberArray<T, N>::data() noexcept -> T * {
		return reinterpret_cast<T*>(this);
	}
		
	
}
