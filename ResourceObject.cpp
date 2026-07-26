#include "ResourceObject.h"

///// ----- ResourceObject ----- /////

ResourceObject::ResourceObject(ID3D12Resource* resource)
	: resource_(resource) {
}

// 管理しているResourceを自動的に解放する
ResourceObject::~ResourceObject() {
	Reset();
}

// 古いResourceを解放してから、新しいResourceを管理する
void ResourceObject::Reset(ID3D12Resource* resource) {
	if (resource_ != nullptr) {
		resource_->Release();
	}

	resource_ = resource;
}