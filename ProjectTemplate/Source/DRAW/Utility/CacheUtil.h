#pragma once
#include "../DrawComponents.h"

static inline void MakeTextureKey(const std::string& path, std::string& outString) {
	size_t fileSeparator = path.find_last_of('\\');
	size_t extension = path.find_last_of('.');
	outString = path.substr(fileSeparator + 1, path.length() - (fileSeparator+1) - (path.length() - extension));
}

static inline void MakeFontKey(const std::string& path, int pixelSize, std::string& outString) {
	size_t fileSeparator = path.find_last_of('\\');
	size_t extension = path.find_last_of('.');
	outString = path.substr(fileSeparator + 1, path.length() - (fileSeparator+1) - (path.length() - extension)) + "|" + std::to_string(pixelSize);
}