#pragma once
#include <algorithm>

/// AL3サンプルプログラム用の数学ライブラリ。
/// MT3準拠で、KamataEngine内部の数学ライブラリと重複する。

struct Matrix4x4 final {
    float m[4][4];
};

struct Vector4 final {
    float x;
    float y;
    float z;
    float w;
};

struct Vector3 final {
    float x;
    float y;
    float z;
};

struct Vector2 final
{
    float x;
    float y;
};
struct Vector2Int final {
    int x;
    int y;
};


// 円周率
const float PI = 3.141592654f;

struct AABB
{
    Vector3 min;
    Vector3 max;
};

// Transform
struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};


// 02_14 29枚目 単項演算子オーバーロード
Vector3 operator+(const Vector3& v);
Vector3 operator-(const Vector3& v);

// 02_06のCameraControllerのUpdate/Reset関数で必要
const Vector3 operator+(const Vector3& v1, const Vector3& v2);
const Vector3 operator-(const Vector3& v1, const Vector3& v2);

// 02_06のスライド24枚目のLerp関数
Vector3 CameraLerp(const Vector3& v1, const Vector3& v2, float t);

// 02_06 スライド29枚目で追加
const Vector3 operator*(const Vector3& v1, const float f);

// 代入演算子オーバーロード
Vector3& operator+=(Vector3& lhs, const Vector3& rhv);
Vector3& operator-=(Vector3& lhs, const Vector3& rhv);
Vector3& operator*=(Vector3& v, float s);
Vector3& operator/=(Vector3& v, float s);

Vector2& operator+=(Vector2& lhs, const Vector2& rhv);

Vector3 operator/(const Vector3& v, float scalar);

// 代入演算子オーバーロード
Matrix4x4& operator*=(Matrix4x4& lhm, const Matrix4x4& rhm);
// 2項演算子オーバーロード
Matrix4x4 operator*(const Matrix4x4& m1, const Matrix4x4& m2);

// 行列の乗算
Matrix4x4 Multiply(Matrix4x4 matrix1, Matrix4x4 matrix2);

// 単位行列の作成
Matrix4x4 MakeIdentityMatrix();
// スケーリング行列の作成
Matrix4x4 MakeScaleMatrix(const Vector3& scale);
// 回転行列の作成
Matrix4x4 MakeRotateXMatrix(float theta);
Matrix4x4 MakeRotateYMatrix(float theta);
Matrix4x4 MakeRotateZMatrix(float theta);
Matrix4x4 MakeRotateMatrix(const Vector3& rot);
// 平行移動行列の作成
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
// アフィン変換行列の作成
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rot, const Vector3& translate);
// 累積の回転行列の場合
Matrix4x4 MakeAffineMatrixR(const Vector3& scale, const Matrix4x4& matRot, const Vector3& translate);
// 透視投影行列
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
// 平行射影行列
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);
// ビューボート変換行列
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);

// 逆行列
Matrix4x4 Inverse(const Matrix4x4& m);

Matrix4x4 MakeIdentity4x4();

Vector3 Normalize(const Vector3& v);
// 内積
float Dot(const Vector3& a, const Vector3& b);

// ベクトルを行列で変換
Vector3 VectorTransform(const Vector3& v, const Matrix4x4& m);

// レイ
float RaySphereIntersect(const Vector3& rayOrigin, const Vector3& rayDir,
    const Vector3& sphereCenter, float radius);

float Smoothstep(float edge0, float edge1, float x);

Vector4 Lerp(const Vector4& a, const Vector4& b, float t);

float Lerp(float x1, float x2, float t);

float EaseIn(float x1, float x2, float t);

float EaseOut(float x1, float x2, float t);

float EaseInOut(float x1, float x2, float t);

bool IsCollision(const AABB& aabb1, const AABB& aabb2);
bool IsCollision(const AABB& aabb, const Vector3& point);

// 2点間の距離の2乗を計算（平方根を取らないことで高速化するのが一般的）
float DistanceSquared(const Vector3& a, const Vector3& b);
// 実際の距離（sqrtを使用）
float Distance(const Vector3& a, const Vector3& b);
// 外積
Vector3 Cross(const Vector3& a, const Vector3& b);