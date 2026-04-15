#include "Calc.h"
#include <cmath>
#include <numbers>

// 02_14 29枚目 単項演算子オーバーロード
Vector3 operator+(const Vector3& v) { return v; }
Vector3 operator-(const Vector3& v) { return Vector3(-v.x, -v.y, -v.z); }

// 02_06の29枚目(CameraControllerのUpdate)で必要
const Vector3 operator*(const Vector3& v1, const float f)
{
    Vector3 temp(v1);
    return temp *= f;
}

// 02_06のCameraControllerのUpdate/Reset関数で必要
const Vector3 operator+(const Vector3& v1, const Vector3& v2)
{
    Vector3 temp(v1);
    return temp += v2;
}

const Vector3 operator-(const Vector3& v1, const Vector3& v2) {
	Vector3 temp(v1);
	return temp -= v2;
}

// 02_06のスライド24枚目のLerp関数
Vector3 CameraLerp(const Vector3& v1, const Vector3& v2, float t) { return Vector3(Lerp(v1.x, v2.x, t), Lerp(v1.y, v2.y, t), Lerp(v1.z, v2.z, t)); }

Vector3& operator+=(Vector3& lhv, const Vector3& rhv)
{
    lhv.x += rhv.x;
    lhv.y += rhv.y;
    lhv.z += rhv.z;
    return lhv;
}

Vector3& operator-=(Vector3& lhv, const Vector3& rhv)
{
    lhv.x -= rhv.x;
    lhv.y -= rhv.y;
    lhv.z -= rhv.z;
    return lhv;
}

Vector3& operator-=(Vector3& lhv, Vector3& rhv)
{
    lhv.x -= rhv.x;
    lhv.y -= rhv.y;
    lhv.z -= rhv.z;
    return lhv;
}

Vector3& operator*=(Vector3& v, float s)
{
    v.x *= s;
    v.y *= s;
    v.z *= s;
    return v;
}

Vector3& operator/=(Vector3& v, float s)
{
    v.x /= s;
    v.y /= s;
    v.z /= s;
    return v;
}

Vector2& operator+=(Vector2& lhs, const Vector2& rhv)
{
    lhs.x += rhv.x;
    lhs.y += rhv.y;
    return lhs;
}

Vector3 operator/(const Vector3& v, float scalar)
{
    return Vector3 { v.x / scalar, v.y / scalar, v.z / scalar };
}

// 行列の乗算
Matrix4x4 Multiply(Matrix4x4 matrix1, Matrix4x4 matrix2)
{
    Matrix4x4 result = {};

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                result.m[i][j] += matrix1.m[i][k] * matrix2.m[k][j];
            }
        }
    }

    return result;
}

Matrix4x4 MakeIdentityMatrix()
{
    static const Matrix4x4 result { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };

    return result;
}

Matrix4x4 MakeScaleMatrix(const Vector3& scale)
{

    Matrix4x4 result { scale.x, 0.0f, 0.0f, 0.0f, 0.0f, scale.y, 0.0f, 0.0f, 0.0f, 0.0f, scale.z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };

    return result;
}

Matrix4x4 MakeRotateXMatrix(float theta)
{
    float sin = std::sin(theta);
    float cos = std::cos(theta);

    Matrix4x4 result { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, cos, sin, 0.0f, 0.0f, -sin, cos, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };

    return result;
}

Matrix4x4 MakeRotateYMatrix(float theta)
{
    float sin = std::sin(theta);
    float cos = std::cos(theta);

    Matrix4x4 result { cos, 0.0f, -sin, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, sin, 0.0f, cos, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };

    return result;
}

Matrix4x4 MakeRotateZMatrix(float theta)
{
    float sin = std::sin(theta);
    float cos = std::cos(theta);

    Matrix4x4 result { cos, sin, 0.0f, 0.0f, -sin, cos, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };

    return result;
}

Matrix4x4 MakeRotateMatrix(const Vector3& rot)
{
    Matrix4x4 matRotX = MakeRotateXMatrix(rot.x);
    Matrix4x4 matRotY = MakeRotateYMatrix(rot.y);
    Matrix4x4 matRotZ = MakeRotateZMatrix(rot.z);
    return matRotZ * matRotX * matRotY;
}

