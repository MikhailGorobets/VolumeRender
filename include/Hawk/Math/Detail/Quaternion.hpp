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

#include <Hawk/Common/INumberArray.hpp>

namespace Hawk::Math::Detail {

    template<typename T>
    class  Quaternion final: public INumberArray<T, 4> {
        static_assert(std::is_same<T, F32>() || std::is_same<T, F64>(), "Invalid scalar type for Quaternion");
    public:
        union {
            struct { T x, y, z, w; };
            struct { T q[4]; };
        };
    public:
        constexpr Quaternion() noexcept = default;
        constexpr Quaternion(Vector<T, 3> const& v, T w) noexcept;
        template <typename... Args> constexpr Quaternion(typename std::enable_if<sizeof...(Args) + 1 == 4, T>::type head, Args... tail) noexcept;

    };

    template<typename T> constexpr auto operator==(Quaternion<T> const& lhs, Quaternion<T> const& rhs) noexcept->bool;
    template<typename T> constexpr auto operator!=(Quaternion<T> const& lhs, Quaternion<T> const& rhs) noexcept->bool;

    template<typename T> constexpr auto operator+(Quaternion<T> const& lhs, Quaternion<T> const& rhs)  noexcept->Quaternion<T>;
    template<typename T> constexpr auto operator-(Quaternion<T> const& lhs, Quaternion<T> const& rhs)  noexcept->Quaternion<T>;
    template<typename T> constexpr auto operator*(Quaternion<T> const& lhs, Quaternion<T> const& rhs) noexcept->Quaternion<T>;

    template<typename T> constexpr auto operator-(Quaternion<T> const& rhs)        noexcept->Quaternion<T>;
    template<typename T> constexpr auto operator*(Quaternion<T> const& lhs, T rhs) noexcept->Quaternion<T>;
    template<typename T> constexpr auto operator*(T lhs, Quaternion<T> const& rhs) noexcept->Quaternion<T>;
    template<typename T> constexpr auto operator/(Quaternion<T> const& lhs, T rhs) noexcept->Quaternion<T>;


    template<typename T> constexpr auto operator+=(Quaternion<T>& lhs, Quaternion<T> const& rhs) noexcept->Quaternion<T>&;
    template<typename T> constexpr auto operator-=(Quaternion<T>& lhs, Quaternion<T> const& rhs) noexcept->Quaternion<T>&;
    template<typename T> constexpr auto operator*=(Quaternion<T>& lhs, Quaternion<T> const& rhs) noexcept->Quaternion<T>&;

    template<typename T> constexpr auto operator*=(Quaternion<T>& lhs, T rhs)  noexcept->Quaternion<T>&;
    template<typename T> constexpr auto operator/=(Quaternion<T>& lhs, T rhs)  noexcept->Quaternion<T>&;
}

namespace Hawk::Math::Detail {
    template<typename T>
    [[nodiscard]] ILINE constexpr auto operator==(Quaternion<T> const& lhs, Quaternion<T> const& rhs) noexcept -> bool {
        for (auto index = 0u; index < 4; index++)
            if (!(std::abs(lhs[index] - rhs[index]) <= std::numeric_limits<T>::epsilon()))
                return false;
        return true;
    }

    template<typename T>
    [[nodiscard]] ILINE constexpr auto operator!=(Quaternion<T> const& lhs, Quaternion<T> const& rhs) noexcept -> bool {
        return !(lhs == rhs);
    }

    template<typename T>
    [[nodiscard]] ILINE constexpr auto operator+(Quaternion<T> const& lhs, Quaternion<T> const& rhs) noexcept -> Quaternion<T> {
        auto result = Quaternion<T>{};
        for (auto index = 0u; index < 4; index++)
            result[index] = lhs[index] + rhs[index];
        return result;
    }

    template<typename T>
    [[nodiscard]] ILINE constexpr auto operator-(Quaternion<T> const& lhs, Quaternion<T> const& rhs) noexcept -> Quaternion<T> {
        auto result = Quaternion<T>{};
        for (auto index = 0u; index < 4; index++)
            result[index] = lhs[index] - rhs[index];
        return result;
    }

    template<typename T>
    [[nodiscard]] ILINE constexpr auto operator*(Quaternion<T> const& lhs, Quaternion<T> const& rhs) noexcept -> Quaternion<T> {
        return Quaternion<T>{ lhs.w* rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.w* rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
            lhs.w* rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
            lhs.w* rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z};
    }

    template<typename T>
    [[nodiscard]] ILINE constexpr auto operator-(Quaternion<T> const& rhs) noexcept -> Quaternion<T> {
        auto result = Quaternion<T>{};
        for (auto index = 0u; index < 4; index++)
            result[index] = -rhs[index];
        return result;
    }

    template<typename T>
    [[nodiscard]] ILINE constexpr auto operator*(Quaternion<T> const& lhs, T rhs) noexcept -> Quaternion<T> {
        return  rhs * lhs;
    }

    template<typename T>
    [[nodiscard]] ILINE  constexpr auto operator*(T lhs, Quaternion<T> const& rhs) noexcept -> Quaternion<T> {
        auto result = Quaternion<T>{};
        for (auto index = 0; index < 4; index++)
            result[index] = lhs * rhs[index];
        return result;
    }

    template<typename T>
    [[nodiscard]] ILINE  constexpr auto operator/(Quaternion<T> const& lhs, T rhs) noexcept -> Quaternion<T> {
        return (T{1} / rhs) * lhs;
    }

    template<typename T>
    ILINE constexpr auto operator+=(Quaternion<T>& lhs, Quaternion<T> const& rhs) noexcept -> Quaternion<T>& {
        lhs = lhs + rhs;
        return lhs;
    }

    template<typename T>
    ILINE constexpr auto operator-=(Quaternion<T>& lhs, Quaternion<T> const& rhs) noexcept -> Quaternion<T>& {
        lhs = lhs - rhs;
        return lhs;
    }

    template<typename T>
    ILINE constexpr auto operator*=(Quaternion<T>& lhs, Quaternion<T> const& rhs) noexcept -> Quaternion<T>& {
        lhs = lhs * rhs;
        return lhs;
    }

    template<typename T>
    ILINE constexpr auto operator*=(Quaternion<T>& lhs, T rhs) noexcept -> Quaternion<T>& {
        lhs = lhs * rhs;
        return lhs;

    }

    template<typename T>
    ILINE constexpr auto operator/=(Quaternion<T>& lhs, T rhs) noexcept -> Quaternion<T>& {
        lhs = lhs / rhs;
        return lhs;
    }


    template<typename T>
    template<typename ...Args>
    ILINE constexpr Quaternion<T>::Quaternion(typename std::enable_if<sizeof...(Args) + 1 == 4, T>::type head, Args ...tail)  noexcept : q{head, T(tail)...} {}

    template<typename T>
    ILINE  constexpr Quaternion<T>::Quaternion(Vector<T, 3> const& v, T a) noexcept : x{v.x}, y{v.y}, z{v.z}, w{a} { }
}