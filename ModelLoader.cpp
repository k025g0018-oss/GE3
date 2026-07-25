#include "ModelLoader.h"

#include <cassert>
#include <fstream>
#include <sstream>

///// ----- OBJファイルの読み込み ----- /////
ModelData ModelLoader::LoadObjFile(
	const std::string& directoryPath,
	const std::string& filename
) {
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

		if (identifier == "v") {
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
			/// --- 三角形を作る ---
			VertexData triangle[3];

			// 面を構成する3頂点を読み込む 面は三角形限定。ほかは未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				lineStream >> vertexDefinition;

				// 頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/'); // /区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}

				// 要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				// VertexData vertex = {position, texcoord, normal};
				// modelData.vertices.push_back(vertex);

				triangle[faceVertex] = {position, texcoord, normal};
			}

			// 頂点を逆順で登録することで、回り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);

			/*
			// 現在のプロジェクトでは、RasterizerState.cppで裏面カリングが有効 なので0,1,2で初期状態で表面になる
			modelData.vertices.push_back(triangle[0]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[2]);
			*/

		} else if (identifier == "mtllib") {
			// materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			lineStream >> materialFilename;
			// 基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}

	// 構築したモデルデータを呼び出し元へ返す
	return modelData;
}

///// ----- MTLファイルの読み込み ----- /////

MaterialData ModelLoader::LoadMaterialTemplateFile(
	const std::string& directoryPath,
	const std::string& filename
) {
	// 1_中で必要となる変数の宣言
	MaterialData materialData; // 構築するMaterialData
	std::string line; // ファイルから読んだ1行を格納するもの

	// 2_ファイルを開く
	std::ifstream file(directoryPath + "/" + filename); // ファイルを開く
	assert(file.is_open()); // とりあえず開けなかったら止める

	// 3_実際にファイルを読み、MaterialDataを構築していく
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		// identifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// 連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}

	// 4_MaterialDataを返す
	return materialData;
}