#pragma once

#include "VertexData.h"

#include <string>
#include <vector>

///// ----- ModelLoader ----- /////
/// --- 読み込んだマテリアルデータ ---
/// MTLファイルから取得したテクスチャ情報を保持する
struct MaterialData {
	std::string textureFilePath;
};

/// --- 読み込んだモデルデータ ---
/// OBJファイルから取得した頂点情報を保持する
struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};

namespace ModelLoader {

	/// --- MTLファイルの読み込み ---
	/// 指定されたMTLファイルを読み込み、マテリアルデータを返す
	MaterialData LoadMaterialTemplateFile(
		const std::string& directoryPath,
		const std::string& filename
	);

	/// --- OBJファイルの読み込み ---
	/// 指定されたOBJファイルを読み込み、モデルデータを返す
	ModelData LoadObjFile(
		const std::string& directoryPath,
		const std::string& filename
	);

} // namespace ModelLoader

