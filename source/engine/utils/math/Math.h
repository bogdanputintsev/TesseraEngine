#pragma once
#include <array>
#include <numbers>

namespace parus::math
{
    /*==================================
     * Constants
     *==================================*/
    static constexpr inline double MATH_EPSILON = 1.192092896e-07f;
    static constexpr inline double MATH_PI = std::numbers::pi;

    /*==================================
     * Vector2 
     *==================================*/
    struct TrivialVector2
    {
        float x;
        float y;
    };
    
    struct Vector2
    {
        float x, y;

        // Default constructor
        Vector2() : x(0), y(0) {}

        // Default destructor
        ~Vector2() = default;
        
        // Parameterized constructor
        Vector2(const float x, const float y) : x(x), y(y) {}

        // Copy constructor
        Vector2(const Vector2& other) = default;

        // Move constructor
        Vector2(Vector2&& other) noexcept : x(other.x), y(other.y) {}

        // Copy assignment operator
        Vector2& operator=(const Vector2& other);

        // Move assignment operator
        Vector2& operator=(Vector2&& other) noexcept;

        // Add two vectors
        Vector2 operator+(const Vector2& other) const;
        
        // Subtract two vectors
        Vector2 operator-(const Vector2& other) const;

        // Addition assignment (+=)
        Vector2& operator+=(const Vector2& other);

        // Subtraction assignment (-=)
        Vector2& operator-=(const Vector2& other);

        // Scalar multiplication (*)
        Vector2 operator*(float scalar) const;

        // Scalar multiplication assignment (*=)
        Vector2& operator*=(float scalar);

        // Equality operator
        bool operator==(const Vector2& other) const;

        // Inequality operator
        bool operator!=(const Vector2& other) const;

        // Make trivially copyable copy of Vector3
        [[nodiscard]] TrivialVector2 trivial() const noexcept { return { .x= x, .y= y }; }
    };

    /*==================================
     * Vector3
     *==================================*/
    struct alignas(16) TrivialVector3
    {
        float x;
        float y;
        float z;
        float pad = 0.0f;
    };
    
    struct Vector3
    {
        float x;
        float y;
        float z;
        
        // Default constructor
        Vector3() : x(0), y(0), z(0) {}

        // Default destructor
        ~Vector3() = default;
        
        // Parameterized constructor
        Vector3(const float x, const float y, const float z) : x(x), y(y), z(z) {}

        // Copy constructor
        Vector3(const Vector3& other) = default;

        // Move constructor
        Vector3(Vector3&& other) noexcept : x(other.x), y(other.y), z(other.z) {}
        
        // Copy assignment operator
        Vector3& operator=(const Vector3& other);

        // Move assignment operator
        Vector3& operator=(Vector3&& other) noexcept;

        // Add two vectors
        Vector3 operator+(const Vector3& other) const;
        
        // Subtract two vectors
        Vector3 operator-(const Vector3& other) const;

        // Addition assignment (+=)
        Vector3& operator+=(const Vector3& other);

        // Subtraction assignment (-=)
        Vector3& operator-=(const Vector3& other);

        // Scalar multiplication (*)
        Vector3 operator*(float scalar) const;

        // Scalar multiplication assignment (*=)
        Vector3& operator*=(float scalar);
        
        // Equality operator
        bool operator==(const Vector3& other) const;

        // Inequality operator
        bool operator!=(const Vector3& other) const;

        // Normalize the vector
        [[nodiscard]] Vector3 normalize() const;

        // Cross product
        [[nodiscard]] Vector3 cross(const Vector3& other) const;

        // Dot product
        [[nodiscard]] float dot(const Vector3& other) const;

        // Default up vector
        static Vector3 up() { return { 0.0f, 1.0f, 0.0f }; }

        // Make trivially copyable copy of Vector3
        [[nodiscard]] TrivialVector3 trivial() const noexcept { return { .x= x, .y= y, .z= z }; }
    };

    /*==================================
     * Matrix4x4
     *==================================*/
    // ReSharper disable once CppInconsistentNaming
    struct alignas(16) TrivialMatrix4x4
    {
        float values[4][4];
    };
    
    // ReSharper disable once CppInconsistentNaming
    struct Matrix4x4
    {
    public:
        // Default constructor - initializes to identity matrix
        Matrix4x4();

        // Default destructor
        ~Matrix4x4() = default;
        
        // Constructor with array input
        explicit Matrix4x4(const std::array<std::array<float, 4>, 4>& values) : values(values) {}

        // Copy constructor
        Matrix4x4(const Matrix4x4& other) = default;

        // Move constructor
        Matrix4x4(Matrix4x4&& other) noexcept = default;

        // Copy assignment operator
        Matrix4x4& operator=(const Matrix4x4& other) = default;

        // Move assignment operator
        Matrix4x4& operator=(Matrix4x4&& other) noexcept = default;

        // Matrix addition
        Matrix4x4 operator+(const Matrix4x4& other) const;

        // Matrix subtraction
        Matrix4x4 operator-(const Matrix4x4& other) const;

        // Matrix multiplication
        Matrix4x4 operator*(const Matrix4x4& other) const;

        // Scalar multiplication
        Matrix4x4 operator*(float scalar) const;

        // Equality operator
        bool operator==(const Matrix4x4& other) const;

        // Inequality operator
        bool operator!=(const Matrix4x4& other) const;

        // Transpose matrix
        [[nodiscard]] Matrix4x4 transpose() const;

        [[nodiscard]] TrivialMatrix4x4 trivial() const noexcept;
        
        static Matrix4x4 perspective(const float fovRadians, const float aspectRatio, const float near, const float far);

        static Matrix4x4 lookAt(const Vector3& eye, const Vector3& target, const Vector3& up);

        static Matrix4x4 identity() { return {}; }

    private:
        std::array<std::array<float, 4>, 4> values;
    };

    /*==================================
     * Vertex
     *==================================*/
    struct TrivialVertex
    {
        TrivialVector3 position;
        TrivialVector3 normal;
        TrivialVector3 tangent;
        TrivialVector2 textureCoordinates;
    };
    
    struct Vertex
    {
        Vector3 position;
        Vector3 normal;
        Vector3 tangent;
        Vector2 textureCoordinates;

        [[nodiscard]] TrivialVertex trivial() const noexcept;
        
        bool operator==(const Vertex& other) const;
    };
    
    /*==================================
     * Math functions
     *==================================*/
    bool isNearlyEqual(const float x, const float y, const double epsilon = MATH_EPSILON);
    float radians(const float degrees); 
    float degrees(const float radians);
}

namespace std
{
    template <>
    struct hash<parus::math::Vector3>
    {
        size_t operator()(const parus::math::Vector3& v) const noexcept
        {
            return ((std::hash<float>()(v.x) ^
                     (std::hash<float>()(v.y) << 1)) >> 1) ^
                   (std::hash<float>()(v.z) << 1);
        }
    };

    template <>
    struct hash<parus::math::Vector2>
    {
        size_t operator()(const parus::math::Vector2& v) const noexcept
        {
            return (std::hash<float>()(v.x) ^
                   (std::hash<float>()(v.y) << 1));
        }
    };

    template <>
    struct hash<parus::math::Vertex> {
        size_t operator()(const parus::math::Vertex& v) const noexcept {
            return ((std::hash<parus::math::Vector3>()(v.position) ^
                    (std::hash<parus::math::Vector3>()(v.normal) << 1)) >> 1) ^
                   ((std::hash<parus::math::Vector2>()(v.textureCoordinates) ^
                    (std::hash<parus::math::Vector3>()(v.tangent) << 1)) >> 1);
        }
    };
}
