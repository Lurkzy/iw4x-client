#include "STDInclude.hpp"

namespace Components
{
	std::mutex FileSystem::Mutex;
	std::recursive_mutex FileSystem::FSMutex;
	Utils::Memory::Allocator FileSystem::MemAllocator;

	void FileSystem::File::read()
	{
		char* _buffer = nullptr;
		int size = Game::FS_ReadFile(this->filePath.data(), &_buffer);

		this->buffer.clear();

		if (size >= 0)
		{
			this->buffer.append(_buffer, size);
			Game::FS_FreeFile(_buffer);
		}
	}

	void FileSystem::RawFile::read()
	{
		this->buffer.clear();

		Game::RawFile* rawfile = Game::DB_FindXAssetHeader(Game::XAssetType::ASSET_TYPE_RAWFILE, this->filePath.data()).rawfile;
		if (!rawfile || Game::DB_IsXAssetDefault(Game::XAssetType::ASSET_TYPE_RAWFILE, this->filePath.data())) return;

		this->buffer.resize(Game::DB_GetRawFileLen(rawfile));
		Game::DB_GetRawBuffer(rawfile, const_cast<char*>(this->buffer.data()), this->buffer.size());
	}

	FileSystem::FileReader::FileReader(std::string file) : name(file), handle(0)
	{
		this->size = Game::FS_FOpenFileReadCurrentThread(this->name.data(), &this->handle);
	}

	FileSystem::FileReader::~FileReader()
	{
		if (this->exists() && this->handle)
		{
			Game::FS_FCloseFile(this->handle);
		}
	}

	bool FileSystem::FileReader::exists()
	{
		return (this->size >= 0 && this->handle);
	}

	std::string FileSystem::FileReader::getName()
	{
		return this->name;
	}

	int FileSystem::FileReader::getSize()
	{
		return this->size;
	}

	std::string FileSystem::FileReader::getBuffer()
	{
		Utils::Memory::Allocator allocator;
		if (!this->exists()) return std::string();

		int position = Game::FS_FTell(this->handle);
		this->seek(0, FS_SEEK_SET);

		char* buffer = allocator.allocateArray<char>(this->size);
		if (!this->read(buffer, this->size))
		{
			this->seek(position, FS_SEEK_SET);
			return std::string();
		}

		this->seek(position, FS_SEEK_SET);

		return std::string(buffer, this->size);
	}

	bool FileSystem::FileReader::read(void* buffer, size_t _size)
	{
		if (!this->exists() || static_cast<size_t>(this->size) < _size || Game::FS_Read(buffer, _size, this->handle) != static_cast<int>(_size))
		{
			return false;
		}

		return true;
	}

	void FileSystem::FileReader::seek(int offset, int origin)
	{
		if (this->exists())
		{
			Game::FS_Seek(this->handle, offset, origin);
		}
	}

	void FileSystem::FileWriter::write(std::string data)
	{
		if (this->handle)
		{
			Game::FS_Write(data.data(), data.size(), this->handle);
		}
	}

	void FileSystem::FileWriter::open(bool append)
	{
		if (append)
		{
			this->handle = Game::FS_FOpenFileAppend(this->filePath.data());
		}
		else
		{
			this->handle = Game::FS_FOpenFileWrite(this->filePath.data());
		}
	}

	void FileSystem::FileWriter::close()
	{
		if (this->handle)
		{
			Game::FS_FCloseFile(this->handle);
			this->handle = 0;
		}
	}

	std::vector<std::string> FileSystem::GetFileList(std::string path, std::string extension)
	{
		std::vector<std::string> fileList;

		int numFiles = 0;
		char** files = Game::FS_GetFileList(path.data(), extension.data(), Game::FS_LIST_PURE_ONLY, &numFiles, 0);

		if (files)
		{
			for (int i = 0; i < numFiles; ++i)
			{
				if (files[i])
				{
					fileList.push_back(files[i]);
				}
			}

			Game::FS_FreeFileList(files);
		}

		return fileList;
	}

	std::vector<std::string> FileSystem::GetSysFileList(std::string path, std::string extension, bool folders)
	{
		std::vector<std::string> fileList;

		int numFiles = 0;
		char** files = Game::Sys_ListFiles(path.data(), extension.data(), NULL, &numFiles, folders);

		if (files)
		{
			for (int i = 0; i < numFiles; ++i)
			{
				if (files[i])
				{
					fileList.push_back(files[i]);
				}
			}

			Game::Sys_FreeFileList(files);
		}

		return fileList;
	}

	void FileSystem::DeleteFile(std::string folder, std::string file)
	{
		char path[MAX_PATH] = { 0 };
		Game::FS_BuildPathToFile(Dvar::Var("fs_basepath").get<const char*>(), reinterpret_cast<char*>(0x63D0BB8), Utils::String::VA("%s/%s", folder.data(), file.data()), reinterpret_cast<char**>(&path));
		Game::FS_Remove(path);
	}

