#include "TextureManager.h"

#include "BufferResource.h"
#include "Logger.h"

#include "externals/DirectXTex/d3dx12.h"

#include <cassert>
#include <vector>

///// ----- TextureManager ----- /////

TextureManager::~TextureManager() {
	Finalize();
}

/// --- 初期化 ---
// Textureを読み込み、GPUへの転送とSRVの作成を行う
void TextureManager::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const DescriptorHeap& srvDescriptorHeap) {
	// テクスチャのファイルパス 番号1~ 0はImGuiが使う
	const char* textureFilePaths[kTextureCount] = {
		"resources/white.png",
		"resources/uvChecker.png",
		"resources/genbaneko.png",
		"resources/monsterBall.png"
	};

	// ImGuiが0番を使うため、Texture用SRVは1番から使用する
	shaderResourceView_.Initialize(srvDescriptorHeap, 1, kTextureCount);

	// 3枚のテクスチャを読み込んでGPUへ転送する
	for (uint32_t i = 0; i < kTextureCount; ++i) {
		textureMipImages_[i] = LoadTexture(textureFilePaths[i]);

		const DirectX::TexMetadata& metadata = textureMipImages_[i].GetMetadata();

		textureResources_[i] = CreateTextureResource(device, metadata);

		intermediateResources_[i] = UploadTextureData(
			textureResources_[i].Get(),
			textureMipImages_[i],
			device,
			commandList
		);

		// TextureごとのSRVを作成する
		shaderResourceView_.CreateTextureSRV(device, i, textureResources_[i].Get(), metadata);
	}
}

/// --- 転送完了後の処理 ---
// GPU転送用のResourceを解放
void TextureManager::ReleaseIntermediateResources() {
	// 転送が終わったのでソースは解放する
	for (Microsoft::WRL::ComPtr<ID3D12Resource>& intermediateResource : intermediateResources_) {
		// GPU転送完了後に中間Resourceの所有権を解放する
		intermediateResource.Reset();
	}
}

/// --- 取得 ---
// ImGuiで選択されたTextureのGPUハンドルを取得
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandle(uint32_t textureIndex) const {
	assert(textureIndex < kTextureCount);
	return shaderResourceView_.GetGPUHandle(textureIndex);
}

uint32_t TextureManager::FindTextureIndex(
	const std::string& filePath
) const {
	// MTLで指定されたパスと読み込み済みテクスチャを対応させる
	if (filePath == "resources/uvChecker.png") {
		return 1;
	}

	if (filePath == "resources/genbaneko.png") {
		return 2;
	}

	if (filePath == "resources/monsterBall.png") {
		return 3;
	}

	// 対応する画像がない場合は白テクスチャを使う
	return 0;
}

/// --- Textureの読み込み ---
// Textureデータを読むための関数
DirectX::ScratchImage TextureManager::LoadTexture(const std::string& filePath) {
	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	const DirectX::TexMetadata& metadata = image.GetMetadata();

	// 1x1画像は縮小できないので、そのまま返す
	if (metadata.width == 1 && metadata.height == 1) {
		return image;
	}

	// ミップマップの作製
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), metadata, DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	// ミップマップ付きのデータを返す
	return mipImages;
}

/// --- TextureResource ---
// TextureResourceを作る
Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
	// 1_metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width); // Textureの幅
	resourceDesc.Height = UINT(metadata.height); // Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行きor配列Textureの配列数
	resourceDesc.Format = metadata.format; // TextureのFormat
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元数

	// 2_利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 細かい設定を行う

	// 3_Resourceを生成する
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST, // 初回のResourceState
		nullptr, // Clear最適値。使わない
		IID_PPV_ARGS(resource.GetAddressOf())
	);
	assert(SUCCEEDED(hr));

	return resource;
}

/// --- Textureの転送 ---
// TextureResourceにデータを転送する
Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);

	uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));

	// 中間用のバッファ(UPLOAD)を作成
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource =
		BufferResource::Create(device, intermediateSize);

	// データ転送コマンドを積む
	UpdateSubresources(commandList, texture, intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

	// Textureへの転送後、利用できるようにCOPY_DESTからGENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;

	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

/// --- 終了処理 ---
// 読み込んだTextureを解放
void TextureManager::Finalize() {
	ReleaseIntermediateResources();
	// TextureResourceはComPtrのデストラクタが自動解放する
}
