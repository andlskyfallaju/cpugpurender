#pragma once
#include <cmath>
#include <iostream>

template<typename T>
struct Vec2 {
    T x, y;
    Vec2() : x(0), y(0) {}
    Vec2(T x, T y) : x(x), y(y) {}

    Vec2<T> operator+(const Vec2<T>& v) const { return Vec2<T>(x + v.x, y + v.y); }
    Vec2<T> operator-(const Vec2<T>& v) const { return Vec2<T>(x - v.x, y - v.y); }
    Vec2<T> operator*(float f) const { return Vec2<T>(x * f, y * f); }
    T operator*(const Vec2<T>& v) const { return x * v.x + y * v.y; } // dot product
};

template<typename T>
struct Vec3 {
    T x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(T x, T y, T z) : x(x), y(y), z(z) {}

    Vec3<T> operator+(const Vec3<T>& v) const { return Vec3<T>(x + v.x, y + v.y, z + v.z); }
    Vec3<T> operator-(const Vec3<T>& v) const { return Vec3<T>(x - v.x, y - v.y, z - v.z); }
    Vec3<T> operator*(float f) const { return Vec3<T>(x * f, y * f, z * f); }
    T operator*(const Vec3<T>& v) const { return x * v.x + y * v.y + z * v.z; } // dot product
    Vec3<T> operator^(const Vec3<T>& v) const { return Vec3<T>(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x); } // cross product

    float norm() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3<T>& normalize() {
        float n = norm();
        if (n > 1e-5) {
            x /= n; y /= n; z /= n;
        }
        return *this;
    }
};

using Vec2f = Vec2<float>;
using Vec2i = Vec2<int>;
using Vec3f = Vec3<float>;
using Vec3i = Vec3<int>;
