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
    template<typename T, U32 M, U32 N>
    class Matrix final: public INumberArray<T, M* N> {
        static_assert(std::is_same<T, F32>() || std::is_same<T, F64>() || std::is_same<T, I32>() || std::is_same<T, U32>(), "Invalid scalar type for Matrix");
    public:
        union {
            struct { Vector<T, N> v[M]; };
            struct { T m[M * N]; };
        };
    public:
        constexpr Matrix() noexcept = default;
        constexpr Matrix(T v) noexcept;
        constexpr auto operator()(U32 row, U32 column)       noexcept->T&;
        constexpr auto operator()(U32 row, U32 column) const noexcept->T const&;
        template <typename... Args> constexpr Matrix(typename std::enable_if<sizeof...(Args) + 1 == M * N, T>::type const& head, Args... tail) noexcept;
    };

    template<typename T>
    class Matrix<T, 2, 2> final: public INumberArray<T, 2 * 2> {
        static_assert(std::is_same<T, F32>() || std::is_same<T, F64>() || std::is_same<T, I32>() || std::is_same<T, U32>(), "Invalid scalar type for Matrix");
    public:
        union {
            struct {
                T m00, m01;
                T m10, m11;
            };
            struct {
                Vector<T, 2> x;
                Vector<T, 2> y;
            };
            struct { Vector<T, 2> v[2]; };
            struct { T m[2 * 2]; };
        };
    public:
        constexpr Matrix() noexcept = default;
        constexpr Matrix(T v) noexcept;
        constexpr auto operator()(U32 row, U32 column)       noexcept->T&;
        constexpr auto operator()(U32 row, U32 column) const noexcept->T const&;
        template <typename... Args> constexpr Matrix(typename std::enable_if<sizeof...(Args) + 1 == 2 * 2, T>::type const& head, Args... tail)        noexcept;
        template <typename... Args> constexpr Matrix(typename std::enable_if<sizeof...(Args) + 1 == 2, Vector<T, 2>>::type const& head, Args... tail) noexcept;
    };

    template<typename T>
    class Matrix<T, 3, 3> final: public INumberArray<T, 3 * 3> {
        static_assert(std::is_same<T, F32>() || std::is_same<T, F64>() || std::is_same<T, I32>() || std::is_same<T, U32>(), "Invalid scalar type for Matrix");
    public:
        union {
            struct {
                T m00, m01, m02;
                T m10, m11, m12;
                T m20, m21, m22;
            };
            struct {
                Vector<T, 3> x;
                Vector<T, 3> y;
                Vector<T, 3> z;
            };
            struct { Vector<T, 3> v[3]; };
            struct { T m[3 * 3]; };
        };
    public:
        constexpr Matrix() noexcept = default;
        constexpr Matrix(T v) noexcept;
        constexpr auto operator()(U32 row, U32 column)       noexcept->T&;
        constexpr auto operator()(U32 row, U32 column) const noexcept->T const&;

        template <U32 M, U32 N> constexpr Matrix(Matrix<T, M, N> const& v) noexcept;
        template <typename... Args> constexpr Matrix(typename std::enable_if<sizeof...(Args) + 1 == 3 * 3, T>::type const& head, Args... tail) noexcept;
        template <typename... Args> constexpr Matrix(typename std::enable_if<sizeof...(Args) + 1 == 3, Vector<T, 3>>::type const& head, Args... tail) noexcept;
    };

    template<typename T>
    class Matrix<T, 4, 4> final: public INumberArray<T, 4 * 4> {
        static_assert(std::is_same<T, F32>() || std::is_same<T, F64>() || std::is_same<T, I32>() || std::is_same<T, U32>(), "Invalid scalar type for Matrix");
    public:
        union {
            struct {
                T m00, m01, m02, m03;
                T m10, m11, m12, m13;
                T m20, m21, m22, m23;
                T m30, m31, m32, m33;
            };
            struct {
                Vector<T, 4> x;
                Vector<T, 4> y;
                Vector<T, 4> z;
                Vector<T, 4> w;
            };
            struct { Vector<T, 4> v[4]; };
            struct { T m[4 * 4]; };
        };
    public:
        constexpr Matrix() noexcept = default;
        constexpr Matrix(T v) noexcept;
        constexpr auto operator()(U32 row, U32 column)       noexcept->T&;
        constexpr auto operator()(U32 row, U32 column) const noexcept->T const&;

        template <U32 M, U32 N> constexpr Matrix(Matrix<T, M, N> const& v) noexcept;
        template <typename... Args> constexpr Matrix(typename std::enable_if<sizeof...(Args) + 1 == 4 * 4, T>::type const& head, Args... tail) noexcept;
        template <typename... Args> constexpr Matrix(typename std::enable_if<sizeof...(Args) + 1 == 4, Vector<T, 4>>::type const& head, Args... tail) noexcept;
    };

    template<typename T, U32 M, U32 N> constexpr auto operator==(Matrix<T, N, M> const& lhs, Matrix<T, N, M> const& rhs) noexcept->bool;
    template<typename T, U32 M, U32 N> constexpr auto operator!=(Matrix<T, N, M> const& lhs, Matrix<T, N, M> const& rhs) noexcept->bool;

    template<typename T, U32 M, U32 N> constexpr auto operator+(Matrix<T, M, N> const& lhs, Matrix<T, M, N> const& rhs) noexcept->Matrix<T, M, N>;
    template<typename T, U32 M, U32 N> constexpr auto operator-(Matrix<T, M, N> const& lhs, Matrix<T, M, N> const& rhs) noexcept->Matrix<T, M, N>;

    template<typename T, U32 M, U32 N> constexpr auto operator-(Matrix<T, M, N> const& rhs)        noexcept->Matrix<T, M, N>;
    template<typename T, U32 M, U32 N> constexpr auto operator*(T lhs, Matrix<T, M, N> const& rhs) noexcept->Matrix<T, M, N>;
    template<typename T, U32 M, U32 N> constexpr auto operator*(Matrix<T, M, N> const& lhs, T rhs) noexcept->Matrix<T, M, N>;
    template<typename T, U32 M, U32 N> constexpr auto operator/(Matrix<T, M, N> const& lhs, T rhs) noexcept->Matrix<T, M, N>;

    template<typename T, U32 M, U32 N> constexpr auto operator+=(Matrix<T, M, N>& lhs, Matrix<T, M, N> const& rhs)  noexcept->Matrix<T, M, N>&;
    template<typename T, U32 M, U32 N> constexpr auto operator-=(Matrix<T, M, N>& lhs, Matrix<T, M, N> const& rhs)  noexcept->Matrix<T, M, N>&;
    template<typename T, U32 M, U32 N> constexpr auto operator*=(Matrix<T, M, N>& lhs, Matrix<T, M, N> const& rhs)  noexcept->Matrix<T, M, N>&;

    template<typename T, U32 M, U32 N> constexpr auto operator*=(Matrix<T, M, N>& lhs, T rhs) noexcept->Matrix<T, M, N>&;
    template<typename T, U32 M, U32 N> constexpr auto operator/=(Matrix<T, M, N>& lhs, T rhs) noexcept->Matrix<T, M, N>&;

    template<typename T, U32 M, U32 N, U32 P> constexpr auto operator*(Matrix<T, M, N> const& lhs, Matrix<T, N, P> const& rhs) noexcept->Matrix<T, M, P>;
    template<typename T, U32 M, U32 N>        constexpr auto operator*(Matrix<T, M, N> const& lhs, Vector<T, N> const& rhs)    noexcept->Vector<T, M>;
    template<typename T, U32 N>               constexpr auto operator*(Matrix<T, N, N> const& lhs, Matrix<T, N, N> const& rhs) noexcept->Matrix<T, N, N>;
}


