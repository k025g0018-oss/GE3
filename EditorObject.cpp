#include "EditorObject.h"

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"

namespace {
// PropertiesのComboで使用するTexture一覧
const char* kTextureModes[] = {
	"0 : No Texture (White)",
	"1 : UV Checker",
	"2 : Genbaneko",
	"3 : Monster Ball"
};
}

Primitive3DEditorObject::Primitive3DEditorObject(
	Primitive3D& object, int& textureMode, ParticleSystem& particleSystem)
	: object_(object), textureMode_(textureMode), particleSystem_(particleSystem) {}

void Primitive3DEditorObject::DrawProperties() {
	// Primitive3Dが所有するMaterialとTextureを編集する
	ImGui::TextUnformatted("Primitive3D Material");
	ImGui::ColorEdit4("Material Color", &object_.GetColor().x);
	ImGui::Combo("Texture Mode", &textureMode_, kTextureModes, IM_ARRAYSIZE(kTextureModes));

	// Primitive3Dの全表示モードでライティングを個別にON/OFFする
	bool enableLighting = object_.IsLightingEnabled();
	if (ImGui::Checkbox("Enable Lighting", &enableLighting)) {
		object_.SetLightingEnabled(enableLighting);
	}

	if (enableLighting) {
		const char* lightingModes[] = {
			"Lambert",
			"Half Lambert"
		};

		// Materialの値を直接変更し、シェーダーへ反映する
		ImGui::Combo(
			"Lighting Mode",
			&object_.GetLightingMode(),
			lightingModes,
			IM_ARRAYSIZE(lightingModes)
		);
	}

	ImGui::Separator();

	// 図形全体へ適用するTransformを編集する
	Transform& transform = object_.GetTransform();
	ImGui::SliderFloat3("Scale", &transform.scale.x, 0.1f, 10.0f);
	ImGui::SliderFloat3("Position", &transform.translate.x, -5.0f, 5.0f);

	// Startで回転を開始し、Stopで停止する。停止時の角度は保持する
	bool& isPlaying = object_.GetIsPlaying();
	if (ImGui::Button(isPlaying ? "Stop" : "Start")) {
		isPlaying = !isPlaying;
	}
	ImGui::SameLine();
	ImGui::Text("State: %s", isPlaying ? "Playing" : "Stopped");
	if (!isPlaying) {
		ImGui::SliderFloat3("Rotation", &transform.rotate.x, -3.1415f, 3.1415f);
	} else {
		ImGui::Text("Rotation: X %.2f / Y %.2f / Z %.2f", transform.rotate.x, transform.rotate.y, transform.rotate.z);
	}

	if (ImGui::Button("Reset All")) {
		// 3D状態と関連するParticleを初期状態へ戻す
		object_.Reset();
		particleSystem_.Reset();
	}
}

Sprite2DEditorObject::Sprite2DEditorObject(Sprite2D& sprite, int& textureMode, bool& isVisible)
	: sprite_(sprite), textureMode_(textureMode), isVisible_(isVisible) {}

void Sprite2DEditorObject::DrawProperties() {
	// Sprite2D専用のTexture、色、Transformを編集する
	ImGui::TextUnformatted("Sprite Control");
	// Spriteの描画だけを個別にON/OFFする
	ImGui::Checkbox("Draw Sprite", &isVisible_);
	ImGui::Combo("Sprite Texture Mode", &textureMode_, kTextureModes, IM_ARRAYSIZE(kTextureModes));
	ImGui::ColorEdit4("Sprite Material Color", &sprite_.GetColor().x);
	Transform& transform = sprite_.GetTransform();
	ImGui::DragFloat3("Sprite Scale", &transform.scale.x, 0.01f);
	ImGui::DragFloat3("Sprite Rotate", &transform.rotate.x, 0.01f);
	ImGui::DragFloat3("Sprite Translate", &transform.translate.x, 1.0f);
	if (ImGui::Button("Reset Sprite")) {
		sprite_.Reset();
	}

	Transform& uvTransform =
		sprite_.GetUVTransform();

	// UVの平行移動を編集する
	ImGui::DragFloat2(
		"UV Translate",
		&uvTransform.translate.x,
		0.01f,
		-10.0f,
		10.0f
	);

	// UVの拡大縮小を編集する
	ImGui::DragFloat2(
		"UV Scale",
		&uvTransform.scale.x,
		0.01f,
		-10.0f,
		10.0f
	);

	// UVのZ軸回転を編集する
	ImGui::SliderAngle(
		"UV Rotate",
		&uvTransform.rotate.z
	);
}

ParticleEditorObject::ParticleEditorObject(ParticleSystem& particleSystem)
	: particleSystem_(particleSystem) {}

void ParticleEditorObject::DrawProperties() {
	// ParticleSystemの現在の状態を表示し、必要な場合は初期化する
	ImGui::TextUnformatted("ParticleSystem");
	ImGui::Separator();
	ImGui::Text("Particle Count : %d", static_cast<int>(particleSystem_.GetParticleCount()));
	ImGui::Text("Max Particle Count : %d", static_cast<int>(particleSystem_.GetMaxParticleCount()));
	ImGui::Text("Total Vertex Count : %d", static_cast<int>(particleSystem_.GetTotalVertexCount()));
	if (ImGui::Button("Reset Particles")) {
		particleSystem_.Reset();
	}
	ImGui::TextDisabled("Detailed execution is shown in Flow Graph.");
}

SceneSettingsEditorObject::SceneSettingsEditorObject(Primitive3D& object) : object_(object) {}

void SceneSettingsEditorObject::DrawProperties() {
	// Primitive3DとParticleSystemを切り替える表示モード一覧
	const char* modes[] = {
		"0: None",
		"1: Single Triangle",
		"2: Double Triangles",
		"3: Single Pyramid",
		"4: Double Pyramids",
		"5: Production Mode"
	};
	int& displayMode = object_.GetDisplayMode();
	ImGui::Combo("Display Mode", &displayMode, modes, IM_ARRAYSIZE(modes));

	// 三角形2枚モードでは、各三角形のTransformを個別に表示する
	if (displayMode == 2) {
		ImGui::TextUnformatted("[Triangle 1]");
		ImGui::SliderFloat3("T1 Scale", object_.GetTriangle1Scale(), 0.1f, 5.0f);
		ImGui::SliderFloat3("T1 Rotation", object_.GetTriangle1Rotate(), -3.1415f, 3.1415f);
		ImGui::SliderFloat3("T1 Position", object_.GetTriangle1Translate(), -3.0f, 3.0f);
		ImGui::Separator();
		ImGui::TextUnformatted("[Triangle 2]");
		ImGui::SliderFloat3("T2 Scale", object_.GetTriangle2Scale(), 0.1f, 5.0f);
		ImGui::SliderFloat3("T2 Rotation", object_.GetTriangle2Rotate(), -3.1415f, 3.1415f);
		ImGui::SliderFloat3("T2 Position", object_.GetTriangle2Translate(), -3.0f, 3.0f);
	}
	// 三角錐2個モードでは、各三角錐のTransformを個別に表示する
	if (displayMode == 4) {
		ImGui::TextUnformatted("[Pyramid 1]");
		ImGui::SliderFloat3("P1 Scale", object_.GetPyramid1Scale(), 0.1f, 5.0f);
		ImGui::SliderFloat3("P1 Rotation", object_.GetPyramid1Rotate(), -3.1415f, 3.1415f);
		ImGui::SliderFloat3("P1 Position", object_.GetPyramid1Translate(), -3.0f, 3.0f);
		ImGui::Separator();
		ImGui::TextUnformatted("[Pyramid 2]");
		ImGui::SliderFloat3("P2 Scale", object_.GetPyramid2Scale(), 0.1f, 5.0f);
		ImGui::SliderFloat3("P2 Rotation", object_.GetPyramid2Rotate(), -3.1415f, 3.1415f);
		ImGui::SliderFloat3("P2 Position", object_.GetPyramid2Translate(), -3.0f, 3.0f);
	}
}

