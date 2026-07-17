#pragma once

#ifdef USE_IMGUI

#include "Object3D.h"
#include "ParticleSystem.h"
#include "Sprite.h"

// ゲームオブジェクトをImGuiのHierarchyとPropertiesへ接続する共通ラッパー
class IEditorObject {
public:
	virtual ~IEditorObject() = default;
	virtual const char* GetName() const = 0;
	virtual void DrawProperties() = 0;
};

class Object3DEditorObject final : public IEditorObject {
public:
	Object3DEditorObject(Object3D& object, Vector4& materialColor, int& textureMode, ParticleSystem& particleSystem);
	const char* GetName() const override { return "Pyramid / 3D Object"; }
	void DrawProperties() override;

private:
	Object3D& object_;
	Vector4& materialColor_;
	int& textureMode_;
	ParticleSystem& particleSystem_;
};

class SpriteEditorObject final : public IEditorObject {
public:
	SpriteEditorObject(Sprite& sprite, int& textureMode);
	const char* GetName() const override { return "Sprite"; }
	void DrawProperties() override;

private:
	Sprite& sprite_;
	int& textureMode_;
};

class ParticleEditorObject final : public IEditorObject {
public:
	explicit ParticleEditorObject(ParticleSystem& particleSystem);
	const char* GetName() const override { return "ParticleSystem"; }
	void DrawProperties() override;

private:
	ParticleSystem& particleSystem_;
};

class SceneSettingsEditorObject final : public IEditorObject {
public:
	explicit SceneSettingsEditorObject(Object3D& object);
	const char* GetName() const override { return "Scene Settings"; }
	void DrawProperties() override;

private:
	Object3D& object_;
};

#endif // USE_IMGUI
