#pragma once

#include <string>
#include <memory>
#include "ResourcePackInfo.h"
#include "SignatureState.h"
#include "../IO/InputStream.h"

class ResourcePack
{
public:
	ResourcePack() = default;
	ResourcePack(const char* file) : file(file), sign(std::make_unique<SignatureState>(::NOT_SIGNED)) {}
	
	static std::shared_ptr<ResourcePack> LoadPack(const char* file);
	bool IsFormatSupported(int format)
	{
		return format >= 0 && format <= 1;
	}
	const char* GetFile() const { return file.c_str(); }
	int GetFormat() const { return (info == nullptr) ? -1 : info->format; }
	const char* GetName() const { return (info == nullptr) ? "Unintialized Pack" : info->name.c_str(); }
	const char* GetAuthor() const { return (info == nullptr) ? "???" : info->author.c_str(); }
	const char* GetVersion() const { return (info == nullptr) ? "0" : info->version.c_str(); }
	const char* GetDescription() const { return (info == nullptr) ? "This message shouldn't appear. Make sure this pack has a valid 'pack.yaml' file and UMP: BE is on the latest stable version." : info->description.c_str(); }
	virtual std::shared_ptr<SignatureState> GetSignature() const { return sign; }
	ResourcePackInfo::NoteInfo* GetNoteInfo() const { return &(info->note); }
	ResourcePackInfo::KeyboardInfo* GetKeyboardInfo() const { return &(info->keyboard); }
	virtual void Init();

	bool operator==(const ResourcePack& other);
	virtual std::shared_ptr<std::ifstream> GetStream(const std::string& name) = 0;
	virtual std::vector<std::string> GetEntries() = 0;
protected:
	std::string file;
	std::unique_ptr<ResourcePackInfo> info;
	std::shared_ptr<SignatureState> sign;
};