#pragma once
#define NOMINMAX

#include <vector>
#include <algorithm>
#include <memory>

#include "Object.h"
#include "Calc.h"
#include "ImGuiManager.h"
#include "CommonStructs.h"
#include "CameraManager.h"

class Camera;

struct RailPoint {
    Vector3 position;
    float roll;        // バンク
    float fov;         // FOV変更用
    float eventID;     // イベント識別
};

class RailCamera {
public:
    std::vector<RailPoint> points;
    // カメラ
    std::unique_ptr <Camera> camera = nullptr;

	void Initialize();
    void Update();
    void EditorUpdate();
    void EditorDraw();
    Vector3 Evaluate(float distance);
    Vector3 GetForward(float distance);

    Vector3 GetPos()const { return pos; }

private:
    Transform cameraTransform
    {
        { 1.0f, 1.0f, 1.0f }, // scale
        { 0.0f, 0.0f, 0.0f }, // rotate
        { 0.0f, 0.0f, -10.0f } // translate
    };

	float t = 1.0f; // レール上の位置
    // RailCamera.h
    bool isRail = false;
    float railT = 0.0f;    // 現在位置
    float railSpeed = 1.0f;    // 速度
    float deltaTime = 1 / 60.0f; // フレーム時間

    Vector3 pos;

    // 選択中
    int selectedPoint;

    // 3Dオブジェクト
    std::vector<std::unique_ptr<Object>> spheres;
    std::vector<std::unique_ptr<Object>> railModels;
  

    // DirectXCommonのポインタ
    DirectXCommon* dxCommon_ = nullptr;
    // WindowAPIのポインタ
    std::unique_ptr <WindowAPI> windowAPI_ = nullptr;

    // 制御点追加
    void AddPoint(Vector3 pos);

    void UpdateGizmo();
    void SelectPointByMouse();


    // レールモデル初期化
    void InitializeRailModels(int count);
    // レール描画
    void DrawRailModels();

    Vector3 CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t);

};

