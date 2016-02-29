#include "STDInclude.hpp"

namespace Components
{
	int AntiCheat::LastCheck;
	std::string AntiCheat::Hash;

	// This function does nothing, it only adds the two passed variables and returns the value
	// The only important thing it does is to clean the first parameter, and then return
	// By returning, the crash procedure will be called, as it hasn't been cleaned from the stack
	void __declspec(naked) AntiCheat::NullSub()
	{
		__asm
		{
			push ebp
			push ecx
			mov ebp, esp

			xor eax, eax
			mov eax, [ebp + 8h]
			mov ecx, [ebp + 0Ch]
			add eax, ecx

			pop ecx
			pop ebp
			retn 4
		}
	}

	void __declspec(naked) AntiCheat::CrashClient()
	{
		static uint8_t crashProcedure[] =
		{
			// Variable space
			0xDC, 0xC1, 0xDC, 0x05, 

			// Uninstall minidump handler
			0xB8, 0x63, 0xE7, 0x2F, 0x00,           // mov  eax, 2FE763h
			0x05, 0xAD, 0xAD, 0x3C, 0x00,           // add  eax, 3CADADh
			0x6A, 0x58,                             // push 88
			0x8B, 0x80, 0xEA, 0x01, 0x00, 0x00,     // mov  eax, [eax + 1EAh]
			0xFF, 0x10,                             // call dword ptr [eax]

			// Crash me.
			0xB8, 0x4F, 0x91, 0x27, 0x00,           // mov  eax, 27914Fh
			0x05, 0xDD, 0x28, 0x1A, 0x00,           // add  eax, 1A28DDh
			0x80, 0x00, 0x68,                       // add  byte ptr [eax], 68h
			0xC3,                                   // retn

			// Random stuff
			0xBE, 0xFF, 0xC2, 0xF4, 0x3A,
		};

		__asm
		{
			// This does absolutely nothing :P
			xor eax, eax
			mov ebx, [esp + 4h]
			shl ebx, 4h
			setz bl

			// Push the fake var onto the stack
			push ebx

			// Get address to VirtualProtect
			mov eax, 6567h
			shl eax, 0Ch
			or eax, 70000A50h

			// Move the address into ebx
			push eax 
			pop ebx

			// Save the address to our crash procedure
			mov eax, offset crashProcedure
			push eax

			// Unprotect the .text segment
			push eax
			push 40h
			push 2D5FFFh
			push 401001h
			call ebx

			// Increment to our crash procedure
			add dword ptr [esp], 4h

			// This basically removes the pushed ebx value from the stack, so returning results in a call to the procedure
			jmp AntiCheat::NullSub
		}
	}

	// This has to be called when doing .text changes during runtime
	void AntiCheat::EmptyHash()
	{
		AntiCheat::LastCheck = 0;
		AntiCheat::Hash.clear();
	}

	void AntiCheat::PerformCheck()
	{
		// Hash .text segment
		// Add 1 to each value, so searching in memory doesn't reveal anything
		size_t textSize = 0x2D6001;
		uint8_t* textBase = reinterpret_cast<uint8_t*>(0x401001);
		std::string hash = Utils::Cryptography::SHA512::Compute(textBase - 1, textSize - 1, false);

		// Set the hash, if none is set
		if (AntiCheat::Hash.empty())
		{
			AntiCheat::Hash = hash;
		}
		// Crash if the hashes don't match
		else if (AntiCheat::Hash != hash)
		{
			AntiCheat::CrashClient();
		}
	}

	void AntiCheat::Frame()
	{
		// Perform check only every 30 seconds
		if (AntiCheat::LastCheck && (Game::Com_Milliseconds() - AntiCheat::LastCheck) < 1000 * 30) return;
		AntiCheat::LastCheck = Game::Com_Milliseconds();

		AntiCheat::PerformCheck();
	}

	AntiCheat::AntiCheat()
	{
		AntiCheat::EmptyHash();

		Renderer::OnFrame(AntiCheat::Frame);
		Dedicated::OnFrame(AntiCheat::Frame);

#ifdef DEBUG
		Command::Add("penis", [] (Command::Params)
		{
			AntiCheat::CrashClient();
		});
#endif
	}

	AntiCheat::~AntiCheat()
	{
		AntiCheat::EmptyHash();
	}
}