	int FileSystem::ReadFile(const char* path, char** buffer)
	{
		if (!buffer) return -1;
		else *buffer = nullptr;
		if (!path) return -1;

		std::lock_guard<std::mutex> _(FileSystem::Mutex);
		FileSystem::FileReader reader(path);

		int size = reader.getSize();
		if (reader.exists() && size >= 0)
		{
			*buffer = FileSystem::AllocateFile(size + 1);
			if (reader.read(*buffer, size)) return size;

			FileSystem::FreeFile(*buffer);
			*buffer = nullptr;
		}

		return -1;
	}

	char* FileSystem::AllocateFile(int size)
	{
		return FileSystem::MemAllocator.allocateArray<char>(size);
	}

	void FileSystem::FreeFile(void* buffer)
	{
		FileSystem::MemAllocator.free(buffer);
	}

	void FileSystem::RegisterFolder(const char* folder)
	{
		std::string fs_cdpath = Dvar::Var("fs_cdpath").get<std::string>();
		std::string fs_basepath = Dvar::Var("fs_basepath").get<std::string>();
		std::string fs_homepath = Dvar::Var("fs_homepath").get<std::string>();

		if (!fs_cdpath.empty())   Game::FS_AddLocalizedGameDirectory(fs_cdpath.data(),   folder);
		if (!fs_basepath.empty()) Game::FS_AddLocalizedGameDirectory(fs_basepath.data(), folder);
		if (!fs_homepath.empty()) Game::FS_AddLocalizedGameDirectory(fs_homepath.data(), folder);
	}

	void FileSystem::RegisterFolders()
	{
		if (ZoneBuilder::IsEnabled())
		{
			FileSystem::RegisterFolder("zonedata");
		}

		FileSystem::RegisterFolder("userraw");
	}

	__declspec(naked) void FileSystem::StartupStub()
	{
		__asm
		{
			pushad
			push esi
			call FileSystem::RegisterFolders
			pop esi
			popad

			mov edx, ds:63D0CC0h

			push 48264Dh
			retn
		}
	}

	int FileSystem::ExecIsFSStub(const char* execFilename)
	{
		return !File(execFilename).exists();
	}

	void FileSystem::FsRestartSync(int a1, int a2)
	{
		std::lock_guard<std::recursive_mutex> _(FileSystem::FSMutex);
		return Utils::Hook::Call<void(int, int)>(0x461A50)(a1, a2); // FS_Restart
	}

	void FileSystem::DelayLoadImagesSync()
	{
		std::lock_guard<std::recursive_mutex> _(FileSystem::FSMutex);
		return Utils::Hook::Call<void()>(0x494060)(); // DB_LoadDelayedImages
	}

	int FileSystem::LoadTextureSync(Game::GfxImageLoadDef **loadDef, Game::GfxImage *image)
	{
		std::lock_guard<std::recursive_mutex> _(FileSystem::FSMutex);
		return Game::Load_Texture(loadDef, image);
	}

	FileSystem::FileSystem()
	{
		FileSystem::MemAllocator.clear();

		// Thread safe file system interaction
		Utils::Hook(0x4F4BFF, FileSystem::AllocateFile, HOOK_CALL).install()->quick();
		//Utils::Hook(Game::FS_ReadFile, FileSystem::ReadFile, HOOK_JUMP).install()->quick();
		Utils::Hook(Game::FS_FreeFile, FileSystem::FreeFile, HOOK_JUMP).install()->quick();

		// Filesystem config checks
		Utils::Hook(0x6098FD, FileSystem::ExecIsFSStub, HOOK_CALL).install()->quick();

		// Register additional folders
		Utils::Hook(0x482647, FileSystem::StartupStub, HOOK_JUMP).install()->quick();

		// exec whitelist removal (YAYFINITY WARD)
		Utils::Hook::Nop(0x609685, 5);
		Utils::Hook::Nop(0x60968C, 2);

		// ignore 'no iwd files found in main'
		Utils::Hook::Nop(0x642A4B, 5);

		// Ignore bad magic, when trying to free hunk when it's already cleared
		Utils::Hook::Set<WORD>(0x49AACE, 0xC35E);

		// Synchronize filesystem restarts
		Utils::Hook(0x4A745B, FileSystem::FsRestartSync, HOOK_CALL).install()->quick(); // SV_SpawnServer
		Utils::Hook(0x4C8609, FileSystem::FsRestartSync, HOOK_CALL).install()->quick(); // FS_ConditionalRestart
		Utils::Hook(0x5AC68E, FileSystem::FsRestartSync, HOOK_CALL).install()->quick();

		// Synchronize db image loading
		Utils::Hook(0x415AB8, FileSystem::DelayLoadImagesSync, HOOK_CALL).install()->quick();
		Utils::Hook(0x4D32BC, FileSystem::LoadTextureSync, HOOK_CALL).install()->quick();
	}

	FileSystem::~FileSystem()
	{
		assert(FileSystem::MemAllocator.empty());
	}
}
