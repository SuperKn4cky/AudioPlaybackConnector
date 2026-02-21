#pragma once

constexpr auto CONFIG_NAME = L"AudioPlaybackConnector.json";
constexpr auto APPDATA_DIR_NAME = L"AudioPlaybackConnector";
constexpr auto BUFFER_SIZE = 4096;
constexpr size_t MAX_CONFIG_SIZE = 1024 * 1024;

fs::path GetLegacySettingsPath()
{
	return GetModuleFsPath(g_hInst).remove_filename() / CONFIG_NAME;
}

fs::path GetSettingsPath()
{
	const wchar_t* appData = _wgetenv(L"APPDATA");
	if (appData && appData[0] != L'\0')
	{
		return fs::path(appData) / APPDATA_DIR_NAME / CONFIG_NAME;
	}

	return GetLegacySettingsPath();
}

fs::path ResolveSettingsLoadPath()
{
	const auto preferredPath = GetSettingsPath();
	std::error_code ec;
	if (fs::exists(preferredPath, ec) && !ec)
	{
		return preferredPath;
	}

	const auto legacyPath = GetLegacySettingsPath();
	ec.clear();
	if (fs::exists(legacyPath, ec) && !ec)
	{
		return legacyPath;
	}

	return preferredPath;
}

std::wstring BuildPrettySettingsJson(const std::vector<std::wstring>& lastDeviceIds)
{
	std::wstring json;
	json.reserve(256 + lastDeviceIds.size() * 64);
	json += L"{\n";
	json += L"  \"reconnect\": ";
	json += g_reconnect ? L"true" : L"false";
	json += L",\n";
	json += L"  \"showNotification\": ";
	json += g_showNotification ? L"true" : L"false";
	json += L",\n";
	json += L"  \"lastDevices\": [";
	if (!lastDeviceIds.empty())
	{
		json += L"\n";
	}

	for (size_t i = 0; i < lastDeviceIds.size(); ++i)
	{
		json += L"    ";
		json += std::wstring(JsonValue::CreateStringValue(lastDeviceIds[i]).Stringify());
		if (i + 1 < lastDeviceIds.size())
		{
			json += L",";
		}
		json += L"\n";
	}

	if (!lastDeviceIds.empty())
	{
		json += L"  ";
	}
	json += L"]\n";
	json += L"}\n";
	return json;
}

void DefaultSettings()
{
	g_reconnect = false;
	g_showNotification = true;
	g_lastDevices.clear();
}

void LoadSettings()
{
	try
	{
		DefaultSettings();

		const auto settingsPath = ResolveSettingsLoadPath();
		wil::unique_hfile hFile(CreateFileW(settingsPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
		THROW_LAST_ERROR_IF(!hFile);

		std::string string;
		while (1)
		{
			size_t size = string.size();
			THROW_HR_IF(E_BOUNDS, size >= MAX_CONFIG_SIZE);
			const DWORD bytesToRead = static_cast<DWORD>(std::min<size_t>(BUFFER_SIZE, MAX_CONFIG_SIZE - size));
			string.resize(size + bytesToRead);
			DWORD read = 0;
			THROW_IF_WIN32_BOOL_FALSE(ReadFile(hFile.get(), string.data() + size, bytesToRead, &read, nullptr));
			string.resize(size + read);
			if (read == 0)
				break;
		}

		std::wstring utf16 = Utf8ToUtf16(string);
		auto jsonObj = JsonObject::Parse(utf16);
		g_reconnect = jsonObj.Lookup(L"reconnect").GetBoolean();
		
		if (jsonObj.HasKey(L"showNotification"))
			g_showNotification = jsonObj.Lookup(L"showNotification").GetBoolean();

		auto lastDevices = jsonObj.Lookup(L"lastDevices").GetArray();
		g_lastDevices.reserve(lastDevices.Size());
		for (const auto& i : lastDevices)
		{
			if (i.ValueType() == JsonValueType::String)
				g_lastDevices.push_back(std::wstring(i.GetString()));
		}
	}
	CATCH_LOG();
}

void SaveSettings()
{
	try
	{
		std::vector<std::wstring> lastDeviceIds;
		{
			std::scoped_lock lock(g_audioPlaybackConnectionsMutex);
			lastDeviceIds.reserve(g_audioPlaybackConnections.size());
			for (const auto& i : g_audioPlaybackConnections)
			{
				lastDeviceIds.push_back(i.first);
			}
		}

		const auto settingsPath = GetSettingsPath();
		const auto settingsDirectory = settingsPath.parent_path();
		if (!settingsDirectory.empty())
		{
			std::error_code ec;
			fs::create_directories(settingsDirectory, ec);
			THROW_HR_IF(E_FAIL, ec);
		}

		wil::unique_hfile hFile(CreateFileW(settingsPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
		THROW_LAST_ERROR_IF(!hFile);

		std::string utf8 = Utf16ToUtf8(BuildPrettySettingsJson(lastDeviceIds));
		DWORD written = 0;
		THROW_IF_WIN32_BOOL_FALSE(WriteFile(hFile.get(), utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr));
		THROW_HR_IF(E_FAIL, written != utf8.size());
	}
	CATCH_LOG();
}
