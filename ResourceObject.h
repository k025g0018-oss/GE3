#pragma once

#include <d3d12.h>

///// ----- ResourceObject ----- /////

class ResourceObject {
public:
	// 後からResourceを設定できるように、nullptrでも生成可能にする
	explicit ResourceObject(ID3D12Resource* resource = nullptr);

	// 管理しているResourceを自動的に解放する
	~ResourceObject();

	// 同じResourceの二重解放を防ぐため、コピーを禁止する
	ResourceObject(const ResourceObject&) = delete;
	ResourceObject& operator=(const ResourceObject&) = delete;

	// 管理するResourceを後から設定する
	void Reset(ID3D12Resource* resource = nullptr);

	/// --- 取得 ---
	// 管理しているResourceのポインタを取得する
	ID3D12Resource* Get() const { return resource_; }

private:
	ID3D12Resource* resource_;

};