Matrix4x4 MakeTranslateMatrix(const Vector3& translate)
{
    Matrix4x4 result { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, translate.x, translate.y, translate.z, 1.0f };

    return result;
}

Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rot, const Vector3& translate)
{

    // スケーリング行列の作成
    Matrix4x4 matScale = MakeScaleMatrix(scale);

    Matrix4x4 matRotX = MakeRotateXMatrix(rot.x);
    Matrix4x4 matRotY = MakeRotateYMatrix(rot.y);
    Matrix4x4 matRotZ = MakeRotateZMatrix(rot.z);
    // 回転行列の合成
    Matrix4x4 matRot = matRotZ * matRotX * matRotY;

    // 平行移動行列の作成
    Matrix4x4 matTrans = MakeTranslateMatrix(translate);

    // スケーリング、回転、平行移動の合成
    Matrix4x4 matTransform = matScale * matRot * matTrans;

    return matTransform;
}

// 累積の回転行列の場合
Matrix4x4 MakeAffineMatrixR(const Vector3& scale, const Matrix4x4& matRot, const Vector3& translate)
{

    // スケーリング行列の作成
    Matrix4x4 matScale = MakeScaleMatrix(scale);

    // 平行移動行列の作成
    Matrix4x4 matTrans = MakeTranslateMatrix(translate);

    // スケーリング、回転、平行移動の合成
    Matrix4x4 matTransform = matScale * matRot * matTrans;

    return matTransform;
}

// 透視投影行列
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip)
{
    Matrix4x4 result = {};

    float yScale = 1.0f / std::tan(fovY * 0.5f);
    float xScale = yScale / aspectRatio;

    result.m[0][0] = xScale;
    result.m[1][1] = yScale;
    result.m[2][2] = farClip / (farClip - nearClip);
    result.m[2][3] = 1.0f;
    result.m[3][2] = -nearClip * farClip / (farClip - nearClip);
    result.m[3][3] = 0.0f;

    return result;
}

// 平行射影行列
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip)
{
    Matrix4x4 result = {};

    result.m[0][0] = 2.0f / (right - left);
    result.m[1][1] = 2.0f / (top - bottom);
    result.m[2][2] = 1.0f / (farClip - nearClip);
    result.m[3][0] = (left + right) / (left - right);
    result.m[3][1] = (top + bottom) / (bottom - top);
    result.m[3][2] = nearClip / (nearClip - farClip);
    result.m[3][3] = 1.0f;

    return result;
}

// ビューボート変換行列
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth)
{
    Matrix4x4 m = {};
    m.m[0][0] = width / 2.0f;
    m.m[1][1] = -height / 2.0f; // Y座標は反転
    m.m[2][2] = maxDepth - minDepth;
    m.m[3][0] = left + width / 2.0f;
    m.m[3][1] = top + height / 2.0f;
    m.m[3][2] = minDepth;
    m.m[3][3] = 1.0f;
    return m;
}

