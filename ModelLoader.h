#pragma once

#include "VertexData.h"

#include <string>
#include <vector>

///// ----- ModelLoader ----- /////
/// --- 読み込んだマテリアルデータ ---
/// MTLファイルから取得したテクスチャ情報を保持する
struct MaterialData {
	// MTL内のnewmtlで指定された名前
	std::string name;

	// Kaで指定された環境光色
	Vector3 ambientColor;

	// Kdで指定された拡散反射色
	Vector3 diffuseColor;

	// map_Kdで指定されたテクスチャ
	std::string textureFilePath;

	// map_Kdの-oで指定されたUVの移動
	Vector3 uvTranslate{
		0.0f, 0.0f, 0.0f
	};

	// map_Kdの-sで指定されたUVの拡大縮小
	Vector3 uvScale{
		1.0f, 1.0f, 1.0f
	};
};

/// --- 読み込んだモデルデータ ---
/// OBJ内の1つのMeshを保持する
struct MeshData {
	// o識別子で指定されたMesh名
	std::string name;

	// usemtl識別子で指定されたMaterial名
	std::string materialName;

	// Meshを構成する頂点
	std::vector<VertexData> vertices;
};

/// OBJファイルから取得した頂点情報を保持する
struct ModelData {
	// OBJ内にある複数のMesh
	std::vector<MeshData> meshes;

	// MTLから読み込んだマテリアル 単一Material
	//MaterialData material;

	// MTL内にある複数のMaterial
	std::vector<MaterialData> materials;
};

namespace ModelLoader {

	/// --- MTLファイルの読み込み ---
	/// 指定されたMTLファイルを読み込み、マテリアルデータを返す
	// MTLファイル内のすべてのMaterialを返す
	std::vector<MaterialData> LoadMaterialTemplateFile(
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

