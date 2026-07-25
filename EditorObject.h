#pragma once

#ifdef USE_IMGUI

#include "Primitive3D.h"
#include "ParticleSystem.h"
#include "Sprite.h"
#include "Sphere.h"
#include "Model.h"

// ゲームオブジェクトをImGuiのHierarchyとPropertiesへ接続する共通ラッパー
class IEditorObject {
public:
	virtual ~IEditorObject() = default;
	// Hierarchyへ表示するオブジェクト名を返す
	virtual const char* GetName() const = 0;
	// 選択中オブジェクトの操作項目をPropertiesへ表示する
	virtual void DrawProperties() = 0;
};

// Primitive3DをHierarchyとPropertiesへ接続するラッパークラス
class Primitive3DEditorObject final : public IEditorObject {
public:
	Primitive3DEditorObject(
		Primitive3D& object,
		int& textureMode,
		ParticleSystem& particleSystem,
		bool& isVisible
	);
	const char* GetName() const override { return "Primitive3D"; }
	void DrawProperties() override;

private:
	// 描画本体は所有せず、mainで作成したPrimitive3Dを参照する
	Primitive3D& object_;
	// Texture選択とParticle設定も同じProperties上で操作する
	int& textureMode_;
	ParticleSystem& particleSystem_;
	// Primitive3Dを描画するかどうかをmainと共有する
	bool& isVisible_;
};

// Sprite2DをHierarchyとPropertiesへ接続するラッパークラス
class Sprite2DEditorObject final : public IEditorObject {
public:
	Sprite2DEditorObject(Sprite2D& sprite, int& textureMode, bool& isVisible);
	const char* GetName() const override { return "Sprite2D"; }
	void DrawProperties() override;

private:
	// 描画本体は所有せず、mainで作成したSprite2Dを参照する
	Sprite2D& sprite_;
	int& textureMode_;
	// Spriteを描画するかどうかをmainと共有する
	bool& isVisible_;
};

// ParticleSystemをHierarchyとPropertiesへ接続するラッパークラス
class ParticleEditorObject final : public IEditorObject {
public:
	ParticleEditorObject(
		ParticleSystem& particleSystem,
		bool& isVisible
	);
	const char* GetName() const override { return "ParticleSystem"; }
	void DrawProperties() override;

private:
	// Particleの生成数や速度をPropertiesから操作するため参照を保持する
	ParticleSystem& particleSystem_;
	// Particleを描画するかどうかをmainと共有する
	bool& isVisible_;
};

// SphereをHierarchyとPropertiesへ接続するラッパークラス
class SphereEditorObject final : public IEditorObject {
public:
	// Sphere本体とTexture選択番号を参照として受け取る
	SphereEditorObject(Sphere& sphere, int& textureMode, bool& isVisible);

	const char* GetName() const override {
		return "Sphere";
	}

	void DrawProperties() override;

private:
	// Sphereの設定を編集するため参照を保持する
	Sphere& sphere_;

	// Sphere専用のTexture選択番号を保持する
	int& textureMode_;

	// Sphereを描画するかどうかをmainと共有する
	bool& isVisible_;
};

// OBJモデルをHierarchyとPropertiesへ接続するラッパークラス
class ModelEditorObject final : public IEditorObject {
public:
	// モデル本体と表示設定を参照として受け取る
	ModelEditorObject(
		Model& model,
		const char* name,
		int& textureMode,
		bool& isVisible
	);

	const char* GetName() const override {
		return name_;
	}

	void DrawProperties() override;

private:
	// Propertiesから編集するOBJモデルを保持する
	Model& model_;

	// Hierarchyへ表示するモデル名を保持する
	const char* name_;

	// モデル専用のTexture選択番号を保持する
	int& textureMode_;

	// モデルを描画するかどうかをmainと共有する
	bool& isVisible_;
};

// Scene全体のStart、Stop、ResetをPropertiesへ表示するラッパークラス
class SceneSettingsEditorObject final : public IEditorObject {
public:
	explicit SceneSettingsEditorObject(Primitive3D& object);
	const char* GetName() const override { return "Scene Settings"; }
	void DrawProperties() override;

private:
	Primitive3D& object_;
};

#endif // USE_IMGUI