// 逆行列
Matrix4x4 Inverse(const Matrix4x4& m) {
    Matrix4x4 result = {};
    float tmp[16];
    const float* src = &m.m[0][0];

    // 余因子展開で4x4逆行列を計算
    tmp[0] = src[5] * src[10] * src[15] - src[5] * src[11] * src[14] - src[9] * src[6] * src[15] + src[9] * src[7] * src[14] + src[13] * src[6] * src[11] - src[13] * src[7] * src[10];
    tmp[4] = -src[4] * src[10] * src[15] + src[4] * src[11] * src[14] + src[8] * src[6] * src[15] - src[8] * src[7] * src[14] - src[12] * src[6] * src[11] + src[12] * src[7] * src[10];
    tmp[8] = src[4] * src[9] * src[15] - src[4] * src[11] * src[13] - src[8] * src[5] * src[15] + src[8] * src[7] * src[13] + src[12] * src[5] * src[11] - src[12] * src[7] * src[9];
    tmp[12] = -src[4] * src[9] * src[14] + src[4] * src[10] * src[13] + src[8] * src[5] * src[14] - src[8] * src[6] * src[13] - src[12] * src[5] * src[10] + src[12] * src[6] * src[9];
    tmp[1] = -src[1] * src[10] * src[15] + src[1] * src[11] * src[14] + src[9] * src[2] * src[15] - src[9] * src[3] * src[14] - src[13] * src[2] * src[11] + src[13] * src[3] * src[10];
    tmp[5] = src[0] * src[10] * src[15] - src[0] * src[11] * src[14] - src[8] * src[2] * src[15] + src[8] * src[3] * src[14] + src[12] * src[2] * src[11] - src[12] * src[3] * src[10];
    tmp[9] = -src[0] * src[9] * src[15] + src[0] * src[11] * src[13] + src[8] * src[1] * src[15] - src[8] * src[3] * src[13] - src[12] * src[1] * src[11] + src[12] * src[3] * src[9];
    tmp[13] = src[0] * src[9] * src[14] - src[0] * src[10] * src[13] - src[8] * src[1] * src[14] + src[8] * src[2] * src[13] + src[12] * src[1] * src[10] - src[12] * src[2] * src[9];
    tmp[2] = src[1] * src[6] * src[15] - src[1] * src[7] * src[14] - src[5] * src[2] * src[15] + src[5] * src[3] * src[14] + src[13] * src[2] * src[7] - src[13] * src[3] * src[6];
    tmp[6] = -src[0] * src[6] * src[15] + src[0] * src[7] * src[14] + src[4] * src[2] * src[15] - src[4] * src[3] * src[14] - src[12] * src[2] * src[7] + src[12] * src[3] * src[6];
    tmp[10] = src[0] * src[5] * src[15] - src[0] * src[7] * src[13] - src[4] * src[1] * src[15] + src[4] * src[3] * src[13] + src[12] * src[1] * src[7] - src[12] * src[3] * src[5];
    tmp[14] = -src[0] * src[5] * src[14] + src[0] * src[6] * src[13] + src[4] * src[1] * src[14] - src[4] * src[2] * src[13] - src[12] * src[1] * src[6] + src[12] * src[2] * src[5];
    tmp[3] = -src[1] * src[6] * src[11] + src[1] * src[7] * src[10] + src[5] * src[2] * src[11] - src[5] * src[3] * src[10] - src[9] * src[2] * src[7] + src[9] * src[3] * src[6];
    tmp[7] = src[0] * src[6] * src[11] - src[0] * src[7] * src[10] - src[4] * src[2] * src[11] + src[4] * src[3] * src[10] + src[8] * src[2] * src[7] - src[8] * src[3] * src[6];
    tmp[11] = -src[0] * src[5] * src[11] + src[0] * src[7] * src[9] + src[4] * src[1] * src[11] - src[4] * src[3] * src[9] - src[8] * src[1] * src[7] + src[8] * src[3] * src[5];
    tmp[15] = src[0] * src[5] * src[10] - src[0] * src[6] * src[9] - src[4] * src[1] * src[10] + src[4] * src[2] * src[9] + src[8] * src[1] * src[6] - src[8] * src[2] * src[5];

    float det = src[0] * tmp[0] + src[1] * tmp[4] + src[2] * tmp[8] + src[3] * tmp[12];
    float invDet = 1.0f / det;

    float* dst = &result.m[0][0];
    for (int i = 0; i < 16; i++) dst[i] = tmp[i] * invDet;

    return result;
}

// 単位行列の作成
Matrix4x4 MakeIdentity4x4()
{
    Matrix4x4 result = {}; // ゼロ初期化
    for (int i = 0; i < 4; ++i)
        result.m[i][i] = 1.0f;
    return result;
}

Vector3 Normalize(const Vector3& v)
{
    float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);

    // 0除算防止
    if (length == 0.0f) {
        return { 0.0f, 0.0f, 0.0f };
    }

    return {
        v.x / length,
        v.y / length,
        v.z / length
    };
}

float Dot(const Vector3& a, const Vector3& b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}