namespace Hawk::Math::Detail {


    template<typename T, U32 M, U32 N>
    [[nodiscard]] ILINE constexpr Matrix<T, M, N>::Matrix(T v) noexcept {
        static_assert(M == N);
        for (auto i = 0; i < M; i++)
            for (auto j = 0; j < N; j++)
                this->operator()(i, j) = (i == j) ? T{v} : T{0};
    }

    template<typename T, U32 M, U32 N>
    [[nodiscard]] ILINE constexpr auto Matrix<T, M, N>::operator()(U32 row, U32 column) noexcept -> T& {
        assert(row < M);
        assert(column < N);
        return this->operator[](row* M + column);
    }

    template<typename T, U32 M, U32 N>
    [[nodiscard]] ILINE constexpr auto Matrix<T, M, N>::operator()(U32 row, U32 column) const noexcept -> T const& {
        assert(row < M);
        assert(column < N);
        return  this->operator[](row* M + column);
    }


    template<typename T>
    ILINE constexpr Matrix<T, 2, 2>::Matrix(T v) noexcept {
        for (auto i = 0; i < 2; i++)
            for (auto j = 0; j < 2; j++)
                this->operator()(i, j) = (i == j) ? T{v} : T{0};
    }

    template<typename T>
    [[nodiscard]] ILINE  constexpr auto Matrix<T, 2, 2>::operator()(U32 row, U32 column) noexcept -> T& {
        assert(row < 2);
        assert(column < 2);
        return this->operator[](row * 2 + column);
    }

