#pragma once
#include <deque>
#include <vector>
#include <string>
#include <memory>

#include "Calc.h"

struct TrailPoint {
    Vector3 Position;
    float Age;        // 発生してからの経過時間（寿命判定やアルファ減衰に使用）
    // 必要に応じて、発生時のカメラからの距離や、剣の振りの場合はUpベクトルを追加
};

struct TrailVertex {
    Vector3 Position;
    Vector2 UV;
    Vector4 Color;
};

class TrailEffect {
public:
	void Initialize(const std::string& textureName, const Transform& transform, float width, float maxLifetime);
    void UpdateLifetimes();
    void AddPoint(const Vector3& currentEmitterPos);
    void GenerateVertices(const Vector3& cameraPos, std::vector<TrailVertex>& outVertices);

    // setter
    void SetTranslate(Vector3 translate) { translate_ = translate; }
	void SetColor(Vector4 color) { m_Color = color; }
	void SetWidth(float width) { m_Width = width; }
	void SetLifetime(float lifetime) { MAX_LIFETIME = lifetime; }
	void SetDistance(float distance) { MIN_DISTANCE = distance; }
	void SetDeltaTime(float dt) { deltaTime = dt; }

    // getter
    Vector3 GetTranslate() { return translate_; }
    bool IsDead(){ return m_Points.empty(); }
    std::string GetTextureName() { return m_TextureName; }

private:
    Vector3 translate_;

    std::deque<TrailPoint> m_Points;
    float m_Width = 1.5f;
    Vector4 m_Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float deltaTime = 0.1f;
	float MAX_LIFETIME = 1.5f; // 消えるまでの時間
	float MIN_DISTANCE = 0.1f; // ユニット以上動いたら新しいポイントを追加
    std::string m_TextureName;
};