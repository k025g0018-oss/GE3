#include "Matrix4x4.h"
#include "Primitive3D.h"
#include "Vector.h"
#include "BufferResource.h"
#include "Collision.h"
#include "CommandContext.h"
#include "DepthStencilView.h"
#include "DescriptorHeap.h"
#include "EditorObject.h"
#include "Logger.h"
#include "ParticleSystem.h"
#include "PipelineState.h"
#include "SceneRenderTexture.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "VertexBuffer.h"
#include "Sphere.h"
#include "Camera.h"
#include "TransformationMatrix.h"
#include "DirectionalLight.h"
#include "Material.h"
#include "ModelLoader.h"
#include "Model.h"

#include <windows.h>
#include <cstdint> // int32_t
#include <string> // 文字列
#include <format>
#include <filesystem> // ファイルやディレクトリに関する操作を行うライブラリ
#include <fstream> // ファイルに書いたり読んだりするライブラリ
#include <chrono> // 時間を扱うライブラリ
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dbghelp.h> // Debug用のあれやこれやを使えるようにする
#include <strsafe.h> // StringCchPrintfWの利用に必要
#include <dxgidebug.h>
#include <dxcapi.h>
#include <filesystem> // フォルダとファイルを列挙するため
#include <string>     // ファイル名をstd::stringで扱うため
#include <system_error> // フォルダ列挙エラーを安全に受け取るため
#include <vector> // ImGuiが使用するSRV番号を管理するため
#include <cmath>
#include <cstring>

// ImGui
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_internal.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
#endif // USE_IMGUI

// libのリンク
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib") // Debug用のあれやこれやを使えるようにする
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

/// --- 関数の定義エリア ---
#pragma region
// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	// ImGui
#if USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif // USE_IMGUI

	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		// ウィンドウが破棄された
		case WM_DESTROY:
			// OSに対して、アプリの終了を伝える
			PostQuitMessage(0);
			return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

// CrashHandlerの登録
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	// Dumpを出力する
	// 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpsディレクトリ以下に出力
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = {0};
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
		time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	// processId(このexeのId)とクラッシュ(例外)の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();
	// 設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{0};
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	// Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
	// 他に関連付けられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する。
	return EXCEPTION_EXECUTE_HANDLER;
}

///// ----- ImGuiパネル ----- /////
#ifdef USE_IMGUI
// Hierarchyで現在選択されている項目を表す
// Hierarchyの選択は共通ラッパーのポインターで管理する

// Dear ImGui 1.92以降が必要とするSRVの確保と解放を管理する
struct ImGuiSrvDescriptorAllocator {
	const DescriptorHeap* descriptorHeap = nullptr;
	std::vector<uint32_t> freeIndices;

	// ゲーム用SRV(1～4番)と衝突しないようにヒープ後半をImGui専用にする
	void Initialize(const DescriptorHeap& heap, uint32_t firstIndex) {
		descriptorHeap = &heap;
		for (uint32_t index = heap.GetDescriptorCount(); index > firstIndex; --index) {
			freeIndices.push_back(index - 1);
		}
	}

	void Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle) {
		assert(!freeIndices.empty());
		const uint32_t index = freeIndices.back();
		freeIndices.pop_back();
		*cpuHandle = descriptorHeap->GetCPUHandle(index);
		*gpuHandle = descriptorHeap->GetGPUHandle(index);
	}

	void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
		const SIZE_T cpuOffset = cpuHandle.ptr - descriptorHeap->GetCPUHandleStart().ptr;
		const UINT64 gpuOffset = gpuHandle.ptr - descriptorHeap->GetGPUHandleStart().ptr;
		const uint32_t cpuIndex = static_cast<uint32_t>(cpuOffset / descriptorHeap->GetDescriptorSize());
		const uint32_t gpuIndex = static_cast<uint32_t>(gpuOffset / descriptorHeap->GetDescriptorSize());
		assert(cpuIndex == gpuIndex);
		freeIndices.push_back(cpuIndex);
	}
};

// 起動時にViewportの比率からDock配置を作り、ウィンドウサイズ変更にも追従させる
void SetupDefaultDockLayout(ImGuiID dockspaceId, const ImVec2& viewportSize) {
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, viewportSize);

	ImGuiID centerDockId = dockspaceId;
	ImGuiID leftDockId = 0;
	ImGuiID rightDockId = 0;
	ImGuiID bottomDockId = 0;

	// 左18%、右22%、下25%を補助パネルとして確保し、中央をSceneにする
	ImGui::DockBuilderSplitNode(centerDockId, ImGuiDir_Left, 0.18f, &leftDockId, &centerDockId);
	ImGui::DockBuilderSplitNode(centerDockId, ImGuiDir_Right, 0.22f, &rightDockId, &centerDockId);
	ImGui::DockBuilderSplitNode(centerDockId, ImGuiDir_Down, 0.25f, &bottomDockId, &centerDockId);

	ImGuiID leftBottomDockId = 0;
	ImGuiID rightBottomDockId = 0;
	ImGui::DockBuilderSplitNode(leftDockId, ImGuiDir_Down, 0.50f, &leftBottomDockId, &leftDockId);
	ImGui::DockBuilderSplitNode(rightDockId, ImGuiDir_Down, 0.50f, &rightBottomDockId, &rightDockId);

	ImGui::DockBuilderDockWindow("Hierarchy", leftDockId);
	ImGui::DockBuilderDockWindow("Flow Graph", leftBottomDockId);
	ImGui::DockBuilderDockWindow("Scene", centerDockId);
	ImGui::DockBuilderDockWindow("Content Browser", bottomDockId);
	ImGui::DockBuilderDockWindow("Properties", rightDockId);
	ImGui::DockBuilderDockWindow("Statistics", rightBottomDockId);
	ImGui::DockBuilderFinish(dockspaceId);
}

// 選択結果をPropertiesへ渡せるように参照で受け取る
void DrawHierarchy(const std::vector<IEditorObject*>& editorObjects, IEditorObject*& selectedObject) {
	ImGui::Begin("Hierarchy");
	for (IEditorObject* editorObject : editorObjects) {
		// ラッパーを一覧へ追加するだけでHierarchyにも反映される
		if (ImGui::Selectable(editorObject->GetName(), selectedObject == editorObject)) {
			selectedObject = editorObject;
		}
	}
	ImGui::End();
}

#if 0
// 旧SelectedObject列挙型を使用する場合の名前変換処理として残す
const char* GetSelectedObjectName(SelectedObject selectedObject) {
	switch (selectedObject) {
		case SelectedObject::Pyramid:
			return "3D Object (Triangle / Pyramid)";
		case SelectedObject::Sprite:
			return "Sprite";
		case SelectedObject::ParticleSystem:
			return "ParticleSystem";
		case SelectedObject::SceneSettings:
			return "Scene Settings";
		default:
			return "None";
	}
}
#endif

// Scene用レンダーテクスチャをパネル内へ表示する カメラ操作
bool DrawScene(
	D3D12_GPU_DESCRIPTOR_HANDLE sceneSrvHandle,
	const IEditorObject* selectedObject
) {
	ImGui::Begin("Scene");

	ImGui::Text(
		"Selected : %s",
		selectedObject
		? selectedObject->GetName()
		: "None"
	);

	const ImVec2 panelSize =
		ImGui::GetContentRegionAvail();

	if (panelSize.x > 0.0f &&
		panelSize.y > 0.0f) {

		const ImTextureRef sceneTexture(
			static_cast<ImTextureID>(
			sceneSrvHandle.ptr
		)
		);

		ImGui::Image(
			sceneTexture,
			panelSize
		);
	}

	// Sceneウィンドウ上だけカメラ操作を有効にする
	const bool isSceneHovered =
		ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
		);

	ImGui::End();

	return isSceneHovered;
}

// Particle Flow内の1ノードを描画する
void DrawParticleFlowNode(
	ImDrawList* drawList,
	const ImVec2& position,
	const ImVec2& size,
	const char* label,
	bool active,
	bool eventActive = false
) {
	const ImU32 backgroundColor = eventActive
		? IM_COL32(205, 115, 35, 255)
		: active ? IM_COL32(50, 145, 90, 255) : IM_COL32(55, 60, 70, 255);
	const ImU32 borderColor = active || eventActive
		? IM_COL32(245, 245, 245, 255)
		: IM_COL32(105, 110, 120, 255);

	drawList->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y), backgroundColor, 7.0f);
	drawList->AddRect(position, ImVec2(position.x + size.x, position.y + size.y), borderColor, 7.0f, 0, 2.0f);
	const ImVec2 textSize = ImGui::CalcTextSize(label);
	drawList->AddText(
		ImVec2(position.x + (size.x - textSize.x) * 0.5f, position.y + (size.y - textSize.y) * 0.5f),
		IM_COL32(255, 255, 255, 255),
		label
	);
}

// ノード間の処理順を矢印で描画する
void DrawParticleFlowArrow(ImDrawList* drawList, const ImVec2& from, const ImVec2& to, bool active) {
	const ImU32 color = active ? IM_COL32(95, 225, 140, 255) : IM_COL32(115, 120, 130, 255);
	drawList->AddLine(from, to, color, active ? 3.0f : 2.0f);
	const float direction = to.x >= from.x ? 1.0f : -1.0f;
	drawList->AddTriangleFilled(
		to,
		ImVec2(to.x - 8.0f * direction, to.y - 5.0f),
		ImVec2(to.x - 8.0f * direction, to.y + 5.0f),
		color
	);
}

