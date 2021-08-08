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
    [[nodiscard]] ILINE constexpr auto INumberArray<T, N>::operator[](U32 index) const noexcept -> T const& {
        assert(index < N);
        return this->begin()[index];
    }

    template<typename T, U32 N>
    [[nodiscard]] ILINE constexpr auto INumberArray<T, N>::operator[](U32 index) noexcept -> T& {
        assert(index < N);
        return this->begin()[index];
    }

    template<typename T, U32 N>
    [[nodiscard]] ILINE constexpr auto INumberArray<T, N>::begin() const noexcept -> const T* {
        return reinterpret_cast<const T*>(this);
    }

    template<typename T, U32 N>
    [[nodiscard]] ILINE constexpr auto INumberArray<T, N>::begin() noexcept -> T* {
        return reinterpret_cast<T*>(this);
    }

    template<typename T, U32 N>
    [[nodiscard]] ILINE constexpr auto INumberArray<T, N>::end() const noexcept -> const T* {
        return this->begin() + N;
    }

    template<typename T, U32 N>
    [[nodiscard]] ILINE constexpr auto INumberArray<T, N>::end() noexcept -> T* {
        return this->begin() + N;
    }

    template<typename T, U32 N>
    [[nodiscard]] ILINE constexpr auto INumberArray<T, N>::data() const noexcept -> const T* {
        return reinterpret_cast<T*>(this);
    }

    template<typename T, U32 N>
    [[nodiscard]] ILINE constexpr auto INumberArray<T, N>::data() noexcept -> T* {
        return reinterpret_cast<T*>(this);
    }
}