    template<typename T>
    [[nodiscard]] ILINE constexpr auto Matrix<T, 2, 2>::operator()(U32 row, U32 column) const noexcept -> T const& {
        assert(row < 2);
        assert(column < 2);
        return this->operator[](row * 2 + column);
    }


    template<typename T>
    ILINE constexpr Matrix<T, 3, 3>::Matrix(T v) noexcept {
        for (auto i = 0; i < 3; i++)
            for (auto j = 0; j < 3; j++)
                this->operator()(i, j) = (i == j) ? T{v} : T{0};
    }

    template<typename T>
    [[nodiscard]] ILINE constexpr auto Matrix<T, 3, 3>::operator()(U32 row, U32 column) noexcept -> T& {
        assert(row < 3);
        assert(column < 3);
        return this->operator[](row * 3 + column);
    }

    template<typename T>
    [[nodiscard]] ILINE constexpr auto Matrix<T, 3, 3>::operator()(U32 row, U32 column) const noexcept -> T const& {
        assert(row < 3);
        assert(column < 3);
        return this->operator[](row * 3 + column);
    }

    template<typename T>
    ILINE constexpr Matrix<T, 4, 4>::Matrix(T v) noexcept {
        for (auto i = 0; i < 4; i++)
            for (auto j = 0; j < 4; j++)
                this->operator()(i, j) = (i == j) ? T{v} : T{0};
    }

    template<typename T>
    [[nodiscard]] ILINE constexpr auto Matrix<T, 4, 4>::operator()(U32 row, U32 column) noexcept -> T& {
        assert(row < 4);
        assert(column < 4);
        return this->operator[](row * 4 + column);
    }

    template<typename T>
    [[nodiscard]] ILINE constexpr auto Matrix<T, 4, 4>::operator()(U32 row, U32 column) const noexcept ->  T const& {
        assert(row < 4);
        assert(column < 4);
        return this->operator[](row * 4 + column);
    }

    template<typename T>
    template<U32 M, U32 N>
    ILINE constexpr Matrix<T, 4, 4>::Matrix(Matrix<T, M, N> const& v) noexcept : Matrix(T{1}) {
        for (auto i = 0u; i < (std::min)(4u, M); i++)
            for (auto j = 0u; j < (std::min)(4u, N); j++)
                this->operator()(i, j) = v(i, j);
    }

    template<typename T>
    template<U32 M, U32 N>
    ILINE constexpr Matrix<T, 3, 3>::Matrix(Matrix<T, M, N> const& v) noexcept : Matrix(T{1}) {
        for (auto i = 0u; i < (std::min)(3u, M); i++)
            for (auto j = 0u; j < (std::min)(3u, N); j++)
                this->operator()(i, j) = v(i, j);
    }


    template<typename T, U32 M, U32 N>
    template<typename ...Args>
    ILINE constexpr Matrix<T, M, N>::Matrix(typename std::enable_if<sizeof...(Args) + 1 == M * N, T>::type const& head, Args ...tail) noexcept : m{head,  T{ tail }...} {}

    template<typename T>
    template<typename ...Args>
    ILINE constexpr Matrix<T, 2, 2>::Matrix(typename std::enable_if<sizeof...(Args) + 1 == 2, Vector<T, 2>>::type const& head, Args ...tail) noexcept : v{head,  Vector<T, 2>{ tail }...} {}

    template<typename T>
    template<typename ...Args>
    ILINE constexpr Matrix<T, 3, 3>::Matrix(typename std::enable_if<sizeof...(Args) + 1 == 3, Vector<T, 3>>::type const& head, Args ...tail) noexcept : v{head,  Vector<T, 3>{ tail }...} {}

    template<typename T>
    template<typename ...Args>
    ILINE constexpr Matrix<T, 4, 4>::Matrix(typename std::enable_if<sizeof...(Args) + 1 == 4, Vector<T, 4>>::type const& head, Args ...tail) noexcept : v{head,  Vector<T, 4>{ tail }...} {}

    template<typename T>
    template<typename ...Args>
    ILINE constexpr Matrix<T, 2, 2>::Matrix(typename std::enable_if<sizeof...(Args) + 1 == 2 * 2, T>::type const& head, Args ...tail) noexcept : m{head, T{ tail }...} {}

    template<typename T>
    template<typename ...Args>
    ILINE constexpr Matrix<T, 3, 3>::Matrix(typename std::enable_if<sizeof...(Args) + 1 == 3 * 3, T>::type const& head, Args ...tail) noexcept : m{head, T{ tail }...} {}

    template<typename T>
    template<typename ...Args>
    ILINE constexpr Matrix<T, 4, 4>::Matrix(typename std::enable_if<sizeof...(Args) + 1 == 4 * 4, T>::type const& head, Args ...tail) noexcept : m{head, T{ tail }...} {}

    template<typename T, U32 M, U32 N>
    [[nodiscard]] ILINE constexpr auto operator==(Matrix<T, N, M> const& lhs, Matrix<T, N, M> const& rhs) noexcept -> bool {
        for (auto index = 0u; index < N * M; index++)
            if (!(std::abs(lhs[index] - rhs[index]) <= std::numeric_limits<T>::epsilon()))
                return false;
        return true;
    }

    template<typename T, U32 M, U32 N>
    [[nodiscard]] ILINE constexpr auto operator!=(Matrix<T, N, M> const& lhs, Matrix<T, N, M> const& rhs) noexcept -> bool {
        return !(lhs == rhs);
    }


    template<typename T, U32 M, U32 N>
    [[nodiscard]] ILINE constexpr auto operator+(Matrix<T, M, N> const& lhs, Matrix<T, M, N> const& rhs) noexcept -> Matrix<T, M, N> {
        auto result = Matrix<T, M, N>{T{0}};
        for (auto index = 0; index < M * N; index++)
            result[index] = lhs[index] + rhs[index];
        return result;
    }

    template<typename T, U32 M, U32 N>
    [[nodiscard]] ILINE constexpr auto operator-(Matrix<T, M, N> const& lhs, Matrix<T, M, N> const& rhs) noexcept -> Matrix<T, M, N> {
        auto result = Matrix<T, M, N>{T{0}};
        for (auto index = 0; index < M * N; index++)
            result[index] = lhs[index] - rhs[index];
        return result;
    }

    template<typename T, U32 M, U32 N>
    [[nodiscard]] ILINE constexpr auto operator-(Matrix<T, M, N> const& rhs) noexcept -> Matrix<T, M, N> {
        auto result = Matrix<T, M, N>{T{0}};
        for (auto index = 0; index < M * N; index++)
            result[index] = -rhs[index];
        return result;
    }

