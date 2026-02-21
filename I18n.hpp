#pragma once
#include "FnvHash.hpp"

std::unordered_map<uint32_t, const wchar_t*> hashToStrMap;

void LoadTranslateData()
{
	auto hRes = FindResourceExW(g_hInst, L"YMO", MAKEINTRESOURCEW(1), GetThreadUILanguage());
	if (hRes)
	{
		const DWORD resSize = SizeofResource(g_hInst, hRes);
		if (resSize < sizeof(uint16_t))
		{
			return;
		}

		auto hResData = LoadResource(g_hInst, hRes);
		if (hResData)
		{
			const auto base = reinterpret_cast<const uint8_t*>(LockResource(hResData));
			if (base)
			{
				const auto readU16 = [](const uint8_t* p) -> uint16_t
				{
					return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
				};
				const auto readU32 = [](const uint8_t* p) -> uint32_t
				{
					return static_cast<uint32_t>(p[0])
						| (static_cast<uint32_t>(p[1]) << 8)
						| (static_cast<uint32_t>(p[2]) << 16)
						| (static_cast<uint32_t>(p[3]) << 24);
				};
				const auto hasNullTerminator = [](const wchar_t* str, size_t maxChars) -> bool
				{
					for (size_t i = 0; i < maxChars; ++i)
					{
						if (str[i] == L'\0')
						{
							return true;
						}
					}
					return false;
				};

				const size_t size = static_cast<size_t>(resSize);
				const uint16_t len = readU16(base);
				const size_t tableOffset = sizeof(uint16_t);
				const size_t entrySize = sizeof(uint32_t) + sizeof(uint16_t);

				if (size < tableOffset + (static_cast<size_t>(len) * entrySize))
				{
					return;
				}

				hashToStrMap.clear();
				hashToStrMap.reserve(len);

				for (uint16_t i = 0; i < len; ++i)
				{
					const size_t entryOffset = tableOffset + (static_cast<size_t>(i) * entrySize);
					const uint32_t hash = readU32(base + entryOffset);
					const uint16_t offset = readU16(base + entryOffset + sizeof(uint32_t));

					if (offset >= size || (offset % sizeof(wchar_t)) != 0)
					{
						continue;
					}

					const size_t bytesRemaining = size - offset;
					const size_t maxChars = bytesRemaining / sizeof(wchar_t);
					if (maxChars == 0)
					{
						continue;
					}

					const auto str = reinterpret_cast<const wchar_t*>(base + offset);
					if (!hasNullTerminator(str, maxChars))
					{
						continue;
					}

					hashToStrMap.emplace(hash, str);
				}
			}
		}
	}
}

const wchar_t* Translate(const wchar_t* str)
{
	static std::unordered_map<const wchar_t*, const wchar_t*> ptrToStrMap;

	auto translation = str;

	auto i = ptrToStrMap.find(str);
	if (i == ptrToStrMap.end())
	{
		auto hash = fnv1a_32(str, wcslen(str) * sizeof(wchar_t));
		auto j = hashToStrMap.find(hash);
		if (j != hashToStrMap.end())
			translation = j->second;

		ptrToStrMap.emplace(str, translation);
	}
	else
		translation = i->second;

	return translation;
}

const wchar_t* TranslateContext(const wchar_t* str, const wchar_t* ctxtStr)
{
	auto translation = Translate(ctxtStr);
	if (translation == ctxtStr)
		return str;
	return translation;
}

#define _(str) Translate(str)
#define C_(ctxt, str) TranslateContext(str, ctxt L"\004" str)
