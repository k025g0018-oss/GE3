#pragma once

#include <ostream>
#include <string>

///// ----- Logger ----- /////

/// --- 文字列変換 ---
// UTF-8文字列をワイド文字列に変換
std::wstring ConvertString(const std::string& str);

// ワイド文字列をUTF-8文字列に変換
std::string ConvertString(const std::wstring& str);

/// --- ログ出力 ---
// 文字列をファイルとデバッグ出力へ書き込む
void Log(std::ostream& os, const std::string& message);

// ワイド文字列をファイルとデバッグ出力へ書き込む
void Log(std::ostream& os, const std::wstring& message);
