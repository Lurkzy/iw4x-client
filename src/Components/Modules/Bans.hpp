#pragma once

namespace Components
{
	class Bans : public Component
	{
	public:
		using banEntry = std::pair<SteamID, Game::netIP_t>;

		Bans();

		static std::unique_lock<Utils::NamedMutex> Lock();

		static void BanClient(Game::client_t* cl, const std::string& reason);
		static void UnbanClient(SteamID id);
		static void UnbanClient(Game::netIP_t ip);

		static bool IsBanned(const banEntry& entry);
		static void InsertBan(const banEntry& entry);

	private:
		struct BanList
		{
			std::vector<SteamID> idList;
			std::vector<Game::netIP_t> ipList;
		};

		static void LoadBans(BanList* list);
		static void SaveBans(const BanList* list);
	};
}