// ParticleSystemの実行順と直近イベントを読み取り専用で可視化する
void DrawParticleFlow(const ParticleSystem& particleSystem) {
	ImGui::Begin("Particle Flow");
	const ParticleSystem::DebugFlowState& state = particleSystem.GetDebugFlowState();

	ImGui::Text(
		"Particles : %d / %d",
		static_cast<int>(particleSystem.GetParticleCount()),
		static_cast<int>(particleSystem.GetMaxParticleCount())
	);
	ImGui::Text("Collision : %u    Spawn : %u", state.collisionCount, state.spawnCount);
	ImGui::Text(
		"Last Event : %s",
		state.resetOccurred ? "Reset" : state.spawnCount > 0 ? "Spawn" : state.collisionCount > 0 ? "Collision" : "Update"
	);
	ImGui::Separator();

	const ImVec2 canvasStart = ImGui::GetCursorScreenPos();
	const ImVec2 nodeSize(125.0f, 44.0f);
	const float gap = 32.0f;
	const float rowGap = 52.0f;
	const ImVec2 updatePos = canvasStart;
	const ImVec2 movePos(updatePos.x + nodeSize.x + gap, updatePos.y);
	const ImVec2 collisionPos(movePos.x + nodeSize.x + gap, movePos.y);
	const ImVec2 reflectPos(collisionPos.x + nodeSize.x + gap, collisionPos.y);
	const ImVec2 spawnPos(reflectPos.x, reflectPos.y + nodeSize.y + rowGap);
	const ImVec2 maxCheckPos(collisionPos.x, spawnPos.y);
	const ImVec2 resetPos(movePos.x, spawnPos.y);

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	DrawParticleFlowArrow(drawList, ImVec2(updatePos.x + nodeSize.x, updatePos.y + 22.0f), ImVec2(movePos.x, movePos.y + 22.0f), state.moveExecuted);
	DrawParticleFlowArrow(drawList, ImVec2(movePos.x + nodeSize.x, movePos.y + 22.0f), ImVec2(collisionPos.x, collisionPos.y + 22.0f), state.collisionChecked);
	DrawParticleFlowArrow(drawList, ImVec2(collisionPos.x + nodeSize.x, collisionPos.y + 22.0f), ImVec2(reflectPos.x, reflectPos.y + 22.0f), state.collisionCount > 0);
	DrawParticleFlowArrow(drawList, ImVec2(reflectPos.x + nodeSize.x * 0.5f, reflectPos.y + nodeSize.y), ImVec2(spawnPos.x + nodeSize.x * 0.5f, spawnPos.y), state.spawnCount > 0);
	DrawParticleFlowArrow(drawList, ImVec2(spawnPos.x, spawnPos.y + 22.0f), ImVec2(maxCheckPos.x + nodeSize.x, maxCheckPos.y + 22.0f), state.updateExecuted);
	DrawParticleFlowArrow(drawList, ImVec2(maxCheckPos.x, maxCheckPos.y + 22.0f), ImVec2(resetPos.x + nodeSize.x, resetPos.y + 22.0f), state.resetOccurred);

	DrawParticleFlowNode(drawList, updatePos, nodeSize, "1. Update", state.updateExecuted);
	DrawParticleFlowNode(drawList, movePos, nodeSize, "2. Move / Rotate", state.moveExecuted);
	DrawParticleFlowNode(drawList, collisionPos, nodeSize, "3. Wall Check", state.collisionChecked);
	DrawParticleFlowNode(drawList, reflectPos, nodeSize, "4. Reflect", state.collisionCount > 0, state.collisionCount > 0);
	DrawParticleFlowNode(drawList, spawnPos, nodeSize, "5. Spawn", state.spawnCount > 0, state.spawnCount > 0);
	DrawParticleFlowNode(drawList, maxCheckPos, nodeSize, "6. Max Check", state.updateExecuted);
	DrawParticleFlowNode(drawList, resetPos, nodeSize, "7. Reset", state.resetOccurred, state.resetOccurred);

	ImGui::Dummy(ImVec2((nodeSize.x + gap) * 4.0f, nodeSize.y * 2.0f + rowGap + 10.0f));
	ImGui::End();
}

// ノードをInvisibleButtonとして登録し、ドラッグ操作で位置を更新する
void DrawDraggableNode(const char* id, const char* label, ImVec2& position, bool active) {
	const ImVec2 nodeSize(135.0f, 46.0f);
	ImGui::SetCursorScreenPos(position);
	ImGui::PushID(id);
	ImGui::InvisibleButton("Node", nodeSize);
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
		position.x += ImGui::GetIO().MouseDelta.x;
		position.y += ImGui::GetIO().MouseDelta.y;
	}
	const ImU32 color = active ? IM_COL32(45, 145, 90, 255) : IM_COL32(55, 60, 70, 255);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(position, ImVec2(position.x + nodeSize.x, position.y + nodeSize.y), color, 7.0f);
	drawList->AddRect(position, ImVec2(position.x + nodeSize.x, position.y + nodeSize.y), IM_COL32(220, 220, 225, 255), 7.0f, 0, 2.0f);
	const ImVec2 textSize = ImGui::CalcTextSize(label);
	drawList->AddText(ImVec2(position.x + (nodeSize.x - textSize.x) * 0.5f, position.y + 14.0f), IM_COL32_WHITE, label);
	ImGui::PopID();
}

void DrawFlowConnection(const ImVec2& fromNode, const ImVec2& toNode, bool active) {
	const ImVec2 nodeSize(135.0f, 46.0f);
	DrawParticleFlowArrow(
		ImGui::GetWindowDrawList(),
		ImVec2(fromNode.x + nodeSize.x, fromNode.y + nodeSize.y * 0.5f),
		ImVec2(toNode.x, toNode.y + nodeSize.y * 0.5f), active);
}

