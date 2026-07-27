#include "Audio.h"

#include <cassert>
#include <cstring>
#include <fstream>

// XAudio2を利用するためのライブラリをリンクする
#pragma comment(lib, "xaudio2.lib")

/// <summary>
/// WAVファイルを読み込む
/// </summary>
SoundData SoundLoadWave(const char *filename)
{
	// WAVファイルをバイナリモードで開く
	std::ifstream file(filename, std::ios::binary);
	assert(file.is_open());

	// RIFFヘッダーを読み込み、WAVEファイルであることを確認する
	RiffHeader riff{};
	file.read(reinterpret_cast<char *>(&riff), sizeof(riff));
	assert(file.good());
	assert(std::memcmp(riff.chunk.id, "RIFF", 4) == 0);
	assert(std::memcmp(riff.type, "WAVE", 4) == 0);

	SoundData soundData{};
	bool formatFound = false;
	bool dataFound = false;

	// fmtやdata以外のチャンクが含まれていても読み飛ばせるように順番に調べる
	while (file && (!formatFound || !dataFound))
	{
		ChunkHeader chunk{};
		file.read(reinterpret_cast<char *>(&chunk), sizeof(chunk));
		if (!file)
		{
			break;
		}

		if (std::memcmp(chunk.id, "fmt ", 4) == 0)
		{
			// WAVEFORMATEXへ格納できる範囲だけ読み込む
			const std::streamsize formatSize =
				static_cast<std::streamsize>((chunk.size < sizeof(WAVEFORMATEX)) ? chunk.size : sizeof(WAVEFORMATEX));
			file.read(reinterpret_cast<char *>(&soundData.wfex), formatSize);

			// 拡張部分がWAVEFORMATEXより大きい場合は残りを読み飛ばす
			if (chunk.size > sizeof(WAVEFORMATEX))
			{
				file.seekg(static_cast<std::streamoff>(chunk.size - sizeof(WAVEFORMATEX)), std::ios::cur);
			}
			formatFound = true;
		}
		else if (std::memcmp(chunk.id, "data", 4) == 0)
		{
			// WAVの32bitチャンクサイズをXAudio2用のバッファサイズへ格納する
			static_assert(sizeof(chunk.size) <= sizeof(unsigned int));
			soundData.bufferSize = static_cast<unsigned int>(chunk.size);
			soundData.pBuffer = new BYTE[soundData.bufferSize];
			file.read(reinterpret_cast<char *>(soundData.pBuffer), static_cast<std::streamsize>(soundData.bufferSize));
			dataFound = true;
		}
		else
		{
			// JUNKやLISTなど、再生に不要なチャンクを読み飛ばす
			file.seekg(static_cast<std::streamoff>(chunk.size), std::ios::cur);
		}

		// RIFFチャンクは偶数バイト境界に配置されるため、パディングを読み飛ばす
		if ((chunk.size & 1U) != 0U)
		{
			file.seekg(1, std::ios::cur);
		}
	}

	// 再生に必要なfmtチャンクとdataチャンクが存在することを確認する
	assert(formatFound);
	assert(dataFound);
	assert(file.good() || file.eof());

	return soundData;
}

/// <summary>
/// 初期化処理
/// </summary>
void Audio::Initialize()
{
	// XAudio2エンジンを生成する
	HRESULT hr = XAudio2Create(xAudio2_.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(hr));

	// 最終的な音声出力を担当するMasteringVoiceを生成する
	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
	assert(SUCCEEDED(hr));
}

/// <summary>
/// 解放処理
/// </summary>
void Audio::Finalize()
{
	// XAudio2本体を解放する前に再生中のVoiceを破棄する
	SoundStopWave();

	if (masterVoice_ != nullptr)
	{
		// IXAudio2Voice系はReleaseではなくDestroyVoiceで破棄する
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
	}

	// XAudio2本体をMasteringVoiceより後に解放する
	xAudio2_.Reset();
}

Audio::~Audio()
{
	Finalize();
}

/// <summary>
/// 音声再生
/// </summary>
void Audio::SoundPlayWave(const SoundData &soundData, bool isLoop)
{
	// すでに再生中なら停止してから新しい音声を開始する
	SoundStopWave();

	// 波形フォーマットをもとにSourceVoiceを生成する
	HRESULT hr = xAudio2_->CreateSourceVoice(&sourceVoice_, &soundData.wfex);
	assert(SUCCEEDED(hr));

	XAUDIO2_BUFFER buffer{};
	buffer.pAudioData = soundData.pBuffer;
	buffer.AudioBytes = soundData.bufferSize;
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	// フラグがtrueの間鳴り続けるように無限ループを指定する
	buffer.LoopCount = isLoop ? XAUDIO2_LOOP_INFINITE : 0;

	hr = sourceVoice_->SubmitSourceBuffer(&buffer);
	assert(SUCCEEDED(hr));

	hr = sourceVoice_->Start();
	assert(SUCCEEDED(hr));
}

/// <summary>
/// 音声停止
/// </summary>
void Audio::SoundStopWave()
{
	if (sourceVoice_ == nullptr)
	{
		return;
	}

	// 再生を止め、登録済みバッファとSourceVoiceを破棄する
	sourceVoice_->Stop();
	sourceVoice_->FlushSourceBuffers();
	sourceVoice_->DestroyVoice();
	sourceVoice_ = nullptr;
}

/// <summary>
/// 音声データ解放
/// </summary>
void SoundUnload(SoundData *soundData)
{
	// バッファのメモリ解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = nullptr;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}