Matrix4x4& operator*=(Matrix4x4& lhm, const Matrix4x4& rhm)
{
    Matrix4x4 result {};

    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 4; j++) {
            for (size_t k = 0; k < 4; k++) {
                result.m[i][j] += lhm.m[i][k] * rhm.m[k][j];
            }
        }
    }
    lhm = result;
    return lhm;
}

Matrix4x4 operator*(const Matrix4x4& m1, const Matrix4x4& m2)
{
    Matrix4x4 result = m1;

    return result *= m2;
}

Vector3 VectorTransform(const Vector3& v, const Matrix4x4& m) {
	float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];
	return Vector3{
		(v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0]) / w,
		(v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1]) / w,
		(v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2]) / w,
	};
}

float RaySphereIntersect(const Vector3& rayOrigin, const Vector3& rayDir, const Vector3& sphereCenter, float radius) {
	Vector3 oc = rayOrigin - sphereCenter;

	float a = Dot(rayDir, rayDir);
	float b = 2.0f * Dot(oc, rayDir);
	float c = Dot(oc, oc) - radius * radius;

	float discriminant = b * b - 4 * a * c;

	if (discriminant < 0) {
		return -1.0f; // 交差なし
	}

	return (-b - sqrtf(discriminant)) / (2.0f * a); // 近い方の距離
}

float Smoothstep(float edge0, float edge1, float x) {
    // ゼロ除算（edge0とedge1が完全に同じ値の場合）の安全対策
    if (edge0 == edge1) {
        return (x >= edge0) ? 1.0f : 0.0f;
    }

    // 割合を 0.0 ～ 1.0 の範囲にクランプする
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);

    // エルミート補間（緩やかに始まって緩やかに終わるカーブ）を計算
    return t * t * (3.0f - 2.0f * t);
}

Vector4 Lerp(const Vector4& a, const Vector4& b, float t) {
    Vector4 result;
    result.x = a.x + (b.x - a.x) * t;
    result.y = a.y + (b.y - a.y) * t;
    result.z = a.z + (b.z - a.z) * t;
    result.w = a.w + (b.w - a.w) * t;
    return result;
}

float Lerp(float x1, float x2, float t) { return (1.0f - t) * x1 + t * x2; }

float EaseIn(float x1, float x2, float t)
{
    float easedT = t * t;

    return Lerp(x1, x2, easedT);
}

float EaseOut(float x1, float x2, float t)
{
    float easedT = 1.0f - std::powf(1.0f - t, 3.0f);

    return Lerp(x1, x2, easedT);
}

float EaseInOut(float x1, float x2, float t)
{
    float easedT = -(std::cosf(std::numbers::pi_v<float> * t) - 1.0f) / 2.0f;

    return Lerp(x1, x2, easedT);
}

bool IsCollision(const AABB& aabb1, const AABB& aabb2)
{
    return (aabb1.min.x <= aabb2.max.x && aabb1.max.x >= aabb2.min.x) && // x軸
        (aabb1.min.y <= aabb2.max.y && aabb1.max.y >= aabb2.min.y) && // y軸
        (aabb1.min.z <= aabb2.max.z && aabb1.max.z >= aabb2.min.z); // z軸
}

bool IsCollision(const AABB& aabb, const Vector3& point)
{
    // 各軸について、点がAABBの範囲内にあるかを確認
    if (point.x < aabb.min.x || point.x > aabb.max.x) {
        return false;
    }
    if (point.y < aabb.min.y || point.y > aabb.max.y) {
        return false;
    }
    if (point.z < aabb.min.z || point.z > aabb.max.z) {
        return false;
    }

    // すべての軸で範囲内なら、衝突（中にある）
    return true;
}

// 2点間の距離の2乗を計算（平方根を取らないことで高速化するのが一般的）
float DistanceSquared(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

// 実際の距離（sqrtを使用）
float Distance(const Vector3& a, const Vector3& b) {
    return std::sqrt(DistanceSquared(a, b));
}

Vector3 Cross(const Vector3& a, const Vector3& b) {
    return Vector3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}