#pragma once

#include "DescriptorHeap.h"
#include "ShaderResourceView.h"

#include "externals/DirectXTex/DirectXTex.h"

#include <array>
#include <cstdint>
#include <string>

///// ----- TextureManager ----- /////

class TextureManager {
public:
	// 使用するテクスチャの数
	static constexpr uint32_t kTextureCount = 4;

	TextureManager() = default;
	~TextureManager();

	TextureManager(const TextureManager&) = delete;
	TextureManager& operator=(const TextureManager&) = delete;

	/// --- 初期化 ---
	// Textureを読み込み、GPUへの転送とSRVの作成を行う
	void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const DescriptorHeap& srvDescriptorHeap);

	/// --- 転送完了後の処理 ---
	// GPU転送用のResourceを解放
	void ReleaseIntermediateResources();

	/// --- 取得 ---
	// ImGuiで選択されたTextureのGPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle(uint32_t textureIndex) const;

	// ファイルパスに対応する読み込み済みテクスチャ番号を取得する
	uint32_t FindTextureIndex(
		const std::string& filePath
	) const;

	/// --- 終了処理 ---
	// 読み込んだTextureを解放
	void Finalize();

private:
	/// --- Textureの読み込み ---
	// Textureデータを読む
	DirectX::ScratchImage LoadTexture(const std::string& filePath);

	/// --- TextureResource ---
	// TextureResourceを作成
	ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

	/// --- Textureの転送 ---
	// TextureResourceにデータを転送
	ID3D12Resource* UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	std::array<DirectX::ScratchImage, kTextureCount> textureMipImages_;
	std::array<ID3D12Resource*, kTextureCount> textureResources_{};
	std::array<ID3D12Resource*, kTextureCount> intermediateResources_{};
	ShaderResourceView shaderResourceView_;
};
