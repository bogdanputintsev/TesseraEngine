#include "Math.h"

#include <corecrt_math.h>
#include <cmath>

namespace parus::math
{
    
    /*==================================
     * Math functions
     *==================================*/
    bool isNearlyEqual(const float x, const float y, const double epsilon)
    {
        return fabsf(x - y) < epsilon;
    }

    float radians(const float degrees)
    {
        return degrees * static_cast<float>(MATH_PI / 180.0f);
    }

    float degrees(const float radians)
    {
        return radians * static_cast<float>(180.0f / MATH_PI);
    }

    /*==================================
     * Vector2 implementation
     *==================================*/
    Vector2& Vector2::operator=(const Vector2& other)
    {
        if (this != &other)
        {
            x = other.x;
            y = other.y;
        }
        return *this;
    }

    Vector2& Vector2::operator=(Vector2&& other) noexcept
    {
        if (this != &other)
        {
            x = other.x;
            y = other.y;
        }
        return *this;
    }

    Vector2 Vector2::operator+(const Vector2& other) const
    {
        return { x + other.x, y + other.y };
    }

    Vector2 Vector2::operator-(const Vector2& other) const
    {
        return { x - other.x, y - other.y };
    }

    Vector2 Vector2::operator*(const float scalar) const
    {
        return { x * scalar, y * scalar };
    }

    Vector2& Vector2::operator+=(const Vector2& other)
    {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vector2& Vector2::operator-=(const Vector2& other)
    {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    Vector2& Vector2::operator*=(const float scalar)
    {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    bool Vector2::operator==(const Vector2& other) const
    {
        return isNearlyEqual(x, other.x)
            && isNearlyEqual(y, other.y);
    }

    bool Vector2::operator!=(const Vector2& other) const
    {
        return !(*this == other);
    }

    /*==================================
     * Vector3 implementation
     *==================================*/
    Vector3& Vector3::operator=(const Vector3& other)
    {
        if (this != &other)
        {
            x = other.x;
            y = other.y;
            z = other.z;
        }
        return *this;
    }

    Vector3& Vector3::operator=(Vector3&& other) noexcept
    {
        if (this != &other)
        {
            x = other.x;
            y = other.y;
            z = other.z;
        }
        return *this;
    }

    Vector3 Vector3::operator+(const Vector3& other) const
    {
        return { x + other.x, y + other.y, z + other.z };
    }

    Vector3 Vector3::operator-(const Vector3& other) const
    {
        return { x - other.x, y - other.y, z - other.z };
    }

    Vector3& Vector3::operator+=(const Vector3& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    Vector3& Vector3::operator-=(const Vector3& other)
    {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    Vector3 Vector3::operator*(const float scalar) const
    {
        return { x * scalar, y * scalar, z * scalar };
    }

    Vector3& Vector3::operator*=(const float scalar)
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    bool Vector3::operator==(const Vector3& other) const
    {
        return isNearlyEqual(x, other.x)
            && isNearlyEqual(y, other.y)
            && isNearlyEqual(z, other.z);
    }

    bool Vector3::operator!=(const Vector3& other) const
    {
        return !(*this == other);
    }

    Vector3 Vector3::normalize() const
    {
        const float length = std::sqrt(x * x + y * y + z * z);
        return (length > MATH_EPSILON)
            ? Vector3 { x / length, y / length, z / length }
            : Vector3 { 0, 0, 0 };
    }

    Vector3 Vector3::cross(const Vector3& other) const
    {
        return {
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        };
    }

    float Vector3::dot(const Vector3& other) const
    {
        return x * other.x + y * other.y + z * other.z;
    }


    /*==================================
     * Matrix4x4 implementation
     *==================================*/
    Matrix4x4::Matrix4x4()
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                values[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
    }

    Matrix4x4 Matrix4x4::operator+(const Matrix4x4& other) const
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                result.values[i][j] = values[i][j] + other.values[i][j];
            }
        }
        return result;
    }

    Matrix4x4 Matrix4x4::operator-(const Matrix4x4& other) const
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                result.values[i][j] = values[i][j] - other.values[i][j];
            }
        }
        return result;
    }

    Matrix4x4 Matrix4x4::operator*(const Matrix4x4& other) const
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                result.values[i][j] = 0;
                for (int k = 0; k < 4; ++k)
                {
                    result.values[i][j] += values[i][k] * other.values[k][j];
                }
            }
        }
        return result;
    }

    Matrix4x4 Matrix4x4::operator*(const float scalar) const
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                result.values[i][j] = 0;
                for (int k = 0; k < 4; ++k)
                {
                    result.values[i][j] += values[i][k] * scalar;
                }
            }
        }
        return result;
    }

    bool Matrix4x4::operator==(const Matrix4x4& other) const
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                if (!isNearlyEqual(values[i][j], other.values[i][j]))
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool Matrix4x4::operator!=(const Matrix4x4& other) const
    {
        return !(*this == other);
    }

    Matrix4x4 Matrix4x4::transpose() const
    {
        Matrix4x4 result;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                result.values[i][j] = values[j][i];
            }
        }
        return result;
    }

    TrivialMatrix4x4 Matrix4x4::trivial() const noexcept
    {
        TrivialMatrix4x4 result{};

        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                result.values[i][j] = values[i][j];
            }
        }

        return result;
    }

    Matrix4x4 Matrix4x4::perspective(const float fovRadians, const float aspectRatio, const float near, const float far)
    {
        Matrix4x4 result{};
        const float tangentOfHalfFov = std::tan(fovRadians / 2.0f);

        result.values[0][0] = 1.0f / (aspectRatio * tangentOfHalfFov);
        result.values[1][1] = -1.0f / tangentOfHalfFov;
        result.values[2][2] = -(far + near) / (far - near);
        result.values[2][3] = -1.0f;
        result.values[3][2] = -(2.0f * far * near) / (far - near);
        result.values[3][3] = 0.0f;

        return result;
    }

    Matrix4x4 Matrix4x4::lookAt(const Vector3& eye, const Vector3& target, const Vector3& up)
    {
        const Vector3 forward = (eye - target).normalize();  // Camera direction (negative Z)
        const Vector3 right = up.cross(forward).normalize(); // Camera right (X-axis)
        const Vector3 upCorrected = forward.cross(right);    // Corrected up (Y-axis)

        Matrix4x4 result;
        result.values[0][0] = right.x;
        result.values[1][0] = right.y;
        result.values[2][0] = right.z;
        result.values[0][1] = upCorrected.x;
        result.values[1][1] = upCorrected.y;
        result.values[2][1] = upCorrected.z;
        result.values[0][2] = forward.x;
        result.values[1][2] = forward.y;
        result.values[2][2] = forward.z;
        result.values[3][0] = -right.dot(eye);
        result.values[3][1] = -upCorrected.dot(eye);
        result.values[3][2] = -forward.dot(eye);
        result.values[3][3] = 1.0f;

        return result;
    }

    /*==================================
     * Vertex
     *==================================*/
    bool Vertex::operator==(const Vertex& other) const
    {
        return position == other.position
            && normal == other.normal
            && textureCoordinates == other.textureCoordinates
            && tangent == other.tangent;
    }
    
    TrivialVertex Vertex::trivial() const noexcept
    {
         return
            {
                .position = position.trivial(),
                .normal = normal.trivial(),
                .tangent = tangent.trivial(),
                .textureCoordinates= textureCoordinates.trivial()
            }; 
    }
}
