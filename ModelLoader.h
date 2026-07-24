#pragma once

#include "VertexData.h"

#include <string>
#include <vector>

///// ----- ModelLoader ----- /////
/// --- 読み込んだモデルデータ ---
/// OBJファイルから取得した頂点情報を保持する
struct ModelData {
	std::vector<VertexData> vertices;
};

namespace ModelLoader {

	/// --- OBJファイルの読み込み ---
	/// 指定されたOBJファイルを読み込み、モデルデータを返す
	ModelData LoadObjFile(
		const std::string& directoryPath,
		const std::string& filename
	);

} // namespace ModelLoader