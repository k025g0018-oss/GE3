#include "EditorObject.h"

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"

namespace {
// PropertiesのComboで使用するTexture一覧
const char* kTextureModes[] = {
	"0 : No Texture (White)",
	"1 : UV Checker",
	"2 : Genbaneko"
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

Sprite2DEditorObject::Sprite2DEditorObject(Sprite2D& sprite, int& textureMode)
	: sprite_(sprite), textureMode_(textureMode) {}

void Sprite2DEditorObject::DrawProperties() {
	// Sprite2D専用のTexture、色、Transformを編集する
	ImGui::TextUnformatted("Sprite Control");
	ImGui::Combo("Sprite Texture Mode", &textureMode_, kTextureModes, IM_ARRAYSIZE(kTextureModes));
	ImGui::ColorEdit4("Sprite Material Color", &sprite_.GetColor().x);
	Transform& transform = sprite_.GetTransform();
	ImGui::DragFloat3("Sprite Scale", &transform.scale.x, 0.01f);
	ImGui::DragFloat3("Sprite Rotate", &transform.rotate.x, 0.01f);
	ImGui::DragFloat3("Sprite Translate", &transform.translate.x, 1.0f);
	if (ImGui::Button("Reset Sprite")) {
		sprite_.Reset();
	}
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

#endif // USE_IMGUI