///// ----- Sphere ----- /////
SphereEditorObject::SphereEditorObject(
	Sphere& sphere,
	int& textureMode,
	bool& isVisible)
	: sphere_(sphere),
	textureMode_(textureMode),
	isVisible_(isVisible) {
}

void SphereEditorObject::DrawProperties() {
	ImGui::TextUnformatted("Sphere Control");
	// Sphereの描画だけを個別にON/OFFする
	ImGui::Checkbox("Draw Sphere", &isVisible_);

	// Sphereのライティングを個別に切り替える
	bool enableLighting =
		sphere_.IsLightingEnabled();

	if (enableLighting) {
		const char* lightingModes[] = {
			"Lambert",
			"Half Lambert"
		};

		// Sphereに使用するライティング方式を切り替える
		ImGui::Combo(
			"Sphere Lighting Mode",
			&sphere_.GetLightingMode(),
			lightingModes,
			IM_ARRAYSIZE(lightingModes)
		);
	}

	// Checkboxが変更されたときだけMaterialへ反映する
	if (ImGui::Checkbox(
		"Enable Lighting",
		&enableLighting
		)) {
		sphere_.SetLightingEnabled(enableLighting);
	}

	// Sphere専用のTexture番号をImGuiから変更する
	ImGui::Combo(
		"Sphere Texture Mode",
		&textureMode_,
		kTextureModes,
		IM_ARRAYSIZE(kTextureModes)
	);

	ImGui::Separator();

	// SphereのTransformも同じPropertiesから編集する
	Transform& transform = sphere_.GetTransform();

	ImGui::DragFloat3(
		"Sphere Scale",
		&transform.scale.x,
		0.01f
	);

	ImGui::DragFloat3(
		"Sphere Rotate",
		&transform.rotate.x,
		0.01f
	);

	ImGui::DragFloat3(
		"Sphere Translate",
		&transform.translate.x,
		0.01f
	);
}

///// ----- OBJ Model ----- /////
ModelEditorObject::ModelEditorObject(
	Model& model,
	const char* name,
	int& textureMode,
	bool& isVisible)
	: model_(model),
	name_(name),
	textureMode_(textureMode),
	isVisible_(isVisible) {
}

void ModelEditorObject::DrawProperties() {
	ImGui::TextUnformatted("OBJ Model Control");

	// OBJモデルの描画だけを個別にON/OFFする
	ImGui::Checkbox("Draw Model", &isVisible_);

	// モデルのライティングを個別にON/OFFする
	bool enableLighting =
		model_.IsLightingEnabled();

	if (ImGui::Checkbox(
		"Enable Lighting",
		&enableLighting
	)) {
		model_.SetLightingEnabled(enableLighting);
	}

	if (enableLighting) {
		const char* lightingModes[] = {
			"Lambert",
			"Half Lambert"
		};

		// ライティングが有効な場合だけ方式を選択できるようにする
		ImGui::Combo(
			"Lighting Mode",
			&model_.GetLightingMode(),
			lightingModes,
			IM_ARRAYSIZE(lightingModes)
		);
	}

	// OBJモデルへ貼るTextureを選択する
	ImGui::Combo(
		"Texture Mode",
		&textureMode_,
		kTextureModes,
		IM_ARRAYSIZE(kTextureModes)
	);

	ImGui::Separator();

	// モデルの拡縮・回転・移動をPropertiesから編集する
	Transform& transform =
		model_.GetTransform();

	ImGui::DragFloat3(
		"Scale",
		&transform.scale.x,
		0.01f
	);

	ImGui::DragFloat3(
		"Rotate",
		&transform.rotate.x,
		0.01f
	);

	ImGui::DragFloat3(
		"Translate",
		&transform.translate.x,
		0.01f
	);

	// OBJファイルから読み込んだ頂点数を確認できるようにする
	ImGui::Text(
		"Vertex Count: %u",
		model_.GetVertexCount()
	);
}

#endif // USE_IMGUI
