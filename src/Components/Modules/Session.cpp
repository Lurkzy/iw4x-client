#include "STDInclude.hpp"

namespace Components
{
	std::recursive_mutex Session::Mutex;
	std::unordered_map<Network::Address, Session::Frame> Session::Sessions;
	std::unordered_map<Network::Address, std::queue<std::shared_ptr<Session::Packet>>> Session::PacketQueue;

	Utils::Cryptography::ECC::Key Session::SignatureKey;

	std::map<std::string, Utils::Slot<Network::Callback>> Session::PacketHandlers;

	void Session::Send(Network::Address target, std::string command, std::string data)
	{
		std::lock_guard<std::recursive_mutex> _(Session::Mutex);

		auto queue = Session::PacketQueue.find(target);
		if (queue == Session::PacketQueue.end())
		{
			Session::PacketQueue[target] = std::queue<std::shared_ptr<Session::Packet>>();
			queue = Session::PacketQueue.find(target);
			if (queue == Session::PacketQueue.end()) Logger::Error("Failed to enqueue session packet!\n");
		}

		std::shared_ptr<Session::Packet> packet = std::make_shared<Session::Packet>();
		packet->command = command;
		packet->data = data;
		packet->tries = 0;

		queue->second.push(packet);
	}

	void Session::Handle(std::string packet, Utils::Slot<Network::Callback> callback)
	{
		std::lock_guard<std::recursive_mutex> _(Session::Mutex);
		Session::PacketHandlers[packet] = callback;
	}

	void Session::RunFrame()
	{
		std::lock_guard<std::recursive_mutex> _(Session::Mutex);

		for (auto queue = Session::PacketQueue.begin(); queue != Session::PacketQueue.end();)
		{
			if (queue->second.empty())
			{
				queue = Session::PacketQueue.erase(queue);
				continue;
			}

			std::shared_ptr<Session::Packet> packet = queue->second.front();
			if (!packet->lastTry.has_value() || !packet->tries || (packet->lastTry.has_value() && packet->lastTry->elapsed(SESSION_TIMEOUT)))
			{
				if (packet->tries <= SESSION_MAX_RETRIES)
				{
					packet->tries++;
					packet->lastTry.emplace(Utils::Time::Point());

					Network::SendCommand(queue->first, "sessionSyn");
				}
				else
				{
					queue->second.pop();
				}
			}

			++queue;
		}
	}

	Session::Session()
	{
		Session::SignatureKey = Utils::Cryptography::ECC::GenerateKey(512);

		Scheduler::OnFrame(Session::RunFrame);

		Network::Handle("sessionSyn", [](Network::Address address, std::string data)
		{
			std::lock_guard<std::recursive_mutex> _(Session::Mutex);

			Session::Frame frame;
			frame.challenge = Utils::Cryptography::Rand::GenerateChallenge();
			Session::Sessions[address] = frame;

			Network::SendCommand(address, "sessionAck", frame.challenge);
		});

		Network::Handle("sessionAck", [](Network::Address address, std::string data)
		{
			std::lock_guard<std::recursive_mutex> _(Session::Mutex);

			auto queue = Session::PacketQueue.find(address);
			if (queue == Session::PacketQueue.end()) return;

			if (!queue->second.empty())
			{
				std::shared_ptr<Session::Packet> packet = queue->second.front();
				queue->second.pop();

				Proto::Session::Packet dataPacket;
				dataPacket.set_publickey(Session::SignatureKey.getPublicKey());
				dataPacket.set_signature(Utils::Cryptography::ECC::SignMessage(Session::SignatureKey, data));
				dataPacket.set_command(packet->command);
				dataPacket.set_data(packet->data);

				Network::SendCommand(address, "sessionFin", dataPacket.SerializeAsString());
			}
		});

		Network::Handle("sessionFin", [](Network::Address address, std::string data)
		{
			std::lock_guard<std::recursive_mutex> _(Session::Mutex);

			auto frame = Session::Sessions.find(address);
			if (frame == Session::Sessions.end()) return;

			std::string challenge = frame->second.challenge;
			Session::Sessions.erase(frame);

			Proto::Session::Packet dataPacket;
			if (!dataPacket.ParseFromString(data)) return;

			Utils::Cryptography::ECC::Key publicKey;
			publicKey.set(dataPacket.publickey());

			if (!Utils::Cryptography::ECC::VerifyMessage(publicKey, challenge, dataPacket.signature())) return;

			auto handler = Session::PacketHandlers.find(dataPacket.command());
			if (handler == Session::PacketHandlers.end()) return;

			handler->second(address, dataPacket.data());
		});
	}

	Session::~Session()
	{
		std::lock_guard<std::recursive_mutex> _(Session::Mutex);
		Session::PacketHandlers.clear();
		Session::PacketQueue.clear();

		Session::SignatureKey.free();
	}

	bool Session::unitTest()
	{
		printf("Testing ECDSA key...");
		Utils::Cryptography::ECC::Key key = Utils::Cryptography::ECC::GenerateKey(512);

		if (!key.isValid())
		{
			printf("Error\n");
			printf("ECDSA key seems invalid!\n");
			return false;
		}

		printf("Success\n");
		printf("Testing 10 valid signatures...");

		for (int i = 0; i < 10; ++i)
		{
			std::string message = Utils::Cryptography::Rand::GenerateChallenge();
			std::string signature = Utils::Cryptography::ECC::SignMessage(key, message);

			if (!Utils::Cryptography::ECC::VerifyMessage(key, message, signature))
			{
				printf("Error\n");
				printf("Signature for '%s' (%d) was invalid!\n", message.data(), i);
				return false;
			}
		}

		printf("Success\n");
		printf("Testing 10 invalid signatures...");

		for (int i = 0; i < 10; ++i)
		{
			std::string message = Utils::Cryptography::Rand::GenerateChallenge();
			std::string signature = Utils::Cryptography::ECC::SignMessage(key, message);

			// Invalidate the message...
			++message[Utils::Cryptography::Rand::GenerateInt() % message.size()];

			if (Utils::Cryptography::ECC::VerifyMessage(key, message, signature))
			{
				printf("Error\n");
				printf("Signature for '%s' (%d) was valid? What the fuck? That is absolutely impossible...\n", message.data(), i);
				return false;
			}
		}

		printf("Success\n");
		printf("Testing ECDSA key import...");

		std::string pubKey = key.getPublicKey();
		std::string message = Utils::Cryptography::Rand::GenerateChallenge();
		std::string signature = Utils::Cryptography::ECC::SignMessage(key, message);

		Utils::Cryptography::ECC::Key testKey;
		testKey.set(pubKey);

		if (!Utils::Cryptography::ECC::VerifyMessage(key, message, signature))
		{
			printf("Error\n");
			printf("Verifying signature for message '%s' using imported keys failed!\n", message.data());
			return false;
		}

		printf("Success\n");
		return true;
	}
}
