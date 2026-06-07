#include <fstream>
#include <Windows.h>
#include "JsonPersistence.h"

#include "KeyRouter.h"
#include "lib/json/json.h"

using namespace std;

namespace
{
	wstring stripWs(const wstring& text)
	{
		const size_t first = text.find_first_not_of(L" \t\r\n");
		if (first == wstring::npos) {
			return L"";
		}

		const size_t last = text.find_last_not_of(L" \t\r\n");
		return text.substr(first, last - first + 1);
	}

	string utf8FromWide(const wstring& text)
	{
		if (text.empty()) {
			return {};
		}

		const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		string utf8(sizeNeeded, '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &utf8[0], sizeNeeded, nullptr, nullptr);
		return utf8;
	}

	wstring wideFromUtf8(const string& text)
	{
		if (text.empty()) {
			return {};
		}

		const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
		wstring wide(sizeNeeded, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &wide[0], sizeNeeded);
		return wide;
	}

	template<typename T>
	T extractInt(const Json::Value& node, const char* key, T fallback)
	{
		if (node.isMember(key) && node[key].isInt()) {
			return static_cast<T>(node[key].asInt());
		}
		return fallback;
	}

	bool extractBool(const Json::Value& node, const char* key, bool fallback)
	{
		if (node.isMember(key) && node[key].isBool()) {
			return node[key].asBool();
		}
		return fallback;
	}

	wstring extractString(const Json::Value& node, const char* key)
	{
		if (node.isMember(key) && node[key].isString()) {
			return wideFromUtf8(node[key].asString());
		}
		return {};
	}

	vector<wstring> splitWheelItems(const wstring& text)
	{
		wstringstream stream(text);
		wstring line;
		vector<wstring> items;

		while (getline(stream, line))
		{
			const wstring trimmed = stripWs(line);
			if (!trimmed.empty()) {
				items.push_back(trimmed);
			}
		}

		return items;
	}

	wstring joinWheelItems(const Json::Value& itemsJson)
	{
		if (!itemsJson.isArray()) {
			return {};
		}

		wstring combined;
		for (Json::ArrayIndex index = 0; index < itemsJson.size(); ++index)
		{
			if (!itemsJson[index].isString()) {
				continue;
			}

			const wstring item = stripWs(wideFromUtf8(itemsJson[index].asString()));
			if (item.empty()) {
				continue;
			}

			if (!combined.empty()) {
				combined += L"\r\n";
			}
			combined += item;
		}

		return combined;
	}

	int clampWheelPresetIndex(const int index)
	{
		return (std::max)(0, (std::min)(4, index));
	}

	int clampStreakPresetIndex(const int index)
	{
		return (std::max)(0, (std::min)(1, index));
	}

	int clampWinLossOverlay(const int overlay)
	{
		return (std::max)(0, (std::min)(2, overlay));
	}

	PerkSlot buildFromJson(const Json::Value& buildJson)
	{
		PerkSlot build;

		if (!buildJson.isObject()) {
			return build;
		}

		if (buildJson["name"].isString())
		{
			const wstring buildName = stripWs(wideFromUtf8(buildJson["name"].asString()));
			if (!buildName.empty()) {
				build.label = buildName;
			}
		}

		if (buildJson["perks"].isArray())
		{
			const Json::Value perksJson = buildJson["perks"];
			for (Json::ArrayIndex index = 0; index < perksJson.size() && index < build.perks.size(); ++index)
			{
				if (perksJson[index].isString()) {
					build.perks[index] = wideFromUtf8(perksJson[index].asString());
				}
			}
		}

		return build;
	}

	std::vector<PerkSlot> buildsFromJson(const Json::Value& buildsJson)
	{
		std::vector<PerkSlot> builds;
		if (!buildsJson.isArray()) {
			return builds;
		}

		for (Json::ArrayIndex index = 0; index < buildsJson.size(); ++index) {
			builds.push_back(buildFromJson(buildsJson[index]));
		}

		return builds;
	}

	Json::Value buildToJson(const PerkSlot& build)
	{
		Json::Value buildJson;
		buildJson["name"] = utf8FromWide(build.label);
		buildJson["perks"] = Json::arrayValue;

		for (const wstring& perk : build.perks) {
			buildJson["perks"].append(utf8FromWide(perk));
		}

		return buildJson;
	}

	Json::Value buildsToJson(const std::vector<PerkSlot>& builds)
	{
		Json::Value buildsJson = Json::arrayValue;
		for (const PerkSlot& build : builds) {
			buildsJson.append(buildToJson(build));
		}

		return buildsJson;
	}

	StreakSlot streakPresetFromJson(const Json::Value& presetJson, const wchar_t* fallbackName)
	{
		StreakSlot preset = makeStreakSlot(fallbackName);

		if (!presetJson.isObject()) {
			return preset;
		}

		if (presetJson["name"].isString())
		{
			const wstring presetName = stripWs(wideFromUtf8(presetJson["name"].asString()));
			if (!presetName.empty()) {
				preset.label = presetName;
			}
		}

		if (presetJson["items"].isArray()) {
			preset.entries = joinWheelItems(presetJson["items"]);
		}
		else if (presetJson["itemsText"].isString()) {
			preset.entries = stripWs(wideFromUtf8(presetJson["itemsText"].asString()));
		}

		return preset;
	}

	Json::Value streakPresetToJson(const StreakSlot& preset)
	{
		Json::Value presetJson;
		presetJson["name"] = utf8FromWide(preset.label);
		presetJson["itemsText"] = utf8FromWide(preset.entries);
		presetJson["items"] = Json::arrayValue;
		for (const wstring& item : splitWheelItems(preset.entries)) {
			presetJson["items"].append(utf8FromWide(item));
		}

		return presetJson;
	}
}

AppConfig loadConfig()
{
	std::ifstream file(CONFIG_FILE, ios::binary);
	Json::Value rootNode;
	AppConfig settings;

	{
		Json::CharReaderBuilder builder;
		std::string errorMsg;
		std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		Json::parseFromStream(builder, file, &rootNode, &errorMsg);
	}

	settings.timerOneKey = extractInt(rootNode, "kb_slot1", settings.timerOneKey);
	settings.timerTwoKey = extractInt(rootNode, "kb_slot2", settings.timerTwoKey);

	settings.padStartKey = extractInt(rootNode, "gp_begin", settings.padStartKey);
	settings.padTimerOneKey = extractInt(rootNode, "gp_slot1", settings.padTimerOneKey);
	settings.padTimerTwoKey = extractInt(rootNode, "gp_slot2", settings.padTimerTwoKey);
	settings.padStartNrKey = extractInt(rootNode, "gp_begin_hold", settings.padStartNrKey);

	settings.flagSeeThrough = extractBool(rootNode, "vis_translucent", settings.flagSeeThrough);
	settings.flagAutoStart = extractBool(rootNode, "gen_autofire", settings.flagAutoStart);
	settings.flagCountUp = extractBool(rootNode, "gen_stopwatch", settings.flagCountUp);
	settings.flagTimerImg = extractBool(rootNode, "vis_custom_bmp", settings.flagTimerImg);

	const int panelStyle = extractInt(rootNode, "vis_panel", static_cast<int>(settings.flagBgStyle));
	settings.flagBgStyle = static_cast<unsigned int>((std::max)(0, (std::min)(2, panelStyle)));

	settings.timerOneImgPath = extractString(rootNode, "vis_bmp_a");
	settings.timerTwoImgPath = extractString(rootNode, "vis_bmp_b");

	settings.flagPassThru = false;

	settings.timerOneDuration = extractInt(rootNode, "dur_slot1", settings.timerOneDuration);
	settings.timerTwoDuration = extractInt(rootNode, "dur_slot2", settings.timerTwoDuration);

	if (rootNode["roulette"].isObject())
	{
		const Json::Value rouletteNode = rootNode["roulette"];
		if (rouletteNode["hub_bmp"].isString()) {
			settings.wheelData.centerImg = wideFromUtf8(rouletteNode["hub_bmp"].asString());
		}

		if (rouletteNode["slots"].isArray())
		{
			const Json::Value slotsNode = rouletteNode["slots"];
			for (Json::ArrayIndex index = 0; index < slotsNode.size() && index < settings.wheelData.presets.size(); ++index)
			{
				const Json::Value slotNode = slotsNode[index];
				if (!slotNode.isObject()) {
					continue;
				}

				if (slotNode["tag"].isString())
				{
					const wstring slotTag = stripWs(wideFromUtf8(slotNode["tag"].asString()));
					if (!slotTag.empty()) {
						settings.wheelData.presets[index].label = slotTag;
					}
				}

				if (slotNode["entries"].isArray()) {
					settings.wheelData.presets[index].entries = joinWheelItems(slotNode["entries"]);
				}
				else if (slotNode["entries_raw"].isString()) {
					settings.wheelData.presets[index].entries = stripWs(wideFromUtf8(slotNode["entries_raw"].asString()));
				}
			}

			if (rouletteNode["active_slot"].isInt()) {
				settings.wheelData.activePreset = clampWheelPresetIndex(rouletteNode["active_slot"].asInt());
			}

			settings.wheelData.entriesText = settings.wheelData.presets[settings.wheelData.activePreset].entries;
		}
		else
		{
			if (rouletteNode["entries"].isArray()) {
				settings.wheelData.entriesText = joinWheelItems(rouletteNode["entries"]);
			}
			else if (rouletteNode["entries_raw"].isString()) {
				settings.wheelData.entriesText = stripWs(wideFromUtf8(rouletteNode["entries_raw"].asString()));
			}

			settings.wheelData.presets[0].entries = settings.wheelData.entriesText;
			settings.wheelData.activePreset = 0;
		}
	}

	if (rootNode["loadouts"].isObject())
	{
		const Json::Value loadoutsNode = rootNode["loadouts"];
		settings.loadoutData.survivor = buildsFromJson(loadoutsNode["surv"]);
		settings.loadoutData.killer = buildsFromJson(loadoutsNode["kil"]);
	}

	if (rootNode["streaks"].isObject())
	{
		const Json::Value streaksNode = rootNode["streaks"];
		if (streaksNode["view"].isInt()) {
			settings.streakData.activeOverlay = clampWinLossOverlay(streaksNode["view"].asInt());
		}

		if (streaksNode["sel"].isInt()) {
			settings.streakData.activePreset = clampStreakPresetIndex(streaksNode["sel"].asInt());
		}

		if (streaksNode["slots"].isArray())
		{
			const Json::Value slotsNode = streaksNode["slots"];
			const wchar_t* defaultTags[] = { L"SideSurvivor", L"SideKiller" };
			for (Json::ArrayIndex index = 0; index < slotsNode.size() && index < settings.streakData.presets.size(); ++index) {
				settings.streakData.presets[index] = streakPresetFromJson(slotsNode[index], defaultTags[index]);
			}
		}
	}

	if (rootNode["bubbles"].isObject())
	{
		const Json::Value bubblesNode = rootNode["bubbles"];
		settings.notifyProfile.toxicChatEnabled = false;
		if (bubblesNode["hotkey"].isInt()) {
			settings.notifyProfile.toxicChatKey = bubblesNode["hotkey"].asInt();
		}
		if (bubblesNode["timed"].isBool()) {
			settings.notifyProfile.toxicChatTimed = bubblesNode["timed"].asBool();
		}
		if (bubblesNode["duration"].isInt() && bubblesNode["duration"].asInt() > 0) {
			settings.notifyProfile.toxicChatDurationMs = bubblesNode["duration"].asInt();
		}
		if (bubblesNode["bmp"].isString()) {
			settings.notifyProfile.toxicChatImg = wideFromUtf8(bubblesNode["bmp"].asString());
		}
	}

	Json::Value paletteNode = rootNode["palette"];
	if (paletteNode["primary"].isInt() && paletteNode["accent"].isInt()
		&& paletteNode["warning"].isInt() && paletteNode["bg"].isInt())
	{
		settings.tintSelection.timerOneColor = paletteNode["primary"].asInt();
		settings.tintSelection.timerTwoColor = paletteNode["accent"].asInt();
		settings.tintSelection.urgentColor = paletteNode["warning"].asInt();
		settings.tintSelection.backdropColor = paletteNode["bg"].asInt();

		if (settings.tintSelection.timerOneColor >= TINT_PALETTE_SZ || settings.tintSelection.timerTwoColor >= TINT_PALETTE_SZ ||
			settings.tintSelection.urgentColor >= TINT_PALETTE_SZ || settings.tintSelection.backdropColor >= TINT_PALETTE_SZ ||
			settings.tintSelection.timerOneColor < 0 || settings.tintSelection.timerTwoColor < 0 ||
			settings.tintSelection.urgentColor < 0 || settings.tintSelection.backdropColor < 0)
		{
			settings.tintSelection.timerOneColor = 3;
			settings.tintSelection.timerTwoColor = 5;
			settings.tintSelection.urgentColor = 8;
			settings.tintSelection.backdropColor = 20;
		}
	}

	return settings;
}

void saveConfig(const AppConfig& settings)
{
	Json::Value doc;

	doc["kb_slot1"] = settings.timerOneKey;
	doc["kb_slot2"] = settings.timerTwoKey;

	doc["gp_begin"] = settings.padStartKey;
	doc["gp_slot1"] = settings.padTimerOneKey;
	doc["gp_slot2"] = settings.padTimerTwoKey;
	doc["gp_begin_hold"] = settings.padStartNrKey;

	doc["vis_translucent"] = settings.flagSeeThrough;
	doc["gen_autofire"] = settings.flagAutoStart;
	doc["gen_stopwatch"] = settings.flagCountUp;
	doc["vis_panel"] = (std::max)(0, (std::min)(2, static_cast<int>(settings.flagBgStyle)));
	doc["vis_custom_bmp"] = settings.flagTimerImg;
	doc["vis_bmp_a"] = utf8FromWide(settings.timerOneImgPath);
	doc["vis_bmp_b"] = utf8FromWide(settings.timerTwoImgPath);
	doc["dur_slot1"] = settings.timerOneDuration;
	doc["dur_slot2"] = settings.timerTwoDuration;
	doc["roulette"]["hub_bmp"] = utf8FromWide(settings.wheelData.centerImg);
	doc["roulette"]["active_slot"] = clampWheelPresetIndex(settings.wheelData.activePreset);

	doc["roulette"]["entries"] = Json::arrayValue;
	const int activeSlot = clampWheelPresetIndex(settings.wheelData.activePreset);
	const wstring activeSlotEntries = settings.wheelData.presets[activeSlot].entries;
	doc["roulette"]["entries_raw"] = utf8FromWide(activeSlotEntries);
	for (const wstring& item : splitWheelItems(activeSlotEntries)) {
		doc["roulette"]["entries"].append(utf8FromWide(item));
	}

	doc["roulette"]["slots"] = Json::arrayValue;
	for (const WheelSlot& slot : settings.wheelData.presets)
	{
		Json::Value slotJson;
		slotJson["tag"] = utf8FromWide(slot.label);
		slotJson["entries"] = Json::arrayValue;
		slotJson["entries_raw"] = utf8FromWide(slot.entries);
		for (const wstring& item : splitWheelItems(slot.entries)) {
			slotJson["entries"].append(utf8FromWide(item));
		}

		doc["roulette"]["slots"].append(slotJson);
	}

	doc["palette"]["primary"] = settings.tintSelection.timerOneColor;
	doc["palette"]["accent"] = settings.tintSelection.timerTwoColor;
	doc["palette"]["warning"] = settings.tintSelection.urgentColor;
	doc["palette"]["bg"] = settings.tintSelection.backdropColor;

	doc["loadouts"]["surv"] = buildsToJson(settings.loadoutData.survivor);
	doc["loadouts"]["kil"] = buildsToJson(settings.loadoutData.killer);
	doc["streaks"]["view"] = clampWinLossOverlay(settings.streakData.activeOverlay);
	doc["streaks"]["sel"] = clampStreakPresetIndex(settings.streakData.activePreset);
	doc["streaks"]["slots"] = Json::arrayValue;
	for (const StreakSlot& slot : settings.streakData.presets) {
		doc["streaks"]["slots"].append(streakPresetToJson(slot));
	}
	doc["bubbles"]["enabled"] = false;
	doc["bubbles"]["hotkey"] = settings.notifyProfile.toxicChatKey;
	doc["bubbles"]["timed"] = settings.notifyProfile.toxicChatTimed;
	doc["bubbles"]["duration"] = settings.notifyProfile.toxicChatDurationMs;
	doc["bubbles"]["bmp"] = utf8FromWide(settings.notifyProfile.toxicChatImg);

	Json::StreamWriterBuilder fmt;
	fmt["commentStyle"] = "None";
	fmt["indentation"] = "   ";
	const std::string serialized = Json::writeString(fmt, doc);
	{
		std::ofstream out(CONFIG_FILE, ios::trunc | ios::binary);
		out.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
	}
}

void createDefaults()
{
	TintPalette defaultColors;
	AppConfig defaultSettings;
	defaultSettings.tintSelection = defaultColors;

	Json::Value doc;
	
	doc["kb_slot1"] = defaultSettings.timerOneKey;
	doc["kb_slot2"] = defaultSettings.timerTwoKey;

	doc["gp_begin"] = defaultSettings.padStartKey;
	doc["gp_slot1"] = defaultSettings.padTimerOneKey;
	doc["gp_slot2"] = defaultSettings.padTimerTwoKey;
	doc["gp_begin_hold"] = defaultSettings.padStartNrKey;

	doc["vis_translucent"] = defaultSettings.flagSeeThrough;
	doc["gen_autofire"] = defaultSettings.flagAutoStart;
	doc["gen_stopwatch"] = defaultSettings.flagCountUp;
	doc["vis_panel"] = defaultSettings.flagBgStyle;
	doc["vis_custom_bmp"] = defaultSettings.flagTimerImg;
	doc["vis_bmp_a"] = utf8FromWide(defaultSettings.timerOneImgPath);
	doc["vis_bmp_b"] = utf8FromWide(defaultSettings.timerTwoImgPath);
	doc["dur_slot1"] = defaultSettings.timerOneDuration;
	doc["dur_slot2"] = defaultSettings.timerTwoDuration;
	doc["roulette"]["hub_bmp"] = utf8FromWide(defaultSettings.wheelData.centerImg);
	doc["roulette"]["active_slot"] = defaultSettings.wheelData.activePreset;
	doc["roulette"]["entries"] = Json::arrayValue;
	for (const wstring& item : splitWheelItems(defaultSettings.wheelData.entriesText)) {
		doc["roulette"]["entries"].append(utf8FromWide(item));
	}
	doc["roulette"]["entries_raw"] = utf8FromWide(defaultSettings.wheelData.entriesText);
	doc["roulette"]["slots"] = Json::arrayValue;
	for (const WheelSlot& slot : defaultSettings.wheelData.presets)
	{
		Json::Value slotJson;
		slotJson["tag"] = utf8FromWide(slot.label);
		slotJson["entries"] = Json::arrayValue;
		slotJson["entries_raw"] = utf8FromWide(slot.entries);
		for (const wstring& item : splitWheelItems(slot.entries)) {
			slotJson["entries"].append(utf8FromWide(item));
		}

		doc["roulette"]["slots"].append(slotJson);
	}

	doc["palette"]["primary"] = defaultSettings.tintSelection.timerOneColor;
	doc["palette"]["accent"] = defaultSettings.tintSelection.timerTwoColor;
	doc["palette"]["warning"] = defaultSettings.tintSelection.urgentColor;
	doc["palette"]["bg"] = defaultSettings.tintSelection.backdropColor;
	doc["loadouts"]["surv"] = buildsToJson(defaultSettings.loadoutData.survivor);
	doc["loadouts"]["kil"] = buildsToJson(defaultSettings.loadoutData.killer);
	doc["streaks"]["view"] = defaultSettings.streakData.activeOverlay;
	doc["streaks"]["sel"] = defaultSettings.streakData.activePreset;
	doc["streaks"]["slots"] = Json::arrayValue;
	for (const StreakSlot& slot : defaultSettings.streakData.presets) {
		doc["streaks"]["slots"].append(streakPresetToJson(slot));
	}
	doc["bubbles"]["enabled"] = defaultSettings.notifyProfile.toxicChatEnabled;
	doc["bubbles"]["hotkey"] = defaultSettings.notifyProfile.toxicChatKey;
	doc["bubbles"]["timed"] = defaultSettings.notifyProfile.toxicChatTimed;
	doc["bubbles"]["duration"] = defaultSettings.notifyProfile.toxicChatDurationMs;
	doc["bubbles"]["bmp"] = utf8FromWide(defaultSettings.notifyProfile.toxicChatImg);

	Json::StreamWriterBuilder fmt;
	fmt["commentStyle"] = "None";
	fmt["indentation"] = "  ";
	const std::string payload = Json::writeString(fmt, doc);
	std::ofstream outputFileStream(CONFIG_FILE, ios::trunc | ios::binary);
	outputFileStream.write(payload.data(), static_cast<std::streamsize>(payload.size()));
}

bool configExists() {
	const ifstream probe(CONFIG_FILE, ios::binary);
	return probe.is_open() && probe.good();
}

void applyLive(const AppConfig& settings) {
	appSettings_ = settings;
	KeyRouter::loadKeys(appSettings_);

	if (overlayHwnd_ == nullptr) {
		return;
	}

	const BYTE alpha = appSettings_.flagSeeThrough ? 0 : 255;
	const DWORD layerFlag = appSettings_.flagSeeThrough ? LWA_COLORKEY : LWA_ALPHA;
	SetLayeredWindowAttributes(overlayHwnd_, 0, alpha, layerFlag);

	DWORD exStyle = static_cast<DWORD>(GetWindowLongPtr(overlayHwnd_, GWL_EXSTYLE));
	if (appSettings_.flagPassThru)
		exStyle |= WS_EX_TRANSPARENT;
	else
		exStyle &= ~WS_EX_TRANSPARENT;

	SetWindowLongPtr(overlayHwnd_, GWL_EXSTYLE, static_cast<LONG>(exStyle));
	SetWindowPos(overlayHwnd_, nullptr, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

void applySave(const AppConfig& settings) {
	saveConfig(settings);
	applyLive(settings);
}