    template<typename T, U32 M, U32 N>
    [[nodiscard]] ILINE constexpr auto operator*(Matrix<T, M, N> const& lhs, T rhs) noexcept -> Matrix<T, M, N> {
        auto result = Matrix<T, M, N>{T{0}};
        for (auto index = 0; index < M * N; index++)
            result[index] = rhs * lhs[index];
        return result;
    }

    template<typename T, U32 M, U32 N>
    [[nodiscard]] ILINE constexpr auto operator*(T lhs, Matrix<T, M, N> const& rhs) noexcept -> Matrix<T, M, N> {
        auto result = Matrix<T, M, N>{T{0}};
        for (auto index = 0; index < M * N; index++)
            result[index] = lhs * rhs[index];
        return result;
    }

    template<typename T, U32 M, U32 N>
    [[nodiscard]] ILINE constexpr auto operator/(Matrix<T, M, N> const& lhs, T rhs) noexcept -> Matrix<T, M, N> {
        auto inv = T{1} / rhs;
        auto result = Matrix<T, M, N>{};
        for (auto index = 0; index < M * N; index++)
            result[index] = inv * lhs[index];
        return result;
    }

    template<typename T, U32 M, U32 N>
    ILINE constexpr auto operator+=(Matrix<T, M, N>& lhs, Matrix<T, M, N> const& rhs) noexcept -> Matrix<T, M, N>& {
        lhs = lhs + rhs;
        return lhs;
    }

    template<typename T, U32 M, U32 N>
    ILINE constexpr auto operator-=(Matrix<T, M, N>& lhs, Matrix<T, M, N> const& rhs) noexcept -> Matrix<T, M, N>& {
        lhs = lhs - rhs;
        return lhs;
    }

    template<typename T, U32 M, U32 N>
    ILINE constexpr auto operator*=(Matrix<T, M, N>& lhs, Matrix<T, M, N> const& rhs) noexcept -> Matrix<T, M, N>& {
        lhs = lhs * rhs;
        return lhs;
    }

    template<typename T, U32 M, U32 N>
    ILINE constexpr auto operator*=(Matrix<T, M, N>& lhs, T rhs) noexcept -> Matrix<T, M, N>& {
        lhs = lhs * rhs;
        return lhs;
    }

    template<typename T, U32 M, U32 N>
    ILINE constexpr auto operator/=(Matrix<T, M, N>& lhs, T rhs) noexcept -> Matrix<T, M, N>& {
        lhs = lhs / rhs;
        return lhs;
    }


    template<typename T, U32 N>
    [[nodiscard]] ILINE constexpr auto operator*(Matrix<T, N, N> const& lhs, Matrix<T, N, N> const& rhs) noexcept -> Matrix<T, N, N> {
        auto res = Matrix<T, N, N>{T{0}};
        for (auto i = 0; i < N; i++)
            for (auto j = 0; j < N; j++)
                for (auto k = 0; k < N; k++)
                    res(i, j) += lhs(i, k) * rhs(k, j);
        return res;
    }

    template<typename T, U32 M, U32 N, U32 P>
    [[nodiscard]] ILINE constexpr auto operator*(Matrix<T, M, N> const& lhs, Matrix<T, N, P> const& rhs) noexcept -> Matrix<T, M, P> {
        auto res = Matrix<T, M, P>{T{0}};
        for (auto i = 0; i < M; i++)
            for (auto j = 0; j < P; j++)
                for (auto k = 0; k < N; k++)
                    res(i, j) += lhs(i, k) * rhs(k, j);
        return res;
    }
    template<typename T, U32 M, U32 N>
    [[nodiscard]] ILINE constexpr auto operator*(Matrix<T, M, N> const& lhs, Vector<T, N> const& rhs) noexcept -> Vector<T, M> {
        auto res = Vector<T, M>{T{0}};
        for (auto i = 0; i < M; i++)
            for (auto j = 0; j < N; j++)
                res[i] += lhs(i, j) * rhs[j];
        return res;
    }
}