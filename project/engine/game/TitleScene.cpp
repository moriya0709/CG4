#include "TitleScene.h"
#include "ObjectCommon.h"
#include "SpriteCommon.h"
#include "LineCommon.h"
#include "SceneManager.h"
#include "SkyBox.h"
#include "TrailEffectManager.h"

void TitleScene::Initialize() {

	// カメラ初期化
	camera = std::make_unique<Camera>();
	camera->SetRotate({ cameraTransform.rotate });
	camera->SetTranslate({ cameraTransform.translate });

	// カメラマネージャ登録
	CameraManager::GetInstance()->AddCamera("main", camera.get());
	CameraManager::GetInstance()->SetActiveCamera("main");

	// 3Dオブジェクト
	object = std::make_unique <Object>();
	object->Initialize(camera.get());
	object->SetTranslate({ 0.0f, 0.0f, 0.0f });

	// 初期化済みの3Dオブジェクトにモデルを紐づける
	object->SetModel("walk.gltf");

	// パーティクル

	// トルネード
	for (int i = 0; i < kTornadoCount; i++) {
		tornado[i] = std::make_unique<ParticleEmitter>();
	}
	tornado[0]->Initialize("Tornado1", transformParticle, 5, 0.1f);
	tornado[1]->Initialize("Tornado2", transformParticle, 5, 0.1f);
	tornado[2]->Initialize("Tornado3", transformParticle, 5, 0.1f);
	tornado[3]->Initialize("Tornado4", transformParticle, 5, 0.1f);
	tornado[4]->Initialize("Tornado5", transformParticle, 5, 0.1f);
	tornado[5]->Initialize("Tornado6", transformParticle, 5, 0.1f);
	tornado[6]->Initialize("Tornado7", transformParticle, 5, 0.1f);
	tornado[7]->Initialize("Tornado8", transformParticle, 5, 0.1f);
	tornado[0]->SetActive("Tornado1");
	tornado[0]->LoadParticle("Resource/particle/tornado.csv");
	tornado[1]->SetActive("Tornado2");
	tornado[1]->LoadParticle("Resource/particle/tornado_2.csv");
	tornado[2]->SetActive("Tornado3");
	tornado[2]->LoadParticle("Resource/particle/tornado_3.csv");
	tornado[3]->SetActive("Tornado4");
	tornado[3]->LoadParticle("Resource/particle/tornado_4.csv");
	tornado[4]->SetActive("Tornado5");
	tornado[4]->LoadParticle("Resource/particle/tornado_5.csv");
	tornado[5]->SetActive("Tornado6");
	tornado[5]->LoadParticle("Resource/particle/tornado_6.csv");
	tornado[6]->SetActive("Tornado7");
	tornado[6]->LoadParticle("Resource/particle/tornado_7.csv");
	tornado[7]->SetActive("Tornado8");
	tornado[7]->LoadParticle("Resource/particle/tornado_8.csv");

	particleMesh = std::make_unique<ParticleEmitter>();
	particleMesh->Initialize("sphere", transformParticle, 5, 0.1f);
	particleMesh->SetActive("sphere");
	particleMesh->LoadParticle("Resource/particle/tornado.csv");

	// トレイルエフェクト
	trailEffect->Initialize("Resource/trail/trail.png", transformParticle, 1.0f, 1.5f);
	trailEffect->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	trailEffect->LoadCsv("Resource/trail/shot.csv");
	trailEffect->SetDistance(0.01f);
	TrailEffectManager::GetInstance()->AddTrail(trailEffect);

	// 線
	line = std::make_unique<Line>();
	line->Initialize(camera.get());
	line->AddLine(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 10.0f, 0.0f));
	
	// 音声再生
	//SoundManager::GetInstance()->Play("bgm");

}

