#pragma once
#include "math_utils.h"

struct Matrix4 {
    float m[4][4];

    Matrix4() {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
    }

    static Matrix4 identity() {
        return Matrix4();
    }

    static Matrix4 lookAt(Vec3f eye, Vec3f center, Vec3f up) {
        Vec3f z = (eye - center).normalize();
        Vec3f x = (up ^ z).normalize();
        Vec3f y = (z ^ x).normalize();

        Matrix4 res;
        res.m[0][0] = x.x; res.m[0][1] = x.y; res.m[0][2] = x.z; res.m[0][3] = -(x * eye);
        res.m[1][0] = y.x; res.m[1][1] = y.y; res.m[1][2] = y.z; res.m[1][3] = -(y * eye);
        res.m[2][0] = z.x; res.m[2][1] = z.y; res.m[2][2] = z.z; res.m[2][3] = -(z * eye);
        res.m[3][0] = 0.0f; res.m[3][1] = 0.0f; res.m[3][2] = 0.0f; res.m[3][3] = 1.0f;
        return res;
    }

    static Matrix4 ortho(float left, float right, float bottom, float top, float nearVal, float farVal) {
        Matrix4 res;
        res.m[0][0] = 2.0f / (right - left);
        res.m[1][1] = 2.0f / (top - bottom);
        res.m[2][2] = -2.0f / (farVal - nearVal);
        res.m[0][3] = -(right + left) / (right - left);
        res.m[1][3] = -(top + bottom) / (top - bottom);
        res.m[2][3] = -(farVal + nearVal) / (farVal - nearVal);
        res.m[3][3] = 1.0f;
        return res;
    }

    Matrix4 operator*(const Matrix4& o) const {
        Matrix4 res;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                res.m[i][j] = m[i][0] * o.m[0][j] +
                              m[i][1] * o.m[1][j] +
                              m[i][2] * o.m[2][j] +
                              m[i][3] * o.m[3][j];
            }
        }
        return res;
    }

    Vec3f transformPoint(const Vec3f& p) const {
        float x = m[0][0]*p.x + m[0][1]*p.y + m[0][2]*p.z + m[0][3];
        float y = m[1][0]*p.x + m[1][1]*p.y + m[1][2]*p.z + m[1][3];
        float z = m[2][0]*p.x + m[2][1]*p.y + m[2][2]*p.z + m[2][3];
        float w = m[3][0]*p.x + m[3][1]*p.y + m[3][2]*p.z + m[3][3];
        if (std::abs(w) > 1e-5f) {
            return Vec3f(x / w, y / w, z / w);
        }
        return Vec3f(x, y, z);
    }
};