void DrawFlowGraph(const ParticleSystem& particleSystem, const Primitive3D& primitive3D) {
	ImGui::Begin("Flow Graph");
	ImGui::TextDisabled("Drag nodes with the left mouse button. Connections visualize processing order.");
	if (ImGui::BeginTabBar("FlowTabs")) {
		if (ImGui::BeginTabItem("ParticleSystem")) {
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			static ImVec2 positions[] = {{20, 35}, {190, 35}, {360, 35}, {530, 35}, {530, 125}, {360, 125}, {190, 125}};
			ImVec2 p[7];
			for (int i = 0; i < 7; ++i) {
				p[i] = ImVec2(origin.x + positions[i].x, origin.y + positions[i].y);
			}
			const auto& state = particleSystem.GetDebugFlowState();
			DrawDraggableNode("ParticleUpdate", "1. Update", p[0], state.updateExecuted);
			DrawDraggableNode("ParticleMove", "2. Move / Rotate", p[1], state.moveExecuted);
			DrawDraggableNode("ParticleWall", "3. Wall Check", p[2], state.collisionChecked);
			DrawDraggableNode("ParticleReflect", "4. Reflect", p[3], state.collisionCount > 0);
			DrawDraggableNode("ParticleSpawn", "5. Spawn", p[4], state.spawnCount > 0);
			DrawDraggableNode("ParticleMax", "6. Max Check", p[5], state.updateExecuted);
			DrawDraggableNode("ParticleReset", "7. Reset", p[6], state.resetOccurred);
			for (int i = 0; i < 7; ++i) {
				positions[i] = ImVec2(p[i].x - origin.x, p[i].y - origin.y);
			}
			for (int i = 0; i < 6; ++i) {
				DrawFlowConnection(p[i], p[i + 1], i < 2 ? state.updateExecuted : state.collisionCount > 0);
			}
			ImGui::Dummy(ImVec2(700.0f, 210.0f));
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("3D Object")) {
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			static ImVec2 positions[] = {{20, 55}, {190, 55}, {360, 55}, {530, 55}};
			ImVec2 p[4];
			for (int i = 0; i < 4; ++i) {
				p[i] = ImVec2(origin.x + positions[i].x, origin.y + positions[i].y);
			}
			DrawDraggableNode("ObjectDisplay", "Display", p[0], true);
			DrawDraggableNode("ObjectStart", "Start", p[1], primitive3D.IsPlaying());
			DrawDraggableNode("ObjectMove", "Move / Rotate", p[2], primitive3D.IsPlaying());
			DrawDraggableNode("ObjectStop", "Stop", p[3], !primitive3D.IsPlaying());
			for (int i = 0; i < 4; ++i) {
				positions[i] = ImVec2(p[i].x - origin.x, p[i].y - origin.y);
			}
			DrawFlowConnection(p[0], p[1], true);
			DrawFlowConnection(p[1], p[2], primitive3D.IsPlaying());
			DrawFlowConnection(p[2], p[3], !primitive3D.IsPlaying());
			ImGui::Dummy(ImVec2(700.0f, 160.0f));
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("2D Sprite")) {
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			static ImVec2 positions[] = {{20, 55}, {210, 55}, {400, 55}};
			ImVec2 p[3];
			for (int i = 0; i < 3; ++i) {
				p[i] = ImVec2(origin.x + positions[i].x, origin.y + positions[i].y);
			}
			DrawDraggableNode("SpriteInitialize", "Initialize", p[0], true);
			DrawDraggableNode("SpriteUpdate", "Update", p[1], true);
			DrawDraggableNode("SpriteDraw", "Draw / Display", p[2], true);
			for (int i = 0; i < 3; ++i) {
				positions[i] = ImVec2(p[i].x - origin.x, p[i].y - origin.y);
			}
			DrawFlowConnection(p[0], p[1], true);
			DrawFlowConnection(p[1], p[2], true);
			ImGui::Dummy(ImVec2(600.0f, 160.0f));
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
}

// 描画統計を表示する
void DrawStatistics(
	const ParticleSystem& particleSystem,
	Camera& camera,
	bool& isPrimitive3DVisible,
	bool& isParticleVisible,
	bool& isSphereVisible,
	bool& isPlaneVisible,
	bool& isAxisVisible,
	bool& isMultiMeshVisible,
	bool& isMultiMaterialVisible,
	bool& isSpriteVisible
) {
	ImGui::Begin("Statistics");

	ImGui::Text(
		"FPS : %.1f",
		ImGui::GetIO().Framerate
	);

	ImGui::Text(
		"Particle Count : %d",
		static_cast<int>(
		particleSystem.GetParticleCount()
	)
	);

	ImGui::Text(
		"Total Vertex Count : %d",
		static_cast<int>(
		particleSystem.GetTotalVertexCount()
	)
	);

	ImGui::Separator();

	// すべての個別Drawを有効にする
	if (ImGui::Button("Show All Objects")) {
		isPrimitive3DVisible = true;
		isParticleVisible = true;
		isSphereVisible = true;
		isPlaneVisible = true;
		isAxisVisible = true;
		isMultiMeshVisible = true;
		isMultiMaterialVisible = true;
		isSpriteVisible = true;
	}

	ImGui::SameLine();

	// すべての個別Drawを無効にする
	if (ImGui::Button("Hide All Objects")) {
		isPrimitive3DVisible = false;
		isParticleVisible = false;
		isSphereVisible = false;
		isPlaneVisible = false;
		isAxisVisible = false;
		isMultiMeshVisible = false;
		isMultiMaterialVisible = false;
		isSpriteVisible = false;
	}

	ImGui::Separator();

	// Cameraは頻繁に確認するためStatisticsへ常時表示する
	ImGui::TextUnformatted("Camera");

	Transform& cameraTransform =
		camera.GetTransform();

	ImGui::DragFloat3(
		"Camera Translate",
		&cameraTransform.translate.x,
		0.01f
	);

	ImGui::DragFloat3(
		"Camera Rotate",
		&cameraTransform.rotate.x,
		0.01f
	);

	if (ImGui::Button("Reset Camera")) {
		camera.Reset();
	}

	ImGui::TextDisabled(
		"Scene Left Drag : Rotate"
	);
	ImGui::TextDisabled(
		"Scene Right Drag : Move"
	);
	ImGui::TextDisabled(
		"Scene Wheel : Zoom"
	);

	ImGui::End();
}

// 指定されたフォルダの中身だけを再帰的に表示する
void DrawDirectoryTree(const std::filesystem::path& directory) {
	std::error_code error;

	for (const auto& entry :
		std::filesystem::directory_iterator(directory, error)) {

		if (error) {
			ImGui::Text("Failed to read directory.");
			return;
		}

		const std::string name = entry.path().filename().string();

		if (entry.is_directory()) {
			// フォルダをツリーとして開閉可能にする
			if (ImGui::TreeNode(name.c_str())) {
				DrawDirectoryTree(entry.path());
				ImGui::TreePop();
			}
		} else {
			// ファイルを選択可能な項目として表示する
			ImGui::Selectable(name.c_str());
		}
	}
}

// Content Browserウィンドウは毎フレーム1回だけ作る
void DrawContentBrowser() {
	ImGui::Begin("Content Browser");

	const std::filesystem::path resourceDirectory = "resources";

	if (std::filesystem::exists(resourceDirectory)) {
		DrawDirectoryTree(resourceDirectory);
	} else {
		ImGui::Text("resources folder was not found.");
	}

	ImGui::End();
}

// TextureManagerへ読み込み済みのPNGなら、対応するSRVを返す
bool TryGetLoadedTexture(
	const std::filesystem::path& path,
	const TextureManager& textureManager,
	D3D12_GPU_DESCRIPTOR_HANDLE& srvHandle
) {
	const std::string fileName = path.filename().string();
	if (fileName == "white.png") {
		srvHandle = textureManager.GetSrvHandle(0);
		return true;
	}
	if (fileName == "uvChecker.png") {
		srvHandle = textureManager.GetSrvHandle(1);
		return true;
	}
	if (fileName == "genbaneko.png") {
		srvHandle = textureManager.GetSrvHandle(2);
		return true;
	}
	return false;
}

// 拡張子に合わせた紙アイコンをDear ImGuiの図形で描く
void DrawDocumentIcon(const char* id, const char* text, ImU32 color, const ImVec2& size) {
	ImGui::InvisibleButton(id, size);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 min = ImGui::GetItemRectMin();
	const ImVec2 max = ImGui::GetItemRectMax();
	const float foldSize = size.x * 0.24f;

	drawList->AddRectFilled(min, max, color, 5.0f);
	drawList->AddTriangleFilled(
		ImVec2(max.x - foldSize, min.y),
		ImVec2(max.x, min.y + foldSize),
		ImVec2(max.x - foldSize, min.y + foldSize),
		IM_COL32(235, 235, 235, 255)
	);

	const ImVec2 textSize = ImGui::CalcTextSize(text);
	drawList->AddText(
		ImVec2(min.x + (size.x - textSize.x) * 0.5f, min.y + (size.y - textSize.y) * 0.58f),
		IM_COL32(255, 255, 255, 255),
		text
	);
}

// フォルダまたはファイルの種類に合ったアイコンを表示する
bool DrawContentBrowserIcon(
	const std::filesystem::directory_entry& entry,
	const TextureManager& textureManager,
	const ImVec2& iconSize
) {
	D3D12_GPU_DESCRIPTOR_HANDLE textureHandle{};
	if (!entry.is_directory() && TryGetLoadedTexture(entry.path(), textureManager, textureHandle)) {
		const ImTextureRef texture(static_cast<ImTextureID>(textureHandle.ptr));
		return ImGui::ImageButton("##TextureThumbnail", texture, iconSize);
	}

	const std::string extension = entry.path().extension().string();
	if (entry.is_directory()) {
		DrawDocumentIcon("##FolderIcon", "DIR", IM_COL32(210, 160, 45, 255), iconSize);
	} else if (extension == ".h" || extension == ".hpp") {
		DrawDocumentIcon("##HeaderIcon", "H", IM_COL32(70, 130, 210, 255), iconSize);
	} else if (extension == ".cpp") {
		DrawDocumentIcon("##CppIcon", "C++", IM_COL32(80, 95, 180, 255), iconSize);
	} else if (extension == ".hlsl" || extension == ".hlsli") {
		DrawDocumentIcon("##ShaderIcon", "HLSL", IM_COL32(145, 75, 180, 255), iconSize);
	} else if (extension == ".png") {
		DrawDocumentIcon("##PngIcon", "PNG", IM_COL32(75, 155, 100, 255), iconSize);
	} else {
		DrawDocumentIcon("##FileIcon", "FILE", IM_COL32(105, 110, 120, 255), iconSize);
	}
	return ImGui::IsItemClicked();
}

// ソリューション内のフォルダとファイルを、アイコン付きで表示する
void DrawContentBrowserAssets(const TextureManager& textureManager) {
	ImGui::Begin("Content Browser");

	static std::filesystem::path currentDirectory = ".";
	static std::filesystem::path selectedPath;

	if (ImGui::Button("Up") && currentDirectory != ".") {
		currentDirectory = currentDirectory.parent_path();
		if (currentDirectory.empty()) {
			currentDirectory = ".";
		}
	}
	ImGui::SameLine();
	const std::string currentPathText = currentDirectory.lexically_normal().string();
	ImGui::TextUnformatted(currentPathText.c_str());
	ImGui::Separator();

	const float previewWidth = 230.0f;
	ImGui::BeginChild("ContentFiles", ImVec2(-previewWidth, 0.0f), ImGuiChildFlags_Borders);

	std::error_code error;
	std::vector<std::filesystem::directory_entry> entries;
	for (const auto& entry : std::filesystem::directory_iterator(currentDirectory, error)) {
		entries.push_back(entry);
	}

	if (error) {
		ImGui::Text("Failed to read directory.");
	} else {
		const float tileWidth = 110.0f;
		const ImVec2 iconSize(72.0f, 72.0f);
		const int columnCount = static_cast<int>(ImGui::GetContentRegionAvail().x / tileWidth);
		const int safeColumnCount = columnCount > 0 ? columnCount : 1;
		int column = 0;

		for (const auto& entry : entries) {
			const std::string fullPath = entry.path().string();
			const std::string fileName = entry.path().filename().string();
			ImGui::PushID(fullPath.c_str());
			ImGui::BeginGroup();

			const bool clicked = DrawContentBrowserIcon(entry, textureManager, iconSize);
			if (clicked) {
				selectedPath = entry.path();
			}
			if (entry.is_directory() && ImGui::IsItemHovered() &&
				ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				currentDirectory = entry.path();
				selectedPath.clear();
			}

			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + tileWidth - 8.0f);
			ImGui::TextWrapped("%s", fileName.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndGroup();
			ImGui::PopID();

			++column;
			if (column < safeColumnCount) {
				ImGui::SameLine();
			} else {
				column = 0;
			}
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("ContentPreview", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
	ImGui::TextUnformatted("Preview");
	ImGui::Separator();
	if (!selectedPath.empty()) {
		const std::string selectedName = selectedPath.filename().string();
		const std::string selectedExtension = selectedPath.extension().string();
		ImGui::TextWrapped("%s", selectedName.c_str());
		ImGui::TextDisabled("%s", selectedExtension.c_str());

		D3D12_GPU_DESCRIPTOR_HANDLE textureHandle{};
		if (TryGetLoadedTexture(selectedPath, textureManager, textureHandle)) {
			const ImTextureRef texture(static_cast<ImTextureID>(textureHandle.ptr));
			const float imageWidth = ImGui::GetContentRegionAvail().x;
			ImGui::Image(texture, ImVec2(imageWidth, imageWidth));
		} else {
			ImGui::Spacing();
			const std::string previewText = selectedExtension.empty() ? "DIR" : selectedExtension;
			DrawDocumentIcon(
				"##PreviewIcon",
				previewText.c_str(),
				IM_COL32(90, 110, 150, 255),
				ImVec2(96.0f, 96.0f)
			);
		}
	}
	ImGui::EndChild();

	ImGui::End();
}

#endif

#pragma endregion 関数の定義エリア

/// --- メイン処理 ---
// windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	// COMの初期化
	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	assert(SUCCEEDED(hr));

	// 誰も捕捉しなかった場合に(Unhandled)、捕捉する関数を登録
	// main関数が始まってすぐに登録
	SetUnhandledExceptionFilter(ExportDump);

	// ログのディレクトリを用意
	std::filesystem::create_directory("logs");

	// 現在時刻を取得 (UTC時刻)
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	// ログファイルの名前にコンマ何秒はいらないので、削って秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	// 日本時間 (PCの設定時間) に変換
	std::chrono::zoned_time localTime{std::chrono::current_zone(), nowSeconds};
	// formatを使って年月日_時分秒の文字列に変換
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	// 時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + dateString + ".log";
	// ファイルを作って書き込み準備
	std::ofstream logStream(logFilePath);

	Log(logStream, "ぶっ飛ばすぜべいべ");

	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello,DirectX!\n");

	WNDCLASS wc{};
	// ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;
	// ウィンドウクラス名(何でもよい)
	wc.lpszClassName = L"CG2WindowClass";
	// インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	// カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// ウィンドウクラスを登録する
	RegisterClass(&wc);

	// クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = {0, 0, kClientWidth, kClientHeight};

	// クライアント領域をもとに実際のサイズにwrcを変更してもらう
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName, // 利用するクラス名
		L"CG2", // タイトルバーの文字
		WS_OVERLAPPEDWINDOW, // ウィンドウスタイル
		CW_USEDEFAULT, // 表示X座標(Windowsに任せる)
		CW_USEDEFAULT, // 表示Y座標(windowsOSに任せる)
		wrc.right - wrc.left, // ウィンドウ横幅
		wrc.bottom - wrc.top, // ウィンドウ縦幅
		nullptr, // 親ウィンドウハンドル
		nullptr, // メニューハンドル
		wc.hInstance, // インスタンスハンドル
		nullptr // オプション
	);

	// ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);

	/// --- DebugLayer ---

#ifdef _DEBUG
	ID3D12Debug1* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		// デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		// さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif // _DEBUG

	/// --- DXGI初期化 ---

	// DXGIファクトリーの生成
	IDXGIFactory7* dxgiFactory = nullptr;

	// HRESULTはWindowsケイのエラーコード、関数が成功したかどうかをSUCCEEDEDマクロで判定できる
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));

	// 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、どうにもできない場合はassertにしておく
	assert(SUCCEEDED(hr));

	// 使用するアダプタ用の変数、最初にnullptr
	IDXGIAdapter4* useAdapter = nullptr;

	// 良い順にアダプタを頼む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
		// アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr)); // 取得できないのは一大事

		// ソフトウェアアダプタでなければ採用
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			// 採用したアダプタの情報をログに出力。wstringの方なので注意
			Log(logStream, std::format(L"Use Adapter:{}\n", adapterDesc.Description));
			break;
		}
		useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
	}

	// 適切なアダプタが見つからなかったので起動できない
	assert(useAdapter != nullptr);

	// --- D3D12Deviceの生成 ---
	ID3D12Device* device = nullptr;
	// 機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[]{
		D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = {"12.2", "12.1", "12.0"};
	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		// 採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
		// 指定した機能レベルでデバイスが生成できたかを確認
		if (SUCCEEDED(hr)) {
			// 生成できたのでログ出力を行ってループを抜ける
			Log(logStream, std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break; // 生成できたらループを抜ける
		}
	}

	// デバイスの生成がうまくいかなかったので起動できない
	assert(device != nullptr);
	Log(logStream, "Complete create D3D12Device!!!\n"); // 初期化完了のログを出す

	// エラー・警告を実行時にプログラムを停止させる、deviceに対して行う
#ifdef _DEBUG
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		// やばいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		// エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// 警告時に止まる(ここをコメントアウトしたら全部の情報が出力される、詳細な情報をログに出力することができる)
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		// エラーと警告の抑制
		// 抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			// windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バクによるエラーメッセージ
			// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		// 抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[]
			= {D3D12_MESSAGE_SEVERITY_INFO};
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		// 指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);

		// 解放
		infoQueue->Release();
	}
#endif // _DEBUG

	///// ----- CommandContext ----- /////

	/// --- 初期化 ---
	// コマンドの記録と実行を管理する
	CommandContext commandContext;
	commandContext.Initialize(device);

	// 描画コマンドを積むCommandListを取得する
	ID3D12GraphicsCommandList* commandList = commandContext.GetCommandList();

	/// --- SwapChainの生成 ---
	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth; // 画面の幅。ウィンドウのクライアント領域を同じものにしておく。
	swapChainDesc.Height = kClientHeight; // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく。
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2; // ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニターにうつしたら、中身を廃棄
	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する。
	hr = dxgiFactory->CreateSwapChainForHwnd(commandContext.GetCommandQueue(), hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
	assert(SUCCEEDED(hr));

	/// --- DescriptorHeapの生成 ---
	// RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse
	DescriptorHeap rtvDescriptorHeap;
	rtvDescriptorHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	// SRV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
	DescriptorHeap srvDescriptorHeap;
	srvDescriptorHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	/// --- DepthStencilView ---
	// DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleはfalse
	DescriptorHeap dsvDescriptorHeap;
	dsvDescriptorHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	/// --- SwapChainからResourceを引っ張ってくる ---
	ID3D12Resource* swapChainResources[2] = {nullptr};
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	// --- RTVを作る ---
	// RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2dテクスチャとして書き込む
	// ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap.GetCPUHandleStart();
	// RTVを2つ作るのでディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	// まず1つ目を作る。1つ目は最初のところに作る。作る場所をこちらで指定してあげる必要がある。
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandles[0]);
	// 2つ目のディスクリプタハンドルを得る(自力で)
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//2つ目を作る
	device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandles[1]);

	///// ----- PSO(Pipeline State Object) ----- /////

	/// --- 初期化 ---
	// RootSignatureと各設定をまとめてPSOを作成する
	PipelineState pipelineState;
	pipelineState.Initialize(device, logStream);

	// WVP用のリソースを作る、Matrix4x4 １つ分のサイズを用意する
	ID3D12Resource* wvpResource = BufferResource::Create(device, sizeof(TransformationMatrix));
	// データを書き込む
	TransformationMatrix* wvpData = nullptr;
	// 書き込むためのアドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	// 単位行列を書き込んでおく
	wvpData->WVP = Matrix4x4::MakeIdentity4x4();
	wvpData->World = Matrix4x4::MakeIdentity4x4();

	///// ----- VertexBuffer ----- /////

	/// --- 初期化 ---
	// VertexResourceを生成する
	// 頂点数の数
	// 最大頂点数
	const uint32_t kMaxVertexCount = 1024;
	VertexBuffer vertexBuffer;
	vertexBuffer.Initialize(device, kMaxVertexCount);

	///// ----- Sprite ----- /////

	///// ----- 初期化 ----- /////
	/// --- Sprite専用のVertexBuffer、Material、WVPを作成 ---
	// 2D専用オブジェクトであることが分かる名前に統一する
	Sprite2D sprite2D;
	sprite2D.Initialize(device, kClientWidth, kClientHeight, 640.0f, 360.0f);

	// 三角形とは別にSpriteのTextureを選択する
	int spriteTextureMode = 1;
	// Spriteの描画を個別に切り替える
	bool isSpriteVisible = true;

	// Primitive3Dの描画を個別に切り替える
	bool isPrimitive3DVisible = true;

	// Particleの描画を個別に切り替える
	bool isParticleVisible = true;

	// ライティング全体の有効状態を保持する
	bool isAllLightingEnabled = true;

	// ライティング全体へ適用する方式を保持する
	int allLightingMode = 0;

	/// --- Material用のリソースを作る ---
	ID3D12Resource* materialResource = BufferResource::Create(device, sizeof(Material));
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 色書き込み
	// 基本色を白にする
	materialData->color =
	{1.0f, 1.0f, 1.0f, 1.0f};
	// main側の頂点法線が未設定なので一旦無効にする
	materialData->enableLighting = false;
	// 初期状態ではUV座標を変化させない
	materialData->uvTransform =
		Matrix4x4::MakeIdentity4x4();

	/// --- 平行光源用の定数バッファを作成する ---
	ID3D12Resource* directionalLightResource =
		BufferResource::Create(device, sizeof(DirectionalLight));
	// CPUから光源情報を書き込むアドレス
	DirectionalLight* directionalLightData = nullptr;
	HRESULT directionalLightMapResult =
		directionalLightResource->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&directionalLightData)
		);

	assert(SUCCEEDED(directionalLightMapResult));

	// デフォルト値
	// 平行光源の色を白に設定する
	directionalLightData->color = {1.0f, 1.0f, 1.0f, 1.0f};
	// 真下へ進む単位ベクトルを設定する
	directionalLightData->direction = {0.0f, -1.0f, 0.0f};
	// 平行光源の明るさを設定する
	directionalLightData->intensity = 1.0f;

	/// --- Resourceのデータを書き込む ---
	// データを書き込む
	VertexData* vertexData = vertexBuffer.GetData();
	// 書き込むためのアドレスを取得
	// VertexBufferの初期化時にMapしたアドレスを使用する

	// 三角錐を構成する
	VertexData pyramidVertices[12] = {
		// 前面
		{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, // 左下
		{{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, // 頂点
		{{0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}}, // 右下

		// 右側面
		{{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, // 右下
		{{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, // 頂点
		{{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}}, // 奥

		// 左側面
		{{0.0f, -0.5f, 0.5f, 1.0f}, {0.0f, 1.0f}}, // 奥
		{{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, // 頂点
		{{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}}, // 左下

		// 底面
		{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, // 左前
		{{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}}, // 右前
		{{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}}, // 奥
	};

	// データをGPUリソースへ書き込む(for文でコピー)
	for (uint32_t i = 0; i < 12; ++i) {
		vertexData[i] = pyramidVertices[i];
	}

	///// ----- TextureManager ----- /////

	/// --- 初期化 ---
	// テクスチャの切り替え用
	int textureMode = 0;

	// Textureを読み込み、GPUへの転送とSRVの作成を行う
	TextureManager textureManager;
	textureManager.Initialize(device, commandList, srvDescriptorHeap);

	///// ----- DSV(Depth Stencil View) ----- /////

	/// --- 初期化 ---
	// 深度ステンシルテクスチャリソースとDSVを作る
	DepthStencilView depthStencilView;
	depthStencilView.Initialize(device, dsvDescriptorHeap, kClientWidth, kClientHeight);

	/// --- Texture転送コマンドの実行 ---
	// コマンドを実行してGPUの完了を待つ
	commandContext.ExecuteAndWait();

	// 転送が終わったのでソースは解放する
	// GPUへの転送が終わったので転送用リソースを解放する
	textureManager.ReleaseIntermediateResources();

	/// --- ViewportとScissor ---
	// ビューポート
	D3D12_VIEWPORT viewport{};
	// クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = kClientWidth;
	viewport.Height = kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// シザー矩形
	D3D12_RECT scissorRect{};
	// ビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;

	// ImGuiを描画するSwapChain側は、実際のウィンドウサイズへ追従させる
	uint32_t backBufferWidth = kClientWidth;
	uint32_t backBufferHeight = kClientHeight;
	D3D12_VIEWPORT editorViewport = viewport;
	D3D12_RECT editorScissorRect = scissorRect;

	///// ----- 文字列 ----- /////
	// メッセージ構造体
	MSG msg{};

	///// ----- 変数の宣言 ----- /////
	/// 三角形
	// Transformの変数を作る
	// 3Dオブジェクトの状態をObject3Dへ集約し、既存の描画処理から参照して使う
	Primitive3D primitive3D;
	primitive3D.Initialize(device);
	Transform& transform = primitive3D.GetTransform();

	// カメラの回転
	bool& isAutoRotate = primitive3D.GetIsPlaying();

	// 描画モード
	int& displayMode = primitive3D.GetDisplayMode();

	// モード1で使う
	float* t1_Scale = primitive3D.GetTriangle1Scale();
	float* t1_Rotate = primitive3D.GetTriangle1Rotate();
	float* t1_Translate = primitive3D.GetTriangle1Translate();

	float* t2_Scale = primitive3D.GetTriangle2Scale();
	float* t2_Rotate = primitive3D.GetTriangle2Rotate();
	float* t2_Translate = primitive3D.GetTriangle2Translate();

	// モード3で使う
	// 三角錐1個目
	float* p1_Scale = primitive3D.GetPyramid1Scale();
	float* p1_Rotate = primitive3D.GetPyramid1Rotate();
	float* p1_Translate = primitive3D.GetPyramid1Translate();
	// 三角錐2個目
	float* p2_Scale = primitive3D.GetPyramid2Scale();
	float* p2_Rotate = primitive3D.GetPyramid2Rotate();
	float* p2_Translate = primitive3D.GetPyramid2Translate();

	///// ----- ParticleSystem ----- /////

	/// --- 初期化 ---
	// 演出モード4で使用する範囲と最大数を設定する
	ParticleSystem particleSystem;
	particleSystem.Initialize(device, -0.9f, 0.9f, 50);

	/// --- カメラ ---
	Camera camera;

	camera.Initialize(
		float(kClientWidth) /
		float(kClientHeight)
	);

	// Object3D用のWorld行列
	Matrix4x4 worldMatrix =
		Matrix4x4::MakeAffineMatrix(
			transform.scale,
			transform.rotate,
			transform.translate
		);

	///// ----- 球 (Sphere) ----- /////
	Sphere sphere;

	// ImGuiでは1～32分割まで変更できるようにする
	sphere.Initialize(device, 32);

	// Sphereのテクスチャ切り替え用の選択番号
	int sphereTextureMode = 3;
	// Sphereの描画を個別に切り替える
	bool isSphereVisible = true;

	///// ----- Plane OBJ Model ----- /////

	Model planeModel;

	// resourcesフォルダのplane.objを読み込む
	planeModel.Initialize(
		device,
		"resources",
		"plane.obj"
	);

	// 片面モデルの表側が初期状態でカメラを向くようにする
	planeModel.GetTransform().rotate.y = 3.141592f;

	// 平面モデルの表示を切り替える
	bool isPlaneVisible = true;

	// MTLで指定されたテクスチャに対応する番号を取得する
	int planeTextureMode =
		static_cast<int>(
			textureManager.FindTextureIndex(
			planeModel.GetTextureFilePath()
			)
			);

	///// ----- Axis OBJ Model ----- /////

	Model axisModel;

	// resourcesフォルダのaxis.objを読み込む
	axisModel.Initialize(
		device,
		"resources",
		"axis.obj"
	);

	// Axisモデルの表示を切り替える
	bool isAxisVisible = true;

	// axis.mtlで指定されたテクスチャ番号を取得する
	int axisTextureMode =
		static_cast<int>(
			textureManager.FindTextureIndex(
			axisModel.GetTextureFilePath()
			)
			);

	///// ----- Multi Mesh OBJ Model ----- /////

	Model multiMeshModel;

	// 複数Meshが含まれているOBJファイルを読み込む
	multiMeshModel.Initialize(
		device,
		"resources",
		"multiMesh.obj"
	);

	// ちょいずらす
	multiMeshModel.GetTransform().translate.x = 3.0f;

	// MultiMeshモデルの表示を切り替える
	bool isMultiMeshVisible = true;

	// MTLで指定されたテクスチャ番号を取得する
	int multiMeshTextureMode =
		static_cast<int>(
			textureManager.FindTextureIndex(
			multiMeshModel.GetTextureFilePath()
			)
			);

	///// ----- Multi Material OBJ Model ----- /////

	Model multiMaterialModel;

	// 複数Materialを使用するOBJファイルを読み込む
	multiMaterialModel.Initialize(
		device,
		"resources",
		"multiMaterial.obj"
	);

	// 他のモデルと重ならない位置へ移動する
	multiMaterialModel.GetTransform().translate.x =
		-3.0f;

	// MultiMaterialモデルの表示を切り替える
	bool isMultiMaterialVisible = true;

	// Propertiesとの互換性用に先頭のTexture番号を取得する
	int multiMaterialTextureMode =
		static_cast<int>(
			textureManager.FindTextureIndex(
			multiMaterialModel.GetTextureFilePath()
			)
			);

	///// ----- ImGuiの初期化 ----- /////
#ifdef USE_IMGUI
	// Texture用SRVは1番から始まるため、その直後をScene用にする
	constexpr uint32_t kTextureSrvStartIndex = 1;
	constexpr uint32_t kSceneSrvDescriptorIndex =
		kTextureSrvStartIndex + TextureManager::kTextureCount;

	SceneRenderTexture sceneRenderTexture;
	sceneRenderTexture.Initialize(
		device,
		srvDescriptorHeap,
		kSceneSrvDescriptorIndex,
		kClientWidth,
		kClientHeight,
		rtvDesc.Format
	);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();

	// Docking対応版へ更新した後に有効化する
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);

	// Dear ImGuiのDirectX 12初期化情報をまとめる
	ImGui_ImplDX12_InitInfo initInfo{};
	initInfo.Device = device;
	initInfo.CommandQueue = commandContext.GetCommandQueue();
	initInfo.NumFramesInFlight = swapChainDesc.BufferCount;
	initInfo.RTVFormat = rtvDesc.Format;
	initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
	initInfo.SrvDescriptorHeap = srvDescriptorHeap.Get();

	// Dear ImGuiにはゲーム用SRVと重ならない64番以降を割り当てる
	ImGuiSrvDescriptorAllocator imguiSrvAllocator;
	imguiSrvAllocator.Initialize(srvDescriptorHeap, 64);
	initInfo.UserData = &imguiSrvAllocator;
	initInfo.SrvDescriptorAllocFn = [](
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle
	) {
		auto* allocator = static_cast<ImGuiSrvDescriptorAllocator*>(info->UserData);
		allocator->Allocate(cpuHandle, gpuHandle);
		};
	initInfo.SrvDescriptorFreeFn = [](
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle
	) {
		auto* allocator = static_cast<ImGuiSrvDescriptorAllocator*>(info->UserData);
		allocator->Free(cpuHandle, gpuHandle);
		};

	// 新しいDirectX 12バックエンド初期化形式を使用する
	ImGui_ImplDX12_Init(&initInfo);

	io.Fonts->Build();

	/// --- ヒエラルキーだお ---

	// ゲームクラスをImGuiへ直接依存させず、Editor用ラッパーを介して表示する
	Primitive3DEditorObject primitive3DEditor(
		primitive3D,
		textureMode,
		particleSystem,
		isPrimitive3DVisible
	);
	Sprite2DEditorObject sprite2DEditor(sprite2D, spriteTextureMode, isSpriteVisible);
	ParticleEditorObject particleEditor(
		particleSystem,
		isParticleVisible
	);
	SceneSettingsEditorObject sceneSettingsEditor(primitive3D);

	// SphereとSphere専用Texture番号をEditorへ接続する
	SphereEditorObject sphereEditor(sphere, sphereTextureMode, isSphereVisible);

	// OBJ平面と平面専用の表示設定をEditorへ接続する
	ModelEditorObject planeModelEditor(
		planeModel,
		"Plane Model",
		planeTextureMode,
		isPlaneVisible
	);

	// AxisモデルとAxis専用の表示設定をEditorへ接続する
	ModelEditorObject axisModelEditor(
		axisModel,
		"Axis Model",
		axisTextureMode,
		isAxisVisible
	);

	// MultiMeshモデルをHierarchyとPropertiesへ接続する
	ModelEditorObject multiMeshModelEditor(
		multiMeshModel,
		"Multi Mesh Model",
		multiMeshTextureMode,
		isMultiMeshVisible
	);

	// MultiMaterialモデルをHierarchyとPropertiesへ接続する
	ModelEditorObject multiMaterialModelEditor(
		multiMaterialModel,
		"Multi Material Model",
		multiMaterialTextureMode,
		isMultiMaterialVisible
	);

	/// --- hierarchyの項目 ---

	std::vector<IEditorObject*> editorObjects = {
		// Primitive3Dの直後に、関連するSceneのモード設定を並べる
		&primitive3DEditor,
		&sceneSettingsEditor,
		&particleEditor,

		// 独立した描画オブジェクトはHierarchyの後ろへ並べる
		// 3D
		&sphereEditor,
		&planeModelEditor,
		&axisModelEditor,
		&multiMeshModelEditor,
		&multiMaterialModelEditor,

		// 2D
		&sprite2DEditor
	};
	IEditorObject* selectedObject = nullptr;

#endif // USE_IMGUI

	///// ----- メインループ ----- /////
	// ウィンドウのxボタンが押されるまでループ
	while (msg.message != WM_QUIT) {
		//windowにメッセージが来てたら最優先で処理させる
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			// WindowsがBackBuffer全体を拡大縮小しないよう、クライアントサイズに合わせて再作成する
			RECT clientRect{};
			GetClientRect(hwnd, &clientRect);
			const uint32_t clientWidth = static_cast<uint32_t>(clientRect.right - clientRect.left);
			const uint32_t clientHeight = static_cast<uint32_t>(clientRect.bottom - clientRect.top);
			if (clientWidth == 0 || clientHeight == 0) {
				continue;
			}

			if (clientWidth != backBufferWidth || clientHeight != backBufferHeight) {
				// ResizeBuffers前にGPUのBackBuffer参照を完了させる
				commandContext.ExecuteAndWait();
				for (ID3D12Resource*& backBuffer : swapChainResources) {
					if (backBuffer != nullptr) {
						backBuffer->Release();
						backBuffer = nullptr;
					}
				}

				HRESULT resizeResult = swapChain->ResizeBuffers(
					swapChainDesc.BufferCount,
					clientWidth,
					clientHeight,
					swapChainDesc.Format,
					0
				);
				assert(SUCCEEDED(resizeResult));

				for (uint32_t index = 0; index < swapChainDesc.BufferCount; ++index) {
					resizeResult = swapChain->GetBuffer(index, IID_PPV_ARGS(&swapChainResources[index]));
					assert(SUCCEEDED(resizeResult));
					device->CreateRenderTargetView(swapChainResources[index], &rtvDesc, rtvHandles[index]);
				}
				backBufferWidth = clientWidth;
				backBufferHeight = clientHeight;
				editorViewport.Width = static_cast<float>(clientWidth);
				editorViewport.Height = static_cast<float>(clientHeight);
				editorScissorRect.right = static_cast<LONG>(clientWidth);
				editorScissorRect.bottom = static_cast<LONG>(clientHeight);
				camera.SetAspectRatio(static_cast<float>(clientWidth) / static_cast<float>(clientHeight));
			}

			///// ----- ImGui先頭 ----- /////
#ifdef USE_IMGUI
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// メインViewport全体をDock領域にして、最大化やサイズ変更へ追従させる
			const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
			const ImGuiID dockspaceId = ImGui::GetID("MainEditorDockSpace");

			// 保存されたDock配置がない場合だけ、デフォルト配置を作成する
			if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
				SetupDefaultDockLayout(
					dockspaceId,
					mainViewport->Size
				);
			}

			// Dockノードを組み直したあとにViewportへ登録する
			ImGui::DockSpaceOverViewport(dockspaceId, mainViewport);

			ImGui::Begin("Directional Light");

			// ライティングを使用する全オブジェクトへまとめて反映する
			if (ImGui::Checkbox(
				"Enable All Lighting",
				&isAllLightingEnabled
			)) {
				primitive3D.SetLightingEnabled(
					isAllLightingEnabled
				);
				sphere.SetLightingEnabled(
					isAllLightingEnabled
				);
				planeModel.SetLightingEnabled(
					isAllLightingEnabled
				);
				axisModel.SetLightingEnabled(
					isAllLightingEnabled
				);
				multiMeshModel.SetLightingEnabled(
					isAllLightingEnabled
				);
				multiMaterialModel.SetLightingEnabled(
					isAllLightingEnabled
				);

				// ONにしたときは選択中の方式も全体へ適用する
				if (isAllLightingEnabled) {
					primitive3D.GetLightingMode() =
						allLightingMode;
					sphere.GetLightingMode() =
						allLightingMode;
					planeModel.GetLightingMode() =
						allLightingMode;
					axisModel.GetLightingMode() =
						allLightingMode;
					multiMeshModel.GetLightingMode() =
						allLightingMode;
					multiMaterialModel.GetLightingMode() =
						allLightingMode;
				}
			}

			if (isAllLightingEnabled) {
				const char* allLightingModes[] = {
					"Lambert",
					"Half Lambert"
				};

				// 全オブジェクトへ同じライティング方式を適用する
				if (ImGui::Combo(
					"All Lighting Mode",
					&allLightingMode,
					allLightingModes,
					IM_ARRAYSIZE(allLightingModes)
				)) {
					primitive3D.GetLightingMode() =
						allLightingMode;
					sphere.GetLightingMode() =
						allLightingMode;
					planeModel.GetLightingMode() =
						allLightingMode;
					axisModel.GetLightingMode() =
						allLightingMode;
					multiMeshModel.GetLightingMode() =
						allLightingMode;
					multiMaterialModel.GetLightingMode() =
						allLightingMode;
				}
			}

			ImGui::Separator();

			// 光源色を変更する
			ImGui::ColorEdit4(
				"Light Color",
				&directionalLightData->color.x
			);

			// 光の進む方向を変更する
			const bool directionChanged = ImGui::DragFloat3(
				"Light Direction",
				&directionalLightData->direction.x,
				0.01f,
				-1.0f,
				1.0f
			);

			// 光源の輝度を変更する
			ImGui::DragFloat(
				"Light Intensity",
				&directionalLightData->intensity,
				0.01f,
				0.0f,
				10.0f
			);

			// 方向が変更されたら必ず単位ベクトルへ正規化する
			if (directionChanged) {
				Vector3& direction = directionalLightData->direction;

				const float length = std::sqrt(
					direction.x * direction.x +
					direction.y * direction.y +
					direction.z * direction.z
				);

				// ゼロベクトルは正規化できないため除外する
				if (length > 0.0001f) {
					direction.x /= length;
					direction.y /= length;
					direction.z /= length;
				} else {
					// 不正な方向になった場合は真下へ戻す
					direction = {0.0f, -1.0f, 0.0f};
				}
			}

			ImGui::End();

			DrawHierarchy(editorObjects, selectedObject);

			const bool isSceneHovered =
				DrawScene(
					sceneRenderTexture.GetSRVHandle(),
					selectedObject
				);

			if (isSceneHovered) {
				ImGuiIO& io = ImGui::GetIO();

				const Vector2 mouseDelta = {
					io.MouseDelta.x,
					io.MouseDelta.y
				};

				// Scene上の左ドラッグでCameraを回転する
				if (ImGui::IsMouseDragging(
					ImGuiMouseButton_Left
					)) {
					camera.RotateByMouse(mouseDelta);
				}

				// Scene上の右ドラッグでCameraを平行移動する
				if (ImGui::IsMouseDragging(
					ImGuiMouseButton_Right
					)) {
					camera.MoveByMouse(mouseDelta);
				}

				// Scene上のホイールでCameraを前後移動する
				if (io.MouseWheel != 0.0f) {
					camera.ZoomByMouse(io.MouseWheel);
				}
			}

			DrawContentBrowserAssets(textureManager);
			DrawStatistics(
				particleSystem,
				camera,
				isPrimitive3DVisible,
				isParticleVisible,
				isSphereVisible,
				isPlaneVisible,
				isAxisVisible,
				isMultiMeshVisible,
				isMultiMaterialVisible,
				isSpriteVisible
			);
			DrawFlowGraph(particleSystem, primitive3D);

#endif // USE_IMGUI

			///// ----- ゲームの処理 ----- /////
			/// --- カメラ ---
			camera.Update();

			// 回転角を更新
			// Start中だけObject3D自身が回転状態を更新する
			// Primitive3Dの更新はCamera行列取得後に行う

			/// --- モードに応じた頂点データの書き込み ---
			// ImGuiで切り替えても、現在のフレームは同じモードで更新と描画を行う
			const int renderingMode = displayMode;
			// Primitive3Dへ移動する前の頂点生成処理は、比較できるよう残して無効化する
#if 0
			uint32_t drawVertexCount = 3; // デフォルト

			if (renderingMode == 0) {
				// None：Object3Dの頂点を作成しない
				drawVertexCount = 0;

			} else if (renderingMode == 1) {
				// 1: 三角形1枚
				drawVertexCount = 3;
				VertexData triangleVertices[3] = {
					{{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, // 上
					{{0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // 右下
					{{-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // 左下
				};
				for (uint32_t i = 0; i < drawVertexCount; ++i) {
					vertexData[i] = triangleVertices[i];
				}
			} else if (renderingMode == 2) {
				// 2: 三角形2枚 (個別SRT)
				drawVertexCount = 6;
				// 三角形1個目
				vertexData[0].position = {-0.5f, -0.5f, 0.0f, 1.0f};
				vertexData[0].texcoord = {0.0f, 1.0f};
				vertexData[1].position = {0.0f, 0.5f, 0.0f, 1.0f};
				vertexData[1].texcoord = {0.5f, 0.0f};
				vertexData[2].position = {0.5f, -0.5f, 0.0f, 1.0f};
				vertexData[2].texcoord = {1.0f, 1.0f};

				// 三角形2個目
				vertexData[3].position = {-0.5f, -0.5f, 0.5f, 1.0f};
				vertexData[3].texcoord = {0.0f, 1.0f};
				vertexData[4].position = {0.0f, 0.0f, 0.0f, 1.0f};
				vertexData[4].texcoord = {0.5f, 0.0f};
				vertexData[5].position = {0.5f, -0.5f, -0.5f, 1.0f};
				vertexData[5].texcoord = {1.0f, 1.0f};

				// 1枚目の三角形の変形計算
				for (uint32_t i = 0; i < 3; ++i) {
					VertexData v = vertexData[i];

					float x = v.position.x * t1_Scale[0];
					float y = v.position.y * t1_Scale[1];
					float z = v.position.z * t1_Scale[2];

					// X軸回転
					float cosX = cosf(t1_Rotate[0]); float sinX = sinf(t1_Rotate[0]);
					float dy = y * cosX - z * sinX; float dz = y * sinX + z * cosX;
					y = dy; z = dz;

					// Y軸回転
					float cosY = cosf(t1_Rotate[1]); float sinY = sinf(t1_Rotate[1]);
					float dx = x * cosY + z * sinY; dz = -x * sinY + z * cosY;
					x = dx; z = dz;

					// Z軸回転
					float cosZ = cosf(t1_Rotate[2]); float sinZ = sinf(t1_Rotate[2]);
					dx = x * cosZ - y * sinZ; dy = x * sinZ + y * cosZ;
					x = dx; y = dy;

					// 平行移動
					v.position.x = x + t1_Translate[0];
					v.position.y = y + t1_Translate[1];
					v.position.z = z + t1_Translate[2];

					// 計算結果を上書き保存
					vertexData[i] = v;
				}

				// 2枚目の三角形の変形計算
				for (uint32_t i = 0; i < 3; ++i) {
					// インデックスを「i + 3」にする
					VertexData v = vertexData[i + 3];

					float x = v.position.x * t2_Scale[0];
					float y = v.position.y * t2_Scale[1];
					float z = v.position.z * t2_Scale[2];

					// X軸回転
					float cosX = cosf(t2_Rotate[0]); float sinX = sinf(t2_Rotate[0]);
					float dy = y * cosX - z * sinX; float dz = y * sinX + z * cosX;
					y = dy; z = dz;

					// Y軸回転
					float cosY = cosf(t2_Rotate[1]); float sinY = sinf(t2_Rotate[1]);
					float dx = x * cosY + z * sinY; dz = -x * sinY + z * cosY;
					x = dx; z = dz;

					// Z軸回転
					float cosZ = cosf(t2_Rotate[2]); float sinZ = sinf(t2_Rotate[2]);
					dx = x * cosZ - y * sinZ; dy = x * sinZ + y * cosZ;
					x = dx; y = dy;

					// 平行移動
					v.position.x = x + t2_Translate[0];
					v.position.y = y + t2_Translate[1];
					v.position.z = z + t2_Translate[2];

					// 計算結果を上書き保存
					vertexData[i + 3] = v;
				}
			} else if (renderingMode == 3) {
				// 3: 三角錐1個
				drawVertexCount = 12;
				VertexData pyramidVertices[12] = {
					{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
					{{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}}, {{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
					{{0.0f, -0.5f, 0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
					{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}}, {{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
				};
				for (uint32_t i = 0; i < drawVertexCount; ++i) {
					vertexData[i] = pyramidVertices[i];
				}
			} else if (renderingMode == 4) {
				// 4: 三角錐2個 (個別SRT)
				drawVertexCount = 24;
				VertexData basePyramid[12] = {
					{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
					{{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
					{{0.0f, -0.5f, 0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.0f}}, {{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 1.0f}},
					{{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f}}, {{0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f}}, {{0.0f, -0.5f, 0.5f, 1.0f}, {1.0f, 1.0f}},
				};

				// 1個目の三角錐の変形計算
				for (uint32_t i = 0; i < 12; ++i) {
					VertexData v = basePyramid[i];
					float x = v.position.x * p1_Scale[0];
					float y = v.position.y * p1_Scale[1];
					float z = v.position.z * p1_Scale[2];
					// X軸回転
					float cosX = cosf(p1_Rotate[0]); float sinX = sinf(p1_Rotate[0]);
					float dy = y * cosX - z * sinX; float dz = y * sinX + z * cosX;
					y = dy; z = dz;
					// Y軸回転
					float cosY = cosf(p1_Rotate[1]); float sinY = sinf(p1_Rotate[1]);
					float dx = x * cosY + z * sinY; dz = -x * sinY + z * cosY;
					x = dx; z = dz;
					// Z軸回転
					float cosZ = cosf(p1_Rotate[2]); float sinZ = sinf(p1_Rotate[2]);
					dx = x * cosZ - y * sinZ; dy = x * sinZ + y * cosZ;
					x = dx; y = dy;
					// 平行移動
					v.position.x = x + p1_Translate[0];
					v.position.y = y + p1_Translate[1];
					v.position.z = z + p1_Translate[2];
					vertexData[i] = v;
				}

				// 2個目の三角錐の変形計算
				for (uint32_t i = 0; i < 12; ++i) {
					VertexData v = basePyramid[i];
					float x = v.position.x * p2_Scale[0];
					float y = v.position.y * p2_Scale[1];
					float z = v.position.z * p2_Scale[2];
					// X軸回転
					float cosX = cosf(p2_Rotate[0]); float sinX = sinf(p2_Rotate[0]);
					float dy = y * cosX - z * sinX; float dz = y * sinX + z * cosX;
					y = dy; z = dz;
					// Y軸回転
					float cosY = cosf(p2_Rotate[1]); float sinY = sinf(p2_Rotate[1]);
					float dx = x * cosY + z * sinY; dz = -x * sinY + z * cosY;
					x = dx; z = dz;
					// Z軸回転
					float cosZ = cosf(p2_Rotate[2]); float sinZ = sinf(p2_Rotate[2]);
					dx = x * cosZ - y * sinZ; dy = x * sinZ + y * cosZ;
					x = dx; y = dy;
					// 平行移動
					v.position.x = x + p2_Translate[0];
					v.position.y = y + p2_Translate[1];
					v.position.z = z + p2_Translate[2];
					vertexData[i + 12] = v;
				}
			} else if (renderingMode == 5) {
				// 5: 演出モード

				// Particle用の三角形をVertexBufferへ書き込む
				particleSystem.WriteTriangleVertices(vertexData);

				// 移動、回転、壁反射、Particleの追加を行う
				particleSystem.Update();
			}

#endif
			// Production Modeだけは既存のParticleSystem用頂点バッファを使用する
			if (renderingMode == 5) {
				particleSystem.WriteTriangleVertices(vertexData);
				particleSystem.Update();
			}

			///// ----- 行列の計算 ----- /////
			// ワールド行列の更新
			worldMatrix = Matrix4x4::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);

			// ビュー行列の更新
			const Matrix4x4& viewMatrix =
				camera.GetViewMatrix();

			// プロジェクション行列の更新
			const Matrix4x4& projectionMatrix =
				camera.GetProjectionMatrix();

			// 三角形・三角錐の頂点と専用WVPはPrimitive3D自身が更新する
			primitive3D.Update(viewMatrix, projectionMatrix);

			// Sphere専用のWVPを更新する
			sphere.Update(
				viewMatrix,
				projectionMatrix
			);

			// OBJ平面専用のWVPを更新する
			planeModel.Update(
				viewMatrix,
				projectionMatrix
			);

			// Axisモデル専用のWVPを更新する
			axisModel.Update(
				viewMatrix,
				projectionMatrix
			);

			// MultiMeshモデル専用のWVPを更新する
			multiMeshModel.Update(
				viewMatrix,
				projectionMatrix
			);

			// MultiMaterialモデル専用のWVPを更新する
			multiMaterialModel.Update(
				viewMatrix,
				projectionMatrix
			);

			// Object3D用WVPを計算する
			// ワールド、ビュー、プロジェクションを掛け合わせる
			Matrix4x4 worldViewProjectionMatrix = Matrix4x4::Multiply(worldMatrix, Matrix4x4::Multiply(viewMatrix, projectionMatrix));

			// wvp行列をGPUに送る
			// 頂点座標をクリップ空間へ変換する行列を送る
			wvpData->WVP = worldViewProjectionMatrix;

			// ライティングで法線を変換するWorld行列を送る
			wvpData->World = worldMatrix;

			///// ----- ImGui中身 ----- /////
#ifdef USE_IMGUI
	// 開発用UIの処理。実際に開発用のUIを出す場合はここをゲーム固有の処理に置き換える
	// Dear ImGui標準機能を確認したいときだけ、次の行を有効にする
	// ImGui::ShowDemoWindow();

	// 旧Windowパネルの編集項目をPropertiesへまとめる
			ImGui::Begin("Properties");
			if (selectedObject) {
				selectedObject->DrawProperties();
			} else {
				ImGui::TextDisabled("Select an item in Hierarchy.");
			}
			ImGui::End();

			// 以前の直書きUIはラッパー移行内容を確認できるよう残す
#if 0
			ImGui::Begin("Properties (Legacy)");
			const char* textureModes[] = {
				"0 : No Texture (White)",
				"1 : UV Checker",
				"2 : Genbaneko"
			};

			/// --- 色変えれます ---
			if (selectedObject == SelectedObject::Pyramid) {
				ImGui::Text("3D Object Material");
				ImGui::ColorEdit4("Material Color", &materialData->x);

				// 区切り線
				ImGui::Separator();

				/// --- 画像変えれます ---
				// テクスチャ切り替え
				const char* textureModes[] = {
					"0 : No Texture (White)",
					"1 : UV Checker",
					"2 : Genbaneko"
				};

				ImGui::Combo(
					"Texture Mode",
					&textureMode,
					textureModes,
					IM_ARRAYSIZE(textureModes)
				);

				// 区切り線
				ImGui::Separator();
			}

			if (selectedObject == SelectedObject::Sprite) {

				///// ----- Sprite ----- /////
				/// --- Texture ---
				// 三角形とは別にSpriteのTextureを切り替える
				ImGui::Text("Sprite Control");
				ImGui::Combo(
					"Sprite Texture Mode",
					&spriteTextureMode,
					textureModes,
					IM_ARRAYSIZE(textureModes)
				);

				/// --- 色 ---
				// Sprite専用のMaterial Colorを変更する
				ImGui::ColorEdit4("Sprite Material Color", &sprite2D.GetColor().x);

				/// --- SRT ---
				// Sprite専用のScale、Rotate、Translateを変更する
				Transform& spriteTransform = sprite2D.GetTransform();
				ImGui::DragFloat3("Sprite Scale", &spriteTransform.scale.x, 0.01f);
				ImGui::DragFloat3("Sprite Rotate", &spriteTransform.rotate.x, 0.01f);
				ImGui::DragFloat3("Sprite Translate", &spriteTransform.translate.x, 1.0f);

				// Spriteだけを初期状態へ戻す
				if (ImGui::Button("Reset Sprite")) {
					sprite2D.Reset();
				}

				// 区切り線
				ImGui::Separator();
			}

			if (selectedObject == SelectedObject::SceneSettings) {
				/// --- モード切り替えを切り替えだドン ---
				const char* modes[] = {
					"0: None",
					"1: Single Triangle",
					"2: Double Triangles",
					"3: Single Pyramid",
					"4: Double Pyramids",
					"5: Production Mode"
				};
				ImGui::Combo("Display Mode", &displayMode, modes, IM_ARRAYSIZE(modes));

				// モード1(三角形2枚)の個別SRTスライダー
				if (displayMode == 1) {
					ImGui::Text("[Triangle 1]");
					ImGui::SliderFloat3("T1 Scale", t1_Scale, 0.1f, 5.0f);
					ImGui::SliderFloat3("T1 Rotation", t1_Rotate, -3.1415f, 3.1415f);
					ImGui::SliderFloat3("T1 Position", t1_Translate, -3.0f, 3.0f);

					ImGui::Separator();

					ImGui::Text("[Triangle 2]");
					ImGui::SliderFloat3("T2 Scale", t2_Scale, 0.1f, 5.0f);
					ImGui::SliderFloat3("T2 Rotation", t2_Rotate, -3.1415f, 3.1415f);
					ImGui::SliderFloat3("T2 Position", t2_Translate, -3.0f, 3.0f);

					ImGui::Separator();
				}

				// モード3(三角錐2個)の個別SRTスライダー
				if (displayMode == 3) {
					ImGui::Text("[Pyramid 1]");
					ImGui::SliderFloat3("P1 Scale", p1_Scale, 0.1f, 5.0f);
					ImGui::SliderFloat3("P1 Rotation", p1_Rotate, -3.1415f, 3.1415f);
					ImGui::SliderFloat3("P1 Position", p1_Translate, -3.0f, 3.0f);

					ImGui::Separator();

					ImGui::Text("[Pyramid 2]");
					ImGui::SliderFloat3("P2 Scale", p2_Scale, 0.1f, 5.0f);
					ImGui::SliderFloat3("P2 Rotation", p2_Rotate, -3.1415f, 3.1415f);
					ImGui::SliderFloat3("P2 Position", p2_Translate, -3.0f, 3.0f);

					ImGui::Separator();
				}

				// 区切り線
				ImGui::Separator();
			}

			if (selectedObject == SelectedObject::Pyramid) {
				/// --- 自動で回転かと座標変えれます ---
				ImGui::Text("Pyramid Control");
				// 上の選択オブジェクト用Transformと表示名が同じでもIDが重ならないようにする
				ImGui::PushID("PyramidControl");

				// 1_拡縮の変更(XYZ)
				ImGui::SliderFloat3("Scale", &transform.scale.x, 0.1f, 10.0f);

				// 2_上下左右・奥への位置移動(XYZ)
				ImGui::SliderFloat3("Position", &transform.translate.x, -5.0f, 5.0f);

				// 3_自動回転の切り替えボタン
				// ボタンを押すたびにON/OFFが切り替わり、OFFになった瞬間に回転を初期値(0)にリセット
				if (ImGui::Button(isAutoRotate ? "Stop & Reset" : "Start Auto Rotate")) {
					isAutoRotate = !isAutoRotate;
					if (!isAutoRotate) {
						transform.rotate = {0.0f, 0.0f, 0.0f}; // 回転を初期値に戻す
					}
				}

				// 4_フラグの状態確認(0か1かで表示)
				ImGui::Text("Auto Rotate Flag: %d", isAutoRotate ? 1 : 0);

				// 5_各軸の回転(XYZ)
				// 自動回転がOFFのときだけ手動でいじれるようにしてONのときは現在の回転角を表示する
				if (!isAutoRotate) {
					ImGui::SliderFloat3("Rotation", &transform.rotate.x, -3.1415f, 3.1415f);
				} else {
					ImGui::Text("Rotation (Auto): X:%.2f, Y:%.2f, Z:%.2f", transform.rotate.x, transform.rotate.y, transform.rotate.z);
				}

				// 6_すべてのパラメータをリセット (SRTと自動回転を初期値に戻す)
				if (ImGui::Button("Reset All")) {
					// グローバルSRTと自動回転のリセット
					transform.scale = {1.0f, 1.0f, 1.0f};
					transform.rotate = {0.0f, 0.0f, 0.0f};
					transform.translate = {0.0f, 0.0f, 0.0f};
					isAutoRotate = false;

					// 現在のモードを維持したまま各パラメータをリセット
					for (int i = 0; i < 3; ++i) {
						t1_Scale[i] = 1.0f;  t1_Rotate[i] = 0.0f;
						t2_Scale[i] = 1.0f;  t2_Rotate[i] = 0.0f;
						p1_Scale[i] = 1.0f;  p1_Rotate[i] = 0.0f;
						p2_Scale[i] = 1.0f;  p2_Rotate[i] = 0.0f;
					}
					// モード1の初期位置
					t1_Translate[0] = -0.2f; t1_Translate[1] = -0.2f; t1_Translate[2] = 0.0f;
					t2_Translate[0] = 0.2f;  t2_Translate[1] = 0.2f;  t2_Translate[2] = 0.2f;

					// モード3の初期位置
					p1_Translate[0] = -0.3f; p1_Translate[1] = 0.0f;  p1_Translate[2] = 0.0f;
					p2_Translate[0] = 0.3f;  p2_Translate[1] = 0.0f;  p2_Translate[2] = 0.0f;

					// 全Particleを削除して最初の1個を生成する
					particleSystem.Reset();
				}
				ImGui::PopID();
			}

			if (selectedObject == SelectedObject::ParticleSystem) {
				ImGui::TextUnformatted("ParticleSystem");
				ImGui::Separator();
				ImGui::Text("Particle Count : %d", static_cast<int>(particleSystem.GetParticleCount()));
				ImGui::Text("Max Particle Count : %d", static_cast<int>(particleSystem.GetMaxParticleCount()));
				ImGui::Text("Total Vertex Count : %d", static_cast<int>(particleSystem.GetTotalVertexCount()));
				if (ImGui::Button("Reset Particles")) {
					// Particleを1個の初期状態へ戻す
					particleSystem.Reset();
				}
				ImGui::TextDisabled("Detailed execution is shown in Particle Flow.");
			}

			if (selectedObject == SelectedObject::None) {
				ImGui::TextDisabled("Select an item in Hierarchy.");
			}

			// 区切り線
			ImGui::Separator();

			ImGui::End();
#endif
#endif // USE_IMGUI

			// ImGuiで変更されたSpriteのSRTからWVPを更新する
			sprite2D.Update();

			///// ----- ImGui終わり ----- /////
#ifdef USE_IMGUI
	// 内部コマンドを生成する
			ImGui::Render();
#endif // USE_IMGUI

#pragma region
			///// ----- コマンドを積む ----- /////
			// これから書き込むバックバッファのインデックスを取得
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			// TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier{};
			// 今回のバリアはTransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			// Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			// バリアを張る対象のリソース。現在のバックバッファに対して行う
			barrier.Transition.pResource = swapChainResources[backBufferIndex];
			// 遷移前(現在)のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			// 遷移後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			// TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);

			// 描画先のRTVとDSVを設定する
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthStencilView.GetHandle();
			D3D12_CPU_DESCRIPTOR_HANDLE gameRenderTarget = rtvHandles[backBufferIndex];
#ifdef USE_IMGUI
			// エディター有効時はゲームをScene用テクスチャへ描画する
			sceneRenderTexture.TransitionToRenderTarget(commandList);
			gameRenderTarget = sceneRenderTexture.GetRTVHandle();
#endif // USE_IMGUI
			commandList->OMSetRenderTargets(1, &gameRenderTarget, false, &dsvHandle);
			// 指定した色で画面全体をクリアする
			float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f}; // 青っぽい色。RGBAの順
			commandList->ClearRenderTargetView(gameRenderTarget, clearColor, 0, nullptr);
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			// 描画用のDescriptorHeapの設定
			ID3D12DescriptorHeap* descriptorHeaps[] = {srvDescriptorHeap.Get()};
			commandList->SetDescriptorHeaps(1, descriptorHeaps);

			// 描画の設定(ドローコール)
			// ViewportとScissorRectの設定
			commandList->RSSetViewports(1, &viewport);
			commandList->RSSetScissorRects(1, &scissorRect);
			// RootSignatureの設定
			commandList->SetGraphicsRootSignature(pipelineState.GetRootSignature());
			// PSOの設定
			commandList->SetPipelineState(pipelineState.GetPipelineState());
			// 頂点バッファビューの設定
			const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView = vertexBuffer.GetView();
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			// 形状を設定
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			// マテリアルCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			// wvp用のBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress());
			// 平行光源をPixel Shaderのb1へ設定する
			commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

			// 画像を指定
			D3D12_GPU_DESCRIPTOR_HANDLE currentTextureHandle =
				textureManager.GetSrvHandle(static_cast<uint32_t>(textureMode));

			// 選択されたテクスチャをシェーダーへ渡す
			commandList->SetGraphicsRootDescriptorTable(
				2,
				currentTextureHandle
			);

			// モードの描画
			if (isParticleVisible &&
				renderingMode == 5) {
				// Production ModeではParticleSystemを描画する
				particleSystem.Draw(
					commandList,
					viewMatrix,
					projectionMatrix
				);

			} else if (isPrimitive3DVisible) {
				// Noneを含む描画判定はPrimitive3D側へ集約する
				primitive3D.Draw(commandList, currentTextureHandle);
			}

			///// ----- Sprite描画 ----- /////

			/// --- Sphere描画 ---
			if (isSphereVisible) {
				// 表示中だけ選択されたTextureでSphereを描画する
				const D3D12_GPU_DESCRIPTOR_HANDLE sphereTextureHandle =
					textureManager.GetSrvHandle(
						static_cast<uint32_t>(sphereTextureMode)
					);
				sphere.Draw(commandList, sphereTextureHandle);
			}

			/// --- OBJ平面描画 ---
			if (isPlaneVisible) {
				// 平面モデルで使用するTextureを取得する
				const D3D12_GPU_DESCRIPTOR_HANDLE planeTextureHandle =
					textureManager.GetSrvHandle(
						static_cast<uint32_t>(planeTextureMode)
					);

				// OBJから読み込んだ平面を描画する
				planeModel.Draw(
					commandList,
					planeTextureHandle
				);
			}

			/// --- Axisモデル描画 ---
			if (isAxisVisible) {
				// Axisモデルで使用するTextureを取得する
				const D3D12_GPU_DESCRIPTOR_HANDLE axisTextureHandle =
					textureManager.GetSrvHandle(
						static_cast<uint32_t>(axisTextureMode)
					);

				// OBJから読み込んだAxisモデルを描画する
				axisModel.Draw(
					commandList,
					axisTextureHandle
				);
			}

			/// --- MultiMeshモデル描画 ---
			if (isMultiMeshVisible) {
				// MultiMeshモデルで使用するTextureを取得する
				const D3D12_GPU_DESCRIPTOR_HANDLE multiMeshTextureHandle =
					textureManager.GetSrvHandle(
						static_cast<uint32_t>(
						multiMeshTextureMode
					)
					);

				// 複数Meshを含むモデルを描画する
				multiMeshModel.Draw(
					commandList,
					multiMeshTextureHandle
				);
			}

			/// --- MultiMaterialモデル描画 ---
			if (isMultiMaterialVisible) {
				// MeshごとにMTLで指定されたTextureを使って描画する
				multiMaterialModel.DrawWithMaterials(
					commandList,
					textureManager
				);
			}

			/// --- Texture ---
			// 三角形とは別に選択されたTextureを取得する
			if (isSpriteVisible) {
				// 表示中だけ選択されたTextureでSpriteを描画する
				const D3D12_GPU_DESCRIPTOR_HANDLE spriteTextureHandle =
					textureManager.GetSrvHandle(static_cast<uint32_t>(spriteTextureMode));
				// 3Dの後に描画してSpriteを最前面へ表示する
				sprite2D.Draw(commandList, spriteTextureHandle);
			}

			// 画面表示できるようにする
			// 今回はRenderTargetからPresentにする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

			// 実際のcommandListのImGuiの描画コマンドを積む
#ifdef USE_IMGUI
	// ゲーム描画をImGuiから読める状態へ戻し、UIはSwapChainへ描画する
			sceneRenderTexture.TransitionToShaderResource(commandList);
			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, nullptr);
			const float editorClearColor[] = {0.08f, 0.08f, 0.08f, 1.0f};
			commandList->ClearRenderTargetView(
				rtvHandles[backBufferIndex],
				editorClearColor,
				0,
				nullptr
			);
			// ImGuiは固定1280x720ではなく、現在のBackBuffer全体へ描画する
			commandList->RSSetViewports(1, &editorViewport);
			commandList->RSSetScissorRects(1, &editorScissorRect);
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif // USE_IMGUI
			// TransitionBarrerを張る
			commandList->ResourceBarrier(1, &barrier);

			///// ----- CommandContext ----- /////

			/// --- コマンドの実行 ---
			// コマンドを実行して画面を表示する
			commandContext.ExecuteAndPresent(swapChain);
#pragma endregion コマンドを積む処理
		}
	} // whileの終わり

	  /// --- ImGui終了処理 ---
#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	sceneRenderTexture.Finalize();
#endif // USE_IMGUI

	///// ----- 解放処理 ----- /////
	/// --- 1_各種バッファ・テクスチャ・リソース(すべてdeviceより前) ---
	vertexBuffer.Finalize();
	primitive3D.Finalize();
	sphere.Finalize(); // 球
	planeModel.Finalize(); // OBJ平面のリソースを解放する
	axisModel.Finalize(); // Axisモデルのリソースを解放する
	multiMeshModel.Finalize(); // 複数Meshモデルのリソースを解放する
	multiMaterialModel.Finalize(); // 複数Materialモデルのリソースを解放する
	sprite2D.Finalize();
	particleSystem.Finalize();
	wvpResource->Release();
	materialResource->Release();
	// 平行光源のリソースを解放する
	directionalLightResource->Release();

	// 読み込んだテクスチャをすべて解放する
	textureManager.Finalize();
	depthStencilView.Finalize();

	/// --- 2_PSOとコマンド関連は各クラスが解放する ---
	pipelineState.Finalize();
	commandContext.Finalize();

	/// --- 4_ディスクリプタヒープ ---
	rtvDescriptorHeap.Finalize();
	srvDescriptorHeap.Finalize();
	dsvDescriptorHeap.Finalize();

	/// --- 5_スワップチェーンとバックバッファリソース ///
	swapChainResources[0]->Release();
	swapChainResources[1]->Release();
	swapChain->Release();

#ifdef _DEBUG
	debugController->Release();
#endif // _DEBUG

	/// --- 8_すべての依存リソースが消えたので解放 ---
	device->Release();
	useAdapter->Release();
	dxgiFactory->Release();

	// DX12リソースがなくなった後にウィンドウを閉じる
	CloseWindow(hwnd);

	/// --- ReportLiveObjects ---
	// リソースリークチェック
	IDXGIDebug1* debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}

	// COMの終了処理
	CoUninitialize();

	return 0;
}