void TitleScene::Update() {
	// 入力取得
	auto input = Input::GetInstance();
	// カメラ更新
	CameraManager::GetInstance()->Update();

	// * 3Dオブジェクト* //
	object->Update();

	//　線
	line->Update();

	// *パーティクル* //
	//if (isTornado) {
	//	for (int i = 0; i < kTornadoCount; i++) {
	//		tornado[i]->Update();
	//	}
	//}
	//particleMesh->Update();
	//particleMesh->Editor();

	// マウスのワールド座標変換
	//transformParticle.translate = input->GetMouseWorld(camera.get());

	// トレイルエフェクト更新
	//trailEffect->AddPoint(transformParticle.translate);
	//trailEffect->SetTranslate(transformParticle.translate);
	
	// スカイボックス
	Skybox::GetInstance()->Update();

#pragma region ライティング
	// *ライティング* //
	// 平行光
	object->SetDirectionalLight(isDirectionalLight);
	object->SetDirectionalLightDirection(DirectionalLightDirection);
	object->SetDirectionalLightColor(DirectionalLightColor);
	object->SetDirectionalLightIntensity(DirectionalLightIntensity);
	// 環境光
	object->SetAmbientLight(isAmbientLight);
	object->SetAmbientLightColor(AmbientLightColor);
	object->SetAmbientLightIntensity(AmbientLightIntensity);
	// ポイントライト
	object->SetPointLight(isPointLight);
	object->SetPointLightColor(PointLightColor);
	object->SetPointLightPosition(PointLightPosition);
	object->SetPointLightIntensity(PointLightIntensity);
	// スポットライト
	object->SetSpotLight(isSpotLight);
	object->SetSpotLightColor(SpotLightColor);
	object->SetSpotLightPosition(SpotLightPosition);
	object->SetSpotLightDirection(SpotLightDirection);
	object->SetSpotLightRange(SpotLightRange);
	object->SetSpotLightIntensity(SpotLightIntensity);
#pragma endregion

#pragma region ポストエフェクト
	// *ポストエフェクト* //
	//PostEffect::GetInstance()->Update(camera.get());

	// 反転
	PostEffect::GetInstance()->SetInversion(isInversion);
	// グレースケール
	PostEffect::GetInstance()->SetGrayscale(isGrayscale);
	// 放射線ブラー
	PostEffect::GetInstance()->SetRadialBlur(isRadialBlur);
	PostEffect::GetInstance()->SetBlurCenter(blurCenter);
	PostEffect::GetInstance()->SetBlurWidth(blurWidth);
	PostEffect::GetInstance()->SetBlurSamples(blurSamples);
	// ディスタンスフォグ
	PostEffect::GetInstance()->SetDistanceFog(isDistanceFog);
	PostEffect::GetInstance()->SetDistanceFogColor(distanceFogColor);
	PostEffect::GetInstance()->SetDistanceFogStart(distanceStart);
	PostEffect::GetInstance()->SetDistanceFogEnd(distanceEnd);
	// ハイトフォグ
	PostEffect::GetInstance()->SetHeightFog(isHeightFog);
	PostEffect::GetInstance()->SetHeightFogColor(heightFogColor);
	PostEffect::GetInstance()->SetHeightFogTop(heightFogTop);
	PostEffect::GetInstance()->SetHeightFogBottom(heightFogBottom);
	PostEffect::GetInstance()->SetHeightFogDensity(heightFogDensity);
	PostEffect::GetInstance()->HightFogUpdate(camera.get());
	// DOF
	PostEffect::GetInstance()->SetDOF(isDOF);
	PostEffect::GetInstance()->SetFocusDistance(focusDistance);
	PostEffect::GetInstance()->SetBokehRadius(bokehRadius);
	PostEffect::GetInstance()->SetFocusRange(focusRange);
	// ブルーム
	PostEffect::GetInstance()->SetBloomIntensity(bloomIntensity);
	PostEffect::GetInstance()->SetBloomThreshold(bloomThreshold);
	PostEffect::GetInstance()->SetBloomBlurRadius(bloomBlurRadius);
	// レンズフレア
	PostEffect::GetInstance()->SetLensFlare(isLensFlare);
	PostEffect::GetInstance()->SetLensFlareGhostCount(lensFlareGhostCount);
	PostEffect::GetInstance()->SetLensFlareHaloWidth(lensFlareHaloWidth);
	PostEffect::GetInstance()->SetIsACES(isACES);
	PostEffect::GetInstance()->SetCAIntensity(caIntensity);
	// モーションブラー
	PostEffect::GetInstance()->SetMotionBlur(isMotionBlur);
	PostEffect::GetInstance()->SetMotionBlurSamples(motionBlurSamples);
	PostEffect::GetInstance()->SetMotionBlurScale(motionBlurScale);

#pragma endregion

#pragma region レイマーチング
	// レイマーチング
	RayMarching::GetInstance()->Update(camera.get());
	//rayMarching->SetTime(rayMarchingTime);
	RayMarching::GetInstance()->SetSunDir(rayMarchingSunDir);
	RayMarching::GetInstance()->SetCloudCoverage(rayMarchingCloudCoverage);
	RayMarching::GetInstance()->SetCloudTop(rayMarchingCloudBottom);
	RayMarching::GetInstance()->SetCloudBottom(rayMarchingCloudTop);
	RayMarching::GetInstance()->SetRialLight(rayMarchingIsRialLight);
	RayMarching::GetInstance()->SetAnimeLight(rayMarchingIsAnimeLight);
	RayMarching::GetInstance()->SetMotionBlur(rayMarchingIsMotionBlur);
	RayMarching::GetInstance()->SetCloudOpacity(rayMarchingCloudOpacity);
	RayMarching::GetInstance()->SetStorm(isStorm);
	RayMarching::GetInstance()->SetThunderFrequency(thunderFrequency);
	RayMarching::GetInstance()->SetThunderBrightness(thunderBrightness);


#pragma endregion

#ifdef USE_IMGUI
	// ImGui
	// フレームレートの取得と表示
	float fps = ImGui::GetIO().Framerate;
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / fps, fps);

	ImGui::Checkbox("isTornado", &isTornado);

	ImGui::DragFloat3("cameraTranslate", &cameraTransform.translate.x, 0.1f, -500.0f, 500.0f);
	ImGui::DragFloat3("cameraRotate", &cameraTransform.rotate.x, 0.01f, -10.0f, 10.0f);
	camera->SetTranslate(cameraTransform.translate);
	camera->SetRotate(cameraTransform.rotate);

