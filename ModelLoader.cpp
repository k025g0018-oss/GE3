#include "ModelLoader.h"

#include <cassert>
#include <fstream>
#include <sstream>

///// ----- OBJファイルの読み込み ----- /////
ModelData ModelLoader::LoadObjFile(
	const std::string& directoryPath,
	const std::string& filename
) {
	// objを読み込む時現在処理しているMeshを記録する
	MeshData* currentMesh = nullptr;
	// 読み込んだ頂点情報を格納するモデルデータ
	ModelData modelData;

	std::vector<Vector4> positions; // 位置
	std::vector<Vector3> normals; // 法線
	std::vector<Vector2> texcoords; // テクスチャ座標

	// OBJファイルの各行を解析するために使用する変数
	std::string line;

	// ディレクトリ名とファイル名を結合してOBJファイルを開く
	std::ifstream file(directoryPath + "/" + filename);

	// ファイルが正常に開けなかった場合は処理を停止する
	assert(file.is_open());

	// OBJファイルを1行ずつ読み込み、モデルデータを構築する
	while (std::getline(file, line)) {
		std::string identifier;
		// これから頂点座標、テクスチャ座標、法線などを解析していく
		std::istringstream lineStream(line);
		lineStream >> identifier; // 先頭の識別子を読む

		// identifierに応じた処理
		if (identifier == "o") {
			MeshData mesh;

			// oの後ろにあるMesh名を読み込む
			lineStream >> mesh.name;

			modelData.meshes.push_back(mesh);

			// これ以降の面を追加するMeshを更新する
			currentMesh = &modelData.meshes.back();

		} else if (identifier == "v") {
			// 頂点位置を読み込む
			Vector4 position;
			lineStream >> position.x >> position.y >> position.z;
			// 右手座標系から左手座標系へ変換する
			position.x *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vt") {
			// テクスチャ座標を読み込む
			Vector2 texcoord;
			lineStream >> texcoord.x >> texcoord.y;
			// モデルのX反転に合わせてUVも左右反転する
			// texcoord.x = 1.0f - texcoord.x;
			// UVの原点を左下から左上へ変換する
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			// 頂点法線を読み込む
			Vector3 normal;
			lineStream >> normal.x >> normal.y >> normal.z;
			// 頂点位置と同様に法線も左手座標系へ変換する
			normal.x *= -1.0f;
			normals.push_back(normal);

		} else if (identifier == "f") {
			// oがないOBJでは、名前なしのMeshを自動的に作る
			if (currentMesh == nullptr) {
				modelData.meshes.push_back({});
				currentMesh = &modelData.meshes.back();
			}

			/// --- 三角形を作る ---
			VertexData triangle[3];

			// 面を構成する3頂点を読み込む 面は三角形限定。ほかは未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				lineStream >> vertexDefinition;

				// 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3] = {};
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/'); // /区切りでインデックスを読んでいく
					
					// 空のUV番号を許可する
					if (!index.empty()) {
						elementIndices[element] =
							static_cast<uint32_t>(
								std::stoi(index));
					}
				}

				// 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];

				// UVがないモデルでは初期値の(0, 0)を使用する
				Vector2 texcoord = {
					0.0f,
					0.0f
				};

				if (elementIndices[1] != 0) {
					texcoord =
						texcoords[elementIndices[1] - 1];
				}

				// 法線が存在する場合だけ読み込む
				Vector3 normal = {
					0.0f,
					0.0f,
					0.0f
				};

				if (elementIndices[2] != 0) {
					normal =
						normals[elementIndices[2] - 1];
				}

				triangle[faceVertex] = {position, texcoord, normal};
			}

			// 現在のMeshへ三角形を追加する
			// 頂点を逆順で登録することで、回り順を逆にする
			currentMesh->vertices.push_back(triangle[2]);
			currentMesh->vertices.push_back(triangle[1]);
			currentMesh->vertices.push_back(triangle[0]);

		} else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			lineStream >> materialFilename;

			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			// MTL内にあるすべてのMaterialを読み込む
			modelData.materials = LoadMaterialTemplateFile(directoryPath, materialFilename);

		} else if (identifier == "usemtl") {
			// oがない場合は名前なしのMeshを作る
			if (currentMesh == nullptr) {
				modelData.meshes.push_back({});
				currentMesh = &modelData.meshes.back();
			}

			// これ以降の面で使用するMaterial名をMeshへ保存する
			lineStream >> currentMesh->materialName;
		}

	}

	// 構築したモデルデータを呼び出し元へ返す
	return modelData;
}

///// ----- MTLファイルの読み込み ----- /////

std::vector<MaterialData> ModelLoader::LoadMaterialTemplateFile(
	const std::string& directoryPath,
	const std::string& filename
) {
	// 1_中で必要となる変数の宣言
	// MTLから読み込んだすべてのMaterial
	std::vector<MaterialData> materials;

	// 現在読み込んでいるMaterial
	MaterialData* currentMaterial = nullptr;

	// ファイルから読んだ1行を格納するもの
	std::string line;

	// 2_ファイルを開く
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開けなかったら止める

	// 3_実際にファイルを読み、MaterialDataを構築していく
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifierに応じた処理
		if (identifier == "newmtl") {
			MaterialData material;

			// マテリアル名を読み込む
			s >> material.name;

			materials.push_back(material);
			currentMaterial = &materials.back();

		} else if (identifier == "Ka" && currentMaterial != nullptr) {
			// 現在のMaterialへ環境光色を設定する
			s >>
				currentMaterial->ambientColor.x >>
				currentMaterial->ambientColor.y >>
				currentMaterial->ambientColor.z;

		} else if (identifier == "Kd" && currentMaterial != nullptr) {
			// 現在のMaterialへ拡散反射色を設定する
			s >>
				currentMaterial->diffuseColor.x >>
				currentMaterial->diffuseColor.y >>
				currentMaterial->diffuseColor.z;

		} else if (identifier == "map_Kd" && currentMaterial != nullptr) {
			std::string textureFilename;

			// map_Kdに含まれるオプションとファイル名を順番に読む
			std::string mapKdToken;
			while (s >> mapKdToken) {
				if (mapKdToken == "-o") {
					// UVの移動を読み込む
					s >>
						currentMaterial->uvTranslate.x >>
						currentMaterial->uvTranslate.y >>
						currentMaterial->uvTranslate.z;

				} else if (mapKdToken == "-s") {
					// UVの拡大縮小を読み込む
					s >>
						currentMaterial->uvScale.x >>
						currentMaterial->uvScale.y >>
						currentMaterial->uvScale.z;

				} else {
					// オプション以外をTextureファイル名として扱う
					textureFilename = mapKdToken;
				}
			}

			// 現在のMaterialへテクスチャパスを設定する
			currentMaterial->textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	// 4_MaterialDataを返す
	// 読み込んだすべてのMaterialを返す
	return materials;
}
