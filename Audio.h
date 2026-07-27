#pragma once

#include <cstdint>
#include <wrl.h>
#include <xaudio2.h>

// チャンクヘッダ
struct ChunkHeader
{
	char id[4];	   // チャンク毎のID
	uint32_t size; // チャンクサイズ
};

// RIFFヘッダチャンク
struct RiffHeader
{
	ChunkHeader chunk; // "RIFF"
	char type[4];	   // "WAVE"
};

// FMTチャンク
struct FormatChunk
{
	ChunkHeader chunk; // "fmt"
	WAVEFORMATEX fmt;  // 波形フォーマット
};

// 音声データ
struct SoundData
{
	// 波形フォーマット
	WAVEFORMATEX wfex{};
	// バッファの先頭アドレス
	BYTE *pBuffer = nullptr;
	// バッファサイズ
	unsigned int bufferSize = 0;
};

/// <summary>
/// WAVファイルを読み込む
/// </summary>
/// <param name="filename"></param>
/// <returns></returns>
SoundData SoundLoadWave(const char *filename);

/// <summary>
/// 読み込んだ音声データを解放する
/// </summary>
void SoundUnload(SoundData *soundData);

class Audio
{
  public:
	Audio() = default;
	~Audio();

	/// <summary>
	/// 初期化処理
	/// </summary>
	void Initialize();

	/// <summary>
	/// 解放処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 読み込んだ音声を再生する
	/// </summary>
	void SoundPlayWave(const SoundData &soundData, bool isLoop);

	/// <summary>
	/// 再生中の音声を停止する
	/// </summary>
	void SoundStopWave();

  private:
	// XAudio2本体はCOMインターフェースなのでComPtrで管理する
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;

	// MasteringVoiceにはReleaseがないため、通常のポインタで管理する
	IXAudio2MasteringVoice *masterVoice_ = nullptr;

	// 再生中の音声を停止できるようにSourceVoiceを保持する
	IXAudio2SourceVoice *sourceVoice_ = nullptr;
};