#pragma region ライティング
	// *ライティング* //
	ImGui::Text("Lighting"); // ライティングのテキスト

	// 平行光
	if (ImGui::TreeNode("DirectionalLight")) {
		ImGui::Checkbox("OnOff", &isDirectionalLight);
		if (isDirectionalLight) {
			ImGui::ColorEdit4("Color", &DirectionalLightColor.x);
			ImGui::DragFloat3("Direction", &DirectionalLightDirection.x, 0.01f, -100.0f, 100.0f);
			ImGui::DragFloat("Intensity", &DirectionalLightIntensity, 0.01f, 0.0f, 10.0f);
		}
		ImGui::TreePop();
	}
	// 環境光
	if (ImGui::TreeNode("AmbientLight")) {
		ImGui::Checkbox("OnOff", &isAmbientLight);
		if (isAmbientLight) {
			ImGui::ColorEdit4("Color", &AmbientLightColor.x);
			ImGui::DragFloat("Intensity", &AmbientLightIntensity, 0.01f, 0.0f, 10.0f);
		}

		ImGui::TreePop();
	}
	// ポイントライト
	if (ImGui::TreeNode("PointLight")) {
		ImGui::Checkbox("OnOff", &isPointLight);
		if (isPointLight) {
			ImGui::ColorEdit4("Color", &PointLightColor.x);
			ImGui::DragFloat3("Position", &PointLightPosition.x, 0.01f, -100.0f, 100.0f);
			ImGui::DragFloat("Intensity", &PointLightIntensity, 0.01f, 0.0f, 10.0f);
		}

		ImGui::TreePop();
	}
	// スポットライト
	if (ImGui::TreeNode("SpotLight")) {
		ImGui::Checkbox("OnOff", &isSpotLight);
		if (isSpotLight) {
			ImGui::ColorEdit4("Color", &SpotLightColor.x);
			ImGui::DragFloat3("Position", &SpotLightPosition.x, 0.01f, -100.0f, 100.0f);
			ImGui::DragFloat3("Direction", &SpotLightDirection.x, 0.01f, -100.0f, 100.0f);
			ImGui::DragFloat("Range", &SpotLightRange, 0.01f, 0.0f, 100.0f);
			ImGui::DragFloat("Intensity", &SpotLightIntensity, 0.01f, 0.0f, 10.0f);
		}

		ImGui::TreePop();
	}

#pragma endregion

#pragma region ポストエフェクト
	// *ポストエフェクト* //
	ImGui::Text("PostEffect"); // ポストエフェクトのテキスト

	// 反転
	if (ImGui::TreeNode("inversion")) {
		ImGui::Checkbox("OnOff", &isInversion);

		ImGui::TreePop();
	}
	// グレースケール
	if (ImGui::TreeNode("grayscale")) {
		ImGui::Checkbox("OnOff", &isGrayscale);

		ImGui::TreePop();
	}
	// 放射線ブラー
	if (ImGui::TreeNode("radialBlur")) {
		ImGui::Checkbox("OnOff", &isRadialBlur);

		if (isRadialBlur) {
			ImGui::DragFloat2("blurCenter", &blurCenter.x, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("blurWidth", &blurWidth, 0.001f, 0.0f, 0.1f);
			ImGui::DragInt("blurSamples", &blurSamples, 1, 1, 100);
		}

		ImGui::TreePop();
	}
	// ディスタンスフォグ
	if (ImGui::TreeNode("distanceFog")) {
		ImGui::Checkbox("OnOff", &isDistanceFog);

		if (isDistanceFog) {
			ImGui::ColorEdit3("fogColor", &distanceFogColor.x);
			ImGui::DragFloat("fogStart", &distanceStart, 0.1f, 0.0f, 100.0f);
			ImGui::DragFloat("fogEnd", &distanceEnd, 0.1f, 0.0f, 100.0f);
		}

		ImGui::TreePop();
	}
	// ハイトフォグ
	if (ImGui::TreeNode("heightFog")) {
		ImGui::Checkbox("OnOff", &isHeightFog);

		if (isHeightFog) {
			ImGui::ColorEdit3("heightFogColor", &heightFogColor.x);
			ImGui::DragFloat("heightFogTop", &heightFogTop, 0.1f, -100.0f, 100.0f);
			ImGui::DragFloat("heightFogBottom", &heightFogBottom, 0.1f, -100.0f, 100.0f);
			ImGui::DragFloat("heightFogDensity", &heightFogDensity, 0.01f, 0.0f, 10.0f);
		}

		ImGui::TreePop();
	}
	// DOF
	if (ImGui::TreeNode("DOF")) {
		ImGui::Checkbox("OnOff", &isDOF);

		if (isDOF) {
			ImGui::DragFloat("focusDistance", &focusDistance, 0.1f, 0.0f, 100.0f);
			ImGui::DragFloat("bokehRadius", &bokehRadius, 0.1f, 0.0f, 100.0f);
			ImGui::DragFloat("focusRange", &focusRange, 0.1f, 0.0f, 100.0f);
		}

		ImGui::TreePop();
	}
	// ブルーム
	if (ImGui::TreeNode("Bloom")) {
		ImGui::DragFloat("bloomThreshold", &bloomThreshold, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("bloomIntensity", &bloomIntensity, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("bloomRadius", &bloomBlurRadius, 0.01f, 0.0f, 10.0f);

		ImGui::TreePop();
	}
	// レンズフレア
	if (ImGui::TreeNode("LensFlare")) {
		ImGui::Checkbox("OnOff", &isLensFlare);

		if (isLensFlare) {
			ImGui::DragInt("lensFlareGhostCount", &lensFlareGhostCount, 1, 0,10);
			ImGui::DragFloat("lensFlareHaloWidth", &lensFlareHaloWidth, 0.01f, 0.0f, 10.0f);
			ImGui::Checkbox("isACES", &isACES);
			ImGui::DragFloat("caIntensity", &caIntensity, 0.001f, 0.0f, 10.0f);
		}
		ImGui::Text("%.3f", PostEffect::GetInstance()->GetLensFlareGhostDispersal());

		ImGui::TreePop();
	}
	// モーションブラー
	if (ImGui::TreeNode("MotionBlur")) {
		ImGui::Checkbox("OnOff", &isMotionBlur);

		if (isLensFlare) {
			ImGui::DragInt("motionBlurSamples", &motionBlurSamples, 1, 0, 20);
			ImGui::DragFloat("motionBlurScale", &motionBlurScale, 0.01f, 0.0f, 10.0f);
		}

		ImGui::TreePop();
	}

#pragma endregion

#pragma region レイマーチング

	// レイマーチング
	//ImGui::DragFloat("rayMarchingTime", &rayMarchingTime, 0.1f,0.0f,10.0f);
	ImGui::DragFloat3("rayMarchingSunDir", &rayMarchingSunDir.x, 0.01f,-1.0f,1.0f);
	ImGui::DragFloat("rayMarchingCloudCoverage", &rayMarchingCloudCoverage, 0.01f,-5.0f,10.0f);
	ImGui::DragFloat("rayMarchingCloudBottom", &rayMarchingCloudBottom, 10.0f,-5000.0f,5000.0f);
	ImGui::DragFloat("rayMarchingCloudTop", &rayMarchingCloudTop, 10.0f, -5000.0f, 5000.0f);
	ImGui::Checkbox("rayMarchingIsRialLight", &rayMarchingIsRialLight);
	ImGui::Checkbox("rayMarchingIsAnimeLight", &rayMarchingIsAnimeLight);
	ImGui::Checkbox("rayMarchingIsMotionBlur", &rayMarchingIsMotionBlur);
	ImGui::DragFloat("rayMarchingCloudOpacity", &rayMarchingCloudOpacity, 0.001f, 0.0f, 0.1f);
	ImGui::Checkbox("isStorm", &isStorm);
	ImGui::DragFloat("thunderFrequency", &thunderFrequency, 0.001f, 0.0f, 10.0f);
	ImGui::DragFloat("thunderBrightness", &thunderBrightness, 0.01f, 0.0f, 300.0f);


#pragma endregion

#endif

}

void TitleScene::Draw2D() {
	// 2Dオブジェクトの描画準備
	SpriteCommon::GetInstance()->SetCommonPipelineState();

	// スプライト描画
	//sprite->Draw();
}
void TitleScene::Draw3D() {
	// スカイボックス
	//Skybox::GetInstance()->Draw();

	// 3Dオブジェクトの描画準備
	//ObjectCommon::GetInstance()->SetCommonPipelineState();

	
	// 3Dオブジェクト描画
	//object->Draw();

	// アニメーション
	//ObjectCommon::GetInstance()->SetAnimationPipelineState();

	// 3Dオブジェクト描画
	object->Draw();

	// アウトライン描画準備
	ObjectCommon::GetInstance()->SetOutlinePipelineState();

	// 線描画準備
	LineCommon::GetInstance()->SetCommonPipelineState();
	
	//line->Draw();
	
}

void TitleScene::Finalize() {
	CameraManager::GetInstance()->RemoveCamera("main");
}
