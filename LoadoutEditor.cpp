#include "LoadoutEditor.h"
#include "JsonPersistence.h"
#include "lib/json/json.h"
#include <CommCtrl.h>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <map>
#include <mmsystem.h>
#include <set>
#include <sstream>
#include <Windowsx.h>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")

namespace
{
	constexpr int kNewBuildButtonId = 301;
	constexpr int kSaveBuildButtonId = 302;
	constexpr int kDeleteBuildButtonId = 303;
	constexpr int kPerkSearchEditId = 304;
	constexpr int kRandomBuildButtonId = 305;
	constexpr int kRandomBuildTimerId = 4301;
	constexpr int kPerkRowHeight = 58;
	constexpr const wchar_t* kIncludedPerksFolder = L"Included Assets\\Builds Survi & Killer";
	constexpr const wchar_t* kBuildRandomStartSoundPath = L"Audio\\build-random-start.wav";
	constexpr const wchar_t* kBuildRandomStartSoundAlias = L"buildStart";
	constexpr const wchar_t* kBuildRandomEndSoundPath = L"Audio\\build-random-end.wav";
	constexpr const wchar_t* kBuildRandomEndSoundAlias = L"buildEnd";


	constexpr COLORREF kBg = RGB(12, 16, 25);
	constexpr COLORREF kPanel = RGB(21, 29, 43);
	constexpr COLORREF kPanelAlt = RGB(15, 22, 34);
	constexpr COLORREF kBorder = RGB(55, 85, 121);
	constexpr COLORREF kAccent = RGB(42, 172, 246);
	constexpr COLORREF kAccentSoft = RGB(134, 224, 255);
	constexpr COLORREF kText = RGB(240, 246, 255);
	constexpr COLORREF kMutedText = RGB(158, 179, 205);
	constexpr COLORREF kDanger = RGB(184, 42, 58);

	COLORREF adjustColor(const COLORREF color, const int amount)
	{
		const int red = (std::max)(0, (std::min)(255, GetRValue(color) + amount));
		const int green = (std::max)(0, (std::min)(255, GetGValue(color) + amount));
		const int blue = (std::max)(0, (std::min)(255, GetBValue(color) + amount));
		return RGB(red, green, blue);
	}

	void addRoundedRectPath(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, const float radius)
	{
		const float clampedRadius = (std::max)(0.0f, (std::min)(radius, (std::min)(rect.Width, rect.Height) / 2.0f));
		const float diameter = clampedRadius * 2.0f;

		path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
		path.AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
		path.AddArc(rect.GetRight() - diameter, rect.GetBottom() - diameter, diameter, diameter, 0.0f, 90.0f);
		path.AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0f, 90.0f);
		path.CloseFigure();
	}

	void drawRoundedPanel(Gdiplus::Graphics& graphics, const RECT& rect, const COLORREF fill, const COLORREF border, const float radius = 14.0f, const float borderWidth = 1.5f)
	{
		Gdiplus::RectF rectF(
			static_cast<float>(rect.left),
			static_cast<float>(rect.top),
			static_cast<float>(rect.right - rect.left),
			static_cast<float>(rect.bottom - rect.top));
		Gdiplus::GraphicsPath path;
		addRoundedRectPath(path, rectF, radius);
		Gdiplus::SolidBrush fillBrush(Gdiplus::Color(255, GetRValue(fill), GetGValue(fill), GetBValue(fill)));
		Gdiplus::Pen borderPen(Gdiplus::Color(255, GetRValue(border), GetGValue(border), GetBValue(border)), borderWidth);
		graphics.FillPath(&fillBrush, &path);
		graphics.DrawPath(&borderPen, &path);
	}

	void drawText(Gdiplus::Graphics& graphics, const std::wstring& text, const RECT& rect, const float size, const int style, const COLORREF color, const Gdiplus::StringAlignment alignment = Gdiplus::StringAlignmentNear)
	{
		Gdiplus::FontFamily family(L"Segoe UI");
		Gdiplus::Font font(&family, size, style, Gdiplus::UnitPixel);
		Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
		Gdiplus::StringFormat format;
		format.SetAlignment(alignment);
		format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
		format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
		Gdiplus::RectF rectF(
			static_cast<float>(rect.left),
			static_cast<float>(rect.top),
			static_cast<float>(rect.right - rect.left),
			static_cast<float>(rect.bottom - rect.top));
		graphics.DrawString(text.c_str(), -1, &font, rectF, &format, &brush);
	}

	void drawWrappedText(Gdiplus::Graphics& graphics, const std::wstring& text, const RECT& rect, const float size, const int style, const COLORREF color)
	{
		Gdiplus::FontFamily family(L"Segoe UI");
		Gdiplus::Font font(&family, size, style, Gdiplus::UnitPixel);
		Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
		Gdiplus::StringFormat format;
		format.SetAlignment(Gdiplus::StringAlignmentNear);
		format.SetLineAlignment(Gdiplus::StringAlignmentNear);
		format.SetTrimming(Gdiplus::StringTrimmingEllipsisWord);
		Gdiplus::RectF rectF(
			static_cast<float>(rect.left),
			static_cast<float>(rect.top),
			static_cast<float>(rect.right - rect.left),
			static_cast<float>(rect.bottom - rect.top));
		graphics.DrawString(text.c_str(), -1, &font, rectF, &format, &brush);
	}

	std::string utf8FromWide(const std::wstring& text)
	{
		if (text.empty()) {
			return {};
		}

		const int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		std::string utf8(sizeNeeded, '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &utf8[0], sizeNeeded, nullptr, nullptr);
		return utf8;
	}

	std::wstring wideFromUtf8(const std::string& text)
	{
		if (text.empty()) {
			return {};
		}

		const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
		std::wstring wide(sizeNeeded, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &wide[0], sizeNeeded);
		return wide;
	}

	bool hasImageExtension(const std::wstring& fileName)
	{
		const size_t dotPos = fileName.find_last_of(L'.');
		if (dotPos == std::wstring::npos) {
			return false;
		}

		std::wstring extension = fileName.substr(dotPos);
		std::transform(extension.begin(), extension.end(), extension.begin(), towlower);
		return extension == L".png" || extension == L".jpg" || extension == L".jpeg" || extension == L".bmp";
	}

	std::wstring lowerText(std::wstring text)
	{
		std::transform(text.begin(), text.end(), text.begin(), towlower);
		return text;
	}

	std::wstring normalizeSearchText(const std::wstring& text)
	{
		std::wstring normalized;
		normalized.reserve(text.size());

		for (wchar_t ch : lowerText(text))
		{
			switch (ch)
			{
			case L'á': case L'à': case L'ä': case L'â': case L'ã':
				ch = L'a';
				break;
			case L'é': case L'è': case L'ë': case L'ê':
				ch = L'e';
				break;
			case L'í': case L'ì': case L'ï': case L'î':
				ch = L'i';
				break;
			case L'ó': case L'ò': case L'ö': case L'ô': case L'õ':
				ch = L'o';
				break;
			case L'ú': case L'ù': case L'ü': case L'û':
				ch = L'u';
				break;
			case L'ñ':
				ch = L'n';
				break;
			default:
				break;
			}

			normalized += (iswalnum(ch) || iswspace(ch)) ? ch : L' ';
		}

		return normalized;
	}

	bool matchesSearchText(const std::wstring& searchText, const std::wstring& query)
	{
		std::wistringstream stream(normalizeSearchText(query));
		std::wstring token;
		bool hasToken = false;
		while (stream >> token)
		{
			hasToken = true;
			if (searchText.find(token) == std::wstring::npos) {
				return false;
			}
		}

		return true;
	}

	std::wstring normalizePerkNameKey(const std::wstring& text)
	{
		std::wstring key;
		for (wchar_t ch : lowerText(text))
		{
			if (iswalnum(ch)) {
				key += ch;
			}
		}
		return key;
	}

	struct PerkTranslation
	{
		const wchar_t* lookupName;
		const wchar_t* englishName;
		const wchar_t* spanishName;
	};

	const PerkTranslation* findPerkTranslation(const std::wstring& name)
	{
		static const PerkTranslation translations[] = {
			{L"A Nurses Calling", L"A Nurse's Calling", L"Vocación de Enfermera"},
			{L"A Nurse's Calling", L"A Nurse's Calling", L"Vocación de Enfermera"},
			{L"Ace In The Hole", L"Ace in the Hole", L"As en la manga"},
			{L"Adrenaline", L"Adrenaline", L"Adrenalina"},
			{L"Aftercare", L"Aftercare", L"Postratamiento"},
			{L"Agitation", L"Agitation", L"Agitación"},
			{L"Alert", L"Alert", L"Alerta"},
			{L"Alien Instinct", L"Alien Instinct", L"Instinto alienígena"},
			{L"All Shaking Thunder", L"All-Shaking Thunder", L"Trueno estremecedor"},
			{L"Any Means Necessary", L"Any Means Necessary", L"Cueste lo que cueste"},
			{L"Apocalyptic Ingenuity", L"Apocalyptic Ingenuity", L"Ingenio apocalíptico"},
			{L"Appraisal", L"Appraisal", L"Evaluación"},
			{L"Autodidact", L"Autodidact", L"Autodidacta"},
			{L"Awakened Awareness", L"Awakened Awareness", L"Percepción activada"},
			{L"Awakened Awarenesss", L"Awakened Awareness", L"Percepción activada"},
			{L"baby Sitter", L"Babysitter", L"Canguro"},
			{L"Babysitter", L"Babysitter", L"Canguro"},
			{L"Background Player", L"Background Player", L"Personaje secundario"},
			{L"Bada Bada Boom", L"Bada Bada Boom", L"Bada Bada Boom"},
			{L"Balanced Landing", L"Balanced Landing", L"Caída equilibrada"},
			{L"Bamboozle", L"Bamboozle", L"Desconcierto"},
			{L"Bardic Inspiration", L"Bardic Inspiration", L"Inspiración bárdica"},
			{L"Batteries Included", L"Batteries Included", L"Pilas incluidas"},
			{L"batteries Included", L"Batteries Included", L"Pilas incluidas"},
			{L"BBQ And Chili", L"Barbecue & Chilli", L"Barbacoa y chile"},
			{L"Barbecue And Chilli", L"Barbecue & Chilli", L"Barbacoa y chile"},
			{L"Beast Of Prey", L"Beast of Prey", L"Bestia de presa"},
			{L"Better Than New", L"Better than New", L"Mejor que nuevo"},
			{L"better Together", L"Better Together", L"Mejor juntos"},
			{L"Better Together", L"Better Together", L"Mejor juntos"},
			{L"Bite The Bullet", L"Bite the Bullet", L"De tripas corazón"},
			{L"Bitter Murmur", L"Bitter Murmur", L"Murmullo amargo"},
			{L"Blast Mine", L"Blast Mine", L"Mina explosiva"},
			{L"Blood Echo", L"Blood Echo", L"Eco sangriento"},
			{L"Blood Pact", L"Blood Pact", L"Pacto de sangre"},
			{L"Blood Rush", L"Blood Rush", L"Impulso sangriento"},
			{L"Blood Warden", L"Blood Warden", L"Guardián de sangre"},
			{L"Bloodhound", L"Bloodhound", L"Sabueso de sangre"},
			{L"Boil Over", L"Boil Over", L"Arrebato"},
			{L"Bond", L"Bond", L"Vínculo"},
			{L"Boon Circle Of Healing", L"Boon: Circle of Healing", L"Bendición: Círculo de curación"},
			{L"Boon Destroyer", L"Shattered Hope", L"Esperanza destruida"},
			{L"Boon Dark Theory", L"Boon: Dark Theory", L"Bendición: Teoría oscura"},
			{L"Dark Theory", L"Boon: Dark Theory", L"Bendición: Teoría oscura"},
			{L"Boon Exponential", L"Boon: Exponential", L"Bendición: Exponencial"},
			{L"Boon Shadow Step", L"Boon: Shadow Step", L"Bendición: Paso sombrío"},
			{L"Borrowed Time", L"Borrowed Time", L"Tiempo prestado"},
			{L"Botany Knowledge", L"Botany Knowledge", L"Conocimientos de botánica"},
			{L"Breakdown", L"Breakdown", L"Ruptura"},
			{L"Breakout", L"Breakout", L"Fuga"},
			{L"Brutal Strength", L"Brutal Strength", L"Fuerza brutal"},
			{L"Buckle Up", L"Buckle Up", L"Sujétate"},
			{L"Built To Last", L"Built to Last", L"Construcción duradera"},
			{L"Call Of Brine", L"Call of Brine", L"Salmuera"},
			{L"Calm Spirit", L"Calm Spirit", L"Espíritu calmado"},
			{L"Camaraderie", L"Camaraderie", L"Camaradería"},
			{L"Champion Of Light", L"Champion of Light", L"Campeón de la luz"},
			{L"Change Of Plan", L"Change of Plan", L"Cambio de plan"},
			{L"Chemical Trap", L"Chemical Trap", L"Trampa química"},
			{L"Clairvoyance", L"Clairvoyance", L"Clarividencia"},
			{L"Clean Break", L"Clean Break", L"Ruptura limpia"},
			{L"Come And Get Me", L"Come and Get Me!", L"Ven por mí"},
			{L"Conviction", L"Conviction", L"Convicción"},
			{L"Corrective Action", L"Corrective Action", L"Medida correctiva"},
			{L"Corrupt Intervention", L"Corrupt Intervention", L"Intervención corrupta"},
			{L"Coulrophobia", L"Coulrophobia", L"Coulrofobia"},
			{L"Counterforce", L"Counterforce", L"Contrafuerza"},
			{L"Coup De Grace", L"Coup de Grâce", L"Golpe de gracia"},
			{L"Cruel Confinement", L"Cruel Confinement", L"Restricción cruel"},
			{L"Cruel Limits", L"Cruel Limits", L"Restricción cruel"},
			{L"Cut Loose", L"Cut Loose", L"Suéltate"},
			{L"Dance With Me", L"Dance With Me", L"Baila conmigo"},
			{L"Dark Arrogance", L"Dark Arrogance", L"Arrogancia oscura"},
			{L"Dark Devotion", L"Dark Devotion", L"Devoción oscura"},
			{L"Dark Sense", L"Dark Sense", L"Percepción oscura"},
			{L"Darkness Revealed", L"Darkness Revealed", L"Oscuridad expuesta"},
			{L"Darkness Revelated", L"Darkness Revealed", L"Oscuridad expuesta"},
			{L"Dead Hard", L"Dead Hard", L"Fajador"},
			{L"Dead Man Switch", L"Dead Man's Switch", L"Interruptor del hombre muerto"},
			{L"Dead Man's Switch", L"Dead Man's Switch", L"Interruptor del hombre muerto"},
			{L"Deadline", L"Deadline", L"Fecha límite"},
			{L"Deadlock", L"Deadlock", L"Candado"},
			{L"Deathbound", L"Deathbound", L"Vínculo mortal"},
			{L"Deception", L"Deception", L"Engaño"},
			{L"Decisive Strike", L"Decisive Strike", L"Golpe decisivo"},
			{L"Deerstalker", L"Deerstalker", L"Acechador de ciervos"},
			{L"Deja Vu", L"Déjà Vu", L"Déjà Vu"},
			{L"Deliverance", L"Deliverance", L"Liberación"},
			{L"Desperate Focus", L"Desperate Focus", L"Concentración desesperada"},
			{L"Desperate Measures", L"Desperate Measures", L"Medidas desesperadas"},
			{L"Detectives Hunch", L"Detective's Hunch", L"Corazonada"},
			{L"Detective's Hunch", L"Detective's Hunch", L"Corazonada"},
			{L"Devour Hope", L"Hex: Devour Hope", L"Maleficio: Devoradora de esperanza"},
			{L"Discordance", L"Discordance", L"Discordancia"},
			{L"Dissolution", L"Dissolution", L"Disolución"},
			{L"Distortion", L"Distortion", L"Distorsión"},
			{L"Distressing", L"Distressing", L"Desasosiego"},
			{L"Diversion", L"Diversion", L"Distracción"},
			{L"Do No Harm", L"Do No Harm", L"No hagas daño"},
			{L"Dominance", L"Dominance", L"Dominancia"},
			{L"Dragons Grip", L"Dragon's Grip", L"Agarre del dragón"},
			{L"Dragon's Grip", L"Dragon's Grip", L"Agarre del dragón"},
			{L"Dramaturgy", L"Dramaturgy", L"Dramaturgia"},
			{L"Duty Of Care", L"Duty of Care", L"Deber de cuidado"},
			{L"Dying Light", L"Dying Light", L"Luz que agoniza"},
			{L"Empathic Connection", L"Empathic Connection", L"Conexión empática"},
			{L"Empathy", L"Empathy", L"Empatía"},
			{L"empty", L"Empty", L"Vacío"},
			{L"Enduring", L"Enduring", L"Resistente"},
			{L"Eruption", L"Eruption", L"Erupción"},
			{L"Exultation", L"Exultation", L"Exultación"},
			{L"Extrasensory Perception", L"Extrasensory Perception", L"Percepción extrasensorial"},
			{L"Eyes Of Belmont", L"Eyes of Belmont", L"Ojos de Belmont"},
			{L"Fast Track", L"Fast Track", L"Primera línea"},
			{L"Finesse", L"Finesse", L"Finura"},
			{L"Fire Up", L"Fire Up", L"Enfurecimiento"},
			{L"Fixated", L"Fixated", L"Fijación"},
			{L"Flashbang", L"Flashbang", L"Granada aturdidora"},
			{L"Flip Flop", L"Flip-Flop", L"Hasta otra"},
			{L"Flood Of Rage", L"Scourge Hook: Floods of Rage", L"Gancho flagelante: Oleada de ira"},
			{L"Fogwise", L"Fogwise", L"Sabiduría de la niebla"},
			{L"For The People", L"For the People", L"Por los demás"},
			{L"Forced Hesitation", L"Forced Hesitation", L"Vacilación forzada"},
			{L"Forced Penance", L"Forced Penance", L"Imposición de penitencia"},
			{L"Forever Entwined", L"Forever Entwined", L"Entrelazados para siempre"},
			{L"Franklins Loss", L"Franklin's Demise", L"Muerte de Franklin"},
			{L"Franklin's Demise", L"Franklin's Demise", L"Muerte de Franklin"},
			{L"Friendly Competition", L"Friendly Competition", L"Competencia amistosa"},
			{L"friends Till The End", L"Friends 'Til the End", L"Amigos hasta el final"},
			{L"Friends Till The End", L"Friends 'Til the End", L"Amigos hasta el final"},
			{L"Furtive Chase", L"Furtive Chase", L"Persecución furtiva"},
			{L"Game Afoot", L"Game Afoot", L"Juego en marcha"},
			{L"Gear Head", L"Gearhead", L"Oído para la maquinaria"},
			{L"Gearhead", L"Gearhead", L"Oído para la maquinaria"},
			{L"Generator Overcharge", L"Overcharge", L"Sobrecarga"},
			{L"Genetic Limits", L"Genetic Limits", L"Límites genéticos"},
			{L"Ghost Notes", L"Ghost Notes", L"Notas fantasma"},
			{L"Grim Embrace", L"Grim Embrace", L"Acogida nefasta"},
			{L"guardian", L"Guardian", L"Guardián"},
			{L"Guardian", L"Guardian", L"Guardián"},
			{L"Hangmans Trick", L"Hangman's Trick", L"Truco del verdugo"},
			{L"Hangman's Trick", L"Hangman's Trick", L"Truco del verdugo"},
			{L"Hardened", L"Hardened", L"Endurecida"},
			{L"Hatred", L"Rancor", L"Rencor"},
			{L"Haunted Ground", L"Hex: Haunted Ground", L"Maleficio: Tierra embrujada"},
			{L"Haywire", L"Haywire", L"Fuera de control"},
			{L"Head On", L"Head On", L"De frente"},
			{L"Help Wanted", L"Help Wanted", L"Se busca ayuda"},
			{L"Hex Blood Favor", L"Hex: Blood Favour", L"Maleficio: Favor de sangre"},
			{L"Hex Blood Favour", L"Hex: Blood Favour", L"Maleficio: Favor de sangre"},
			{L"Hex Crowd Control", L"Hex: Crowd Control", L"Maleficio: Control de masas"},
			{L"Hex Face The Darkness", L"Hex: Face the Darkness", L"Maleficio: Enfrenta la oscuridad"},
			{L"Hex Hive Mind", L"Hex: Hive Mind", L"Maleficio: Mente colmena"},
			{L"Hex Overture Of Doom", L"Hex: Overture of Doom", L"Maleficio: Obertura fatal"},
			{L"Hex Pentimento", L"Hex: Pentimento", L"Maleficio: Pentimento"},
			{L"Hex Plaything", L"Hex: Plaything", L"Maleficio: Juguete"},
			{L"Hex Retribution", L"Hex: Retribution", L"Maleficio: Represalias"},
			{L"Hex Ruin", L"Hex: Ruin", L"Maleficio: Ruina"},
			{L"Hex Undying", L"Hex: Undying", L"Maleficio: Inmortal"},
			{L"Hex Wretched Fate", L"Hex: Wretched Fate", L"Maleficio: Destino miserable"},
			{L"Hoarder", L"Hoarder", L"Acaparadora"},
			{L"Hope", L"Hope", L"Esperanza"},
			{L"Hubris", L"Hubris", L"Hibris"},
			{L"Human Greed", L"Human Greed", L"Codicia humana"},
			{L"Huntress Lullaby", L"Hex: Huntress Lullaby", L"Maleficio: Nana de cazadora"},
			{L"Hyperfocus", L"Hyperfocus", L"Hiperconcentración"},
			{L"Hysteria", L"Hysteria", L"Histeria"},
			{L"Illumination", L"Boon: Illumination", L"Bendición: Iluminación"},
			{L"Im All Ears", L"I'm All Ears", L"Soy todo oídos"},
			{L"I'm All Ears", L"I'm All Ears", L"Soy todo oídos"},
			{L"Infectious Fright", L"Infectious Fright", L"Terror contagioso"},
			{L"Inner Focus", L"Inner Focus", L"Concentración interna"},
			{L"inner Strength", L"Inner Strength", L"Fuerza interior"},
			{L"Inner Strength", L"Inner Strength", L"Fuerza interior"},
			{L"Insidious", L"Insidious", L"Insidia"},
			{L"Invocation Treacherous Crows", L"Invocation: Treacherous Crows", L"Invocación: Cuervos traicioneros"},
			{L"Invocation Weaving Spiders", L"Invocation: Weaving Spiders", L"Invocación: Arañas tejedoras"},
			{L"Iron Grasp", L"Iron Grasp", L"Apretón de hierro"},
			{L"Iron Maiden", L"Iron Maiden", L"Doncella de hierro"},
			{L"Iron Will", L"Iron Will", L"Voluntad de hierro"},
			{L"Kindred", L"Kindred", L"Familia"},
			{L"Knock Out", L"Knock Out", L"Noqueo"},
			{L"Languid Touch", L"Languid Touch", L"Toque lánguido"},
			{L"Last Stand", L"Last Stand", L"Última resistencia"},
			{L"Leader", L"Leader", L"Líder"},
			{L"Left Behind", L"Left Behind", L"Abandonado a tu suerte"},
			{L"Lethal Pursuer", L"Lethal Pursuer", L"Acecho letal"},
			{L"Leverage", L"Leverage", L"Ventaja"},
			{L"Light Footed", L"Light-Footed", L"De pies ligeros"},
			{L"Lightborn", L"Lightborn", L"Hijo de la luz"},
			{L"Lightweight", L"Lightweight", L"De pies ligeros"},
			{L"Lithe", L"Lithe", L"Agilidad"},
			{L"Low Profile", L"Low Profile", L"Perfil bajo"},
			{L"Lucky Break", L"Lucky Break", L"Golpe de suerte"},
			{L"Lucky Star", L"Lucky Star", L"Estrella de la suerte"},
			{L"Mad Grit", L"Mad Grit", L"Furia ciega"},
			{L"Make Your Choice", L"Make Your Choice", L"Toma una decisión"},
			{L"made For This", L"Made for This", L"Hecho para esto"},
			{L"Made For This", L"Made for This", L"Hecho para esto"},
			{L"Merciless Storm", L"Merciless Storm", L"Tormenta vil"},
			{L"Mettle Of Man", L"Mettle of Man", L"El temple del hombre"},
			{L"Mind Breaker", L"Mindbreaker", L"Quebrantamentes"},
			{L"Mindbreaker", L"Mindbreaker", L"Quebrantamentes"},
			{L"Mirrored Illusion", L"Mirrored Illusion", L"Ilusión reflejada"},
			{L"Missing", L"Missing", L"Faltante"},
			{L"Moment Of Glory", L"Moment of Glory", L"Momento de gloria"},
			{L"Monitor And Abuse", L"Monitor & Abuse", L"Monitorización y abuso"},
			{L"Monstrous Shrine", L"Monstrous Shrine", L"Santuario monstruoso"},
			{L"Nemesis", L"Nemesis", L"Némesis"},
			{L"No Holds Barred", L"No Holds Barred", L"Sin restricciones"},
			{L"No Mither", L"No Mither", L"Me la pela"},
			{L"No One Escapes Death", L"Hex: No One Escapes Death", L"Maleficio: Nadie escapa de la muerte"},
			{L"No One Left Behind", L"No One Left Behind", L"Nadie se queda atrás"},
			{L"No Quarter", L"No Quarter", L"Sin piedad"},
			{L"No Way Out", L"No Way Out", L"Sin escapatoria"},
			{L"None Are Free", L"None Are Free", L"Nadie es libre"},
			{L"Nothing But Misery", L"Nothing but Misery", L"Solo miseria"},
			{L"Nowhere To Hide", L"Nowhere to Hide", L"No hay dónde esconderse"},
			{L"Object Of Obsession", L"Object of Obsession", L"Objeto de obsesión"},
			{L"Off The Record", L"Off the Record", L"Extraoficial"},
			{L"One Two Three Four", L"One-Two-Three-Four!", L"¡Uno-Dos-Tres-Cuatro!"},
			{L"Open Handed", L"Open-Handed", L"A mano descubierta"},
			{L"Oppression", L"Oppression", L"Opresión"},
			{L"Overcome", L"Overcome", L"Sobreponerse"},
			{L"Overwhelming Presence", L"Overwhelming Presence", L"Presencia abrumadora"},
			{L"Overzealous", L"Overzealous", L"Pasión"},
			{L"Pain Resonance", L"Scourge Hook: Pain Resonance", L"Gancho flagelante: Dolor retumbante"},
			{L"Parental Guidance", L"Parental Guidance", L"Consejo paterno"},
			{L"Phantom Fear", L"Phantom Fear", L"Miedo fantasmal"},
			{L"Pharmacy", L"Pharmacy", L"Farmacia"},
			{L"Play With Your Food", L"Play with Your Food", L"Jugar con la comida"},
			{L"Plot Twist", L"Plot Twist", L"Giro argumental"},
			{L"Plunderers Instinct", L"Plunderer's Instinct", L"Instinto de saqueador"},
			{L"Poised", L"Poised", L"Serenidad"},
			{L"Pop Goes The Weasel", L"Pop Goes the Weasel", L"Pim, pam, pum"},
			{L"Potential Energy", L"Potential Energy", L"Energía potencial"},
			{L"Power Struggle", L"Power Struggle", L"Lucha intensa"},
			{L"Predator", L"Predator", L"Depredación"},
			{L"Premonition", L"Premonition", L"Premonición"},
			{L"Prove Thyself", L"Prove Thyself", L"Demuestra lo que vales"},
			{L"push Through It", L"Push Through It", L"Sigue adelante"},
			{L"Push Through It", L"Push Through It", L"Sigue adelante"},
			{L"Quick And Quiet", L"Quick & Quiet", L"Velocidad silenciosa"},
			{L"Rapid Brutality", L"Rapid Brutality", L"Brutalidad rápida"},
			{L"Rapid Response", L"Rapid Response", L"Respuesta rápida"},
			{L"Ravenous", L"Ravenous", L"Voraz"},
			{L"Reactive Healing", L"Reactive Healing", L"Curación reactiva"},
			{L"Reassurance", L"Reassurance", L"Reafirmación"},
			{L"Red Herring", L"Red Herring", L"Bulo"},
			{L"Remember Me", L"Remember Me", L"Recuérdame"},
			{L"Repressed Alliance", L"Repressed Alliance", L"Supresión de alianza"},
			{L"Residual Manifest", L"Residual Manifest", L"Manifestación residual"},
			{L"Resilience", L"Resilience", L"Resiliencia"},
			{L"Resurgence", L"Resurgence", L"Resurgimiento"},
			{L"Road Life", L"Road Life", L"Vida en la carretera"},
			{L"Rookie Spirit", L"Rookie Spirit", L"Espíritu de novato"},
			{L"Ruin", L"Hex: Ruin", L"Maleficio: Ruina"},
			{L"Saboteur", L"Saboteur", L"Sabotear"},
			{L"Save The Best For Last", L"Save the Best for Last", L"Lo mejor para el final"},
			{L"Scavenger", L"Scavenger", L"Carroñero"},
			{L"scavenger", L"Scavenger", L"Carroñero"},
			{L"Scene Partner", L"Scene Partner", L"Compañero de escena"},
			{L"Scourge Hook Gift Of Pain", L"Scourge Hook: Gift of Pain", L"Gancho flagelante: Obsequio doloroso"},
			{L"Scourge Hook Jagged Compass", L"Scourge Hook: Jagged Compass", L"Gancho flagelante: Brújula dentada"},
			{L"second Wind", L"Second Wind", L"Segundo aliento"},
			{L"Second Wind", L"Second Wind", L"Segundo aliento"},
			{L"Secret Project", L"Secret Project", L"Proyecto secreto"},
			{L"Self Aware", L"Self-Aware", L"Consciencia"},
			{L"Self Care", L"Self-Care", L"Autocuración"},
			{L"Self-Preservation", L"Self-Preservation", L"Autoconservación"},
			{L"Septic Touch", L"Septic Touch", L"Toque séptico"},
			{L"Shadowborn", L"Shadowborn", L"Hijo de las sombras"},
			{L"Shoulder The Burden", L"Shoulder the Burden", L"Cargar con el peso"},
			{L"situational Awareness", L"Situational Awareness", L"Percepción situacional"},
			{L"Situational Awareness", L"Situational Awareness", L"Percepción situacional"},
			{L"Slippery Meat", L"Slippery Meat", L"Carne resbaladiza"},
			{L"Sloppy Butcher", L"Sloppy Butcher", L"Carnicero chapucero"},
			{L"Small Game", L"Small Game", L"Caza menor"},
			{L"Smash Hit", L"Smash Hit", L"Éxito aplastante"},
			{L"Sole SideSurvivor", L"Sole SideSurvivor", L"Solo quedo yo"},
			{L"Solidarity", L"Solidarity", L"Solidaridad"},
			{L"Soul Guard", L"Soul Guard", L"Salvaguarda de alma"},
			{L"Specialist", L"Specialist", L"Especialista"},
			{L"Spies From The Shadows", L"Spies from the Shadows", L"Espías de las sombras"},
			{L"Spine Chill", L"Spine Chill", L"Escalofríos"},
			{L"Spirit Fury", L"Spirit Fury", L"Furia espiritual"},
			{L"Sprint Burst", L"Sprint Burst", L"Esprint"},
			{L"Stake Out", L"Stake Out", L"Bajo vigilancia"},
			{L"Starstruck", L"Starstruck", L"Deslumbrado"},
			{L"Still Sight", L"Still Sight", L"Vista fija"},
			{L"Streetwise", L"Streetwise", L"Con calle"},
			{L"Strength In Shadows", L"Strength in Shadows", L"Fortaleza en las sombras"},
			{L"Stridor", L"Stridor", L"Aliento"},
			{L"Superior Anatomy", L"Superior Anatomy", L"Anatomía superior"},
			{L"Surge", L"Surge", L"Sobretensión"},
			{L"Surveillance", L"Surveillance", L"Supervisión"},
			{L"survival Instincts", L"Survival Instincts", L"Instintos de supervivencia"},
			{L"Survival Instincts", L"Survival Instincts", L"Instintos de supervivencia"},
			{L"Teamwork Collective Stealth", L"Teamwork: Collective Stealth", L"Trabajo en equipo: Sigilo colectivo"},
			{L"Teamwork Full Circuit", L"Teamwork: Full Circuit", L"Trabajo en equipo: Circuito completo"},
			{L"Teamwork Power Of Two", L"Teamwork: Power of Two", L"Trabajo en equipo: Poder de dos"},
			{L"Teamwork Soft Spoken", L"Teamwork: Soft-Spoken", L"Trabajo en equipo: Voz baja"},
			{L"Teamwork Throw Down", L"Teamwork: Throw Down", L"Trabajo en equipo: Lanzamiento"},
			{L"Teamwork Toughen Up", L"Teamwork: Toughen Up", L"Trabajo en equipo: Endurecerse"},
			{L"Technician", L"Technician", L"Pericia técnica"},
			{L"Tenacity", L"Tenacity", L"Tenacidad"},
			{L"Terminus", L"Terminus", L"Terminal"},
			{L"Territorial Imperative", L"Territorial Imperative", L"Instinto territorial"},
			{L"Thatanophobia", L"Thanatophobia", L"Tanatofobia"},
			{L"Thanatophobia", L"Thanatophobia", L"Tanatofobia"},
			{L"The Third Seal", L"Hex: The Third Seal", L"Maleficio: El tercer sello"},
			{L"This Is Not Happening", L"This Is Not Happening", L"Esto no puede estar pasando"},
			{L"Thrill Of The Hunt", L"Hex: Thrill of the Hunt", L"Maleficio: La emoción de la caza"},
			{L"Thrilling Tremors", L"Thrilling Tremors", L"Temblores trepidantes"},
			{L"Thwack", L"THWACK!", L"¡Zas!"},
			{L"Tinkerer", L"Tinkerer", L"Manitas"},
			{L"Toy With Them", L"Toy with Them", L"Juega con ellos"},
			{L"Trail Of Torment", L"Trail of Torment", L"Rastro de tormento"},
			{L"troubleshooter", L"Troubleshooter", L"Solucionador de problemas"},
			{L"Troubleshooter", L"Troubleshooter", L"Solucionador de problemas"},
			{L"Turn Back The Clock", L"Turn Back the Clock", L"Retroceder el reloj"},
			{L"two Can Play", L"Two Can Play", L"Dos pueden jugar"},
			{L"Two Can Play", L"Two Can Play", L"Dos pueden jugar"},
			{L"Ultimate Weapon", L"Ultimate Weapon", L"Arma definitiva"},
			{L"Unbound", L"Unbound", L"Sin ataduras"},
			{L"Unbreakable", L"Unbreakable", L"Inquebrantable"},
			{L"Undone", L"Undone", L"Deshecho"},
			{L"Unforeseen", L"Unforeseen", L"Imprevisto"},
			{L"Unnerving Presence", L"Unnerving Presence", L"Presencia perturbadora"},
			{L"Unrelenting", L"Unrelenting", L"Implacable"},
			{L"Up The Ante", L"Up the Ante", L"Subir las apuestas"},
			{L"Urban Evasion", L"Urban Evasion", L"Evasión urbana"},
			{L"Vigil", L"Vigil", L"Vigilia"},
			{L"Visionary", L"Visionary", L"Visión de futuro"},
			{L"Vittorios Gambit", L"Potential Energy", L"Energía potencial"},
			{L"Wake Up", L"Wake Up!", L"¡Despierta!"},
			{L"Wandering Eye", L"Wandering Eye", L"Mirada errante"},
			{L"We See You", L"We See You", L"Te vemos"},
			{L"Weave Attunement", L"Weave Attunement", L"Armonización del tejido"},
			{L"Weeping Wounds", L"Weeping Wounds", L"Heridas llorosas"},
			{L"Well Make It", L"We'll Make It", L"Lo conseguiremos"},
			{L"We'll Make It", L"We'll Make It", L"Lo conseguiremos"},
			{L"Were Gonna Live Forever", L"We're Gonna Live Forever", L"Vamos a vivir para siempre"},
			{L"We're Gonna Live Forever", L"We're Gonna Live Forever", L"Vamos a vivir para siempre"},
			{L"Whispers", L"Whispers", L"Murmullos"},
			{L"Wicked", L"Wicked", L"Malicia"},
			{L"Windows Of Opportunity", L"Windows of Opportunity", L"Oportunidades"},
			{L"Wiretap", L"Wiretap", L"Interceptor"},
			{L"Zanshin Tactics", L"Zanshin Tactics", L"Tácticas de Zanshin"},
			{L"Rampage", L"Rampage", L"Furia"},
			{L"Scared To Death", L"Scared to Death", L"Aterrorizado"},
			{L"Silent Shadow", L"Silent Shadow", L"Sombra Silenciosa"}
		};

		const std::wstring key = normalizePerkNameKey(name);
		for (const PerkTranslation& translation : translations)
		{
			if (normalizePerkNameKey(translation.lookupName) == key) {
				return &translation;
			}
		}

		return nullptr;
	}

	std::wstring getCanonicalPerkName(const std::wstring& rawName)
	{
		if (const PerkTranslation* translation = findPerkTranslation(rawName)) {
			return translation->englishName;
		}

		return rawName;
	}

	std::wstring getSpanishPerkName(const std::wstring& rawName)
	{
		if (const PerkTranslation* translation = findPerkTranslation(rawName)) {
			return translation->spanishName;
		}

		return rawName;
	}

	bool startsWithInsensitive(const std::wstring& text, const std::wstring& prefix)
	{
		if (text.size() < prefix.size()) {
			return false;
		}

		return lowerText(text.substr(0, prefix.size())) == lowerText(prefix);
	}

	struct PerkMetadata
	{
		std::wstring spanishName;
		std::wstring characterName;
		std::wstring description;
	};

	std::wstring getModuleDirectory()
	{
		wchar_t path[MAX_PATH] = {};
		const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
		if (length == 0) {
			return L"";
		}

		std::wstring modulePath(path, length);
		const size_t slashPos = modulePath.find_last_of(L"\\/");
		return slashPos == std::wstring::npos ? L"" : modulePath.substr(0, slashPos);
	}

	std::wstring combinePath(const std::wstring& directory, const std::wstring& relativePath)
	{
		if (directory.empty()) {
			return relativePath;
		}

		const wchar_t lastChar = directory[directory.size() - 1];
		return lastChar == L'\\' || lastChar == L'/'
			? directory + relativePath
			: directory + L"\\" + relativePath;
	}

	std::wstring resolveAudioPath(const std::wstring& relativePath)
	{
		const std::wstring modulePath = combinePath(getModuleDirectory(), relativePath);
		if (GetFileAttributesW(modulePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
			return modulePath;
		}

		wchar_t currentDirectory[MAX_PATH] = {};
		if (GetCurrentDirectoryW(MAX_PATH, currentDirectory) > 0)
		{
			const std::wstring currentPath = combinePath(currentDirectory, relativePath);
			if (GetFileAttributesW(currentPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
				return currentPath;
			}
		}

		return modulePath;
	}

	bool directoryExists(const std::wstring& path)
	{
		const DWORD attributes = GetFileAttributesW(path.c_str());
		return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	std::wstring resolvePerksFolder()
	{
		const std::wstring moduleDirectory = getModuleDirectory();
		const std::wstring moduleIncluded = combinePath(moduleDirectory, kIncludedPerksFolder);
		const std::wstring moduleProjectIncluded = combinePath(moduleDirectory, L"..\\..\\Included Assets\\Builds Survi & Killer");

		wchar_t currentDirectory[MAX_PATH] = {};
		const DWORD currentLength = GetCurrentDirectoryW(MAX_PATH, currentDirectory);
		const std::wstring currentIncluded = currentLength > 0
			? combinePath(currentDirectory, kIncludedPerksFolder)
			: std::wstring();

		const std::wstring candidates[] = {
			moduleIncluded,
			moduleProjectIncluded,
			currentIncluded,
			kIncludedPerksFolder
		};

		for (const std::wstring& candidate : candidates)
		{
			if (directoryExists(candidate)) {
				return candidate;
			}
		}

		return moduleIncluded;
	}

	void stopBuildAudio(const wchar_t* alias)
	{
		UNREFERENCED_PARAMETER(alias);
		PlaySoundW(nullptr, nullptr, 0);
	}

	void playBuildAudio(const wchar_t* relativePath, const wchar_t* alias)
	{
		if (relativePath == nullptr || alias == nullptr) {
			return;
		}

		UNREFERENCED_PARAMETER(alias);
		stopBuildAudio(nullptr);

		const std::wstring audioPath = resolveAudioPath(relativePath);
		if (GetFileAttributesW(audioPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
			return;
		}

		PlaySoundW(audioPath.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
	}

	bool loadJsonFile(const std::wstring& path, Json::Value& root)
	{
		std::ifstream file(utf8FromWide(path), std::ios::binary);
		if (!file.is_open()) {
			return false;
		}

		Json::Reader reader;
		return reader.parse(file, root);
	}

	void insertPerkMetadataAlias(std::map<std::wstring, PerkMetadata>& metadata, const std::wstring& alias, const std::wstring& target)
	{
		const auto targetIt = metadata.find(normalizePerkNameKey(target));
		if (targetIt != metadata.end()) {
			metadata[normalizePerkNameKey(alias)] = targetIt->second;
		}
	}

	const std::map<std::wstring, PerkMetadata>& getPerkMetadataMap()
	{
		static std::map<std::wstring, PerkMetadata> metadata;
		static bool loaded = false;
		if (loaded) {
			return metadata;
		}

		loaded = true;
		Json::Value root;
		const std::wstring moduleDirectory = getModuleDirectory();
		const std::wstring candidates[] = {
			moduleDirectory.empty() ? L"perk_metadata.json" : moduleDirectory + L"\\perk_metadata.json",
			L"perk_metadata.json",
			L"..\\..\\perk_metadata.json"
		};

		for (const std::wstring& candidate : candidates)
		{
			if (loadJsonFile(candidate, root)) {
				break;
			}
		}

		const Json::Value perks = root["perks"];
		for (Json::ArrayIndex index = 0; index < perks.size(); ++index)
		{
			const Json::Value item = perks[index];
			PerkMetadata perkMetadata;
			perkMetadata.spanishName = wideFromUtf8(item.get("spanishName", "").asString());
			perkMetadata.characterName = wideFromUtf8(item.get("character", "").asString());
			perkMetadata.description = wideFromUtf8(item.get("description", "").asString());

			const std::wstring key = wideFromUtf8(item.get("key", "").asString());
			if (!key.empty()) {
				metadata[normalizePerkNameKey(key)] = perkMetadata;
			}
			if (!perkMetadata.spanishName.empty()) {
				metadata[normalizePerkNameKey(perkMetadata.spanishName)] = perkMetadata;
			}
		}

		insertPerkMetadataAlias(metadata, L"BBQ And Chili", L"barbecueandchilli");
		insertPerkMetadataAlias(metadata, L"Barbecue And Chilli", L"barbecueandchilli");
		insertPerkMetadataAlias(metadata, L"Cruel Confinement", L"cruellimits");
		insertPerkMetadataAlias(metadata, L"Hangmans Trick", L"scourgehookhangmanstrick");
		insertPerkMetadataAlias(metadata, L"Hangman's Trick", L"scourgehookhangmanstrick");
		insertPerkMetadataAlias(metadata, L"Guardian", L"babysitter");
		insertPerkMetadataAlias(metadata, L"Monstrous Shrine", L"scourgehookmonstrousshrine");
		insertPerkMetadataAlias(metadata, L"Self Aware", L"fixated");
		insertPerkMetadataAlias(metadata, L"Self-Aware", L"fixated");
		insertPerkMetadataAlias(metadata, L"Situational Awareness", L"bettertogether");
		insertPerkMetadataAlias(metadata, L"Two Can Play", L"hextwocanplay");
		insertPerkMetadataAlias(metadata, L"two Can Play", L"hextwocanplay");
		insertPerkMetadataAlias(metadata, L"Weeping Wounds", L"scourgehookweepingwounds");

		return metadata;
	}

	const PerkMetadata* findPerkMetadata(const std::wstring& rawName, const std::wstring& englishName, const std::wstring& spanishName)
	{
		const std::map<std::wstring, PerkMetadata>& metadata = getPerkMetadataMap();
		const std::wstring keys[] = {
			normalizePerkNameKey(rawName),
			normalizePerkNameKey(englishName),
			normalizePerkNameKey(spanishName)
		};

		for (const std::wstring& key : keys)
		{
			const auto it = metadata.find(key);
			if (it != metadata.end()) {
				return &it->second;
			}
		}

		return nullptr;
	}

	std::wstring cleanPerkFileName(const std::wstring& fileName)
	{
		std::wstring name = fileName;
		const size_t dotPos = name.find_last_of(L'.');
		if (dotPos != std::wstring::npos) {
			name = name.substr(0, dotPos);
		}

		const std::wstring prefixes[] = {
			L"iconPerks_",
			L"iconsPerks_",
			L"T_iconPerks_",
			L"T_iconsPerks_",
			L"T_UI_iconPerks_",
			L"T_UI_iconsPerks_",
			L"T_UIiconPerks_",
			L"T_UIiconsPerks_"
		};

		for (const std::wstring& prefix : prefixes)
		{
			if (startsWithInsensitive(name, prefix)) {
				name = name.substr(prefix.size());
				break;
			}
		}

		std::wstring cleaned;
		for (size_t index = 0; index < name.size(); ++index)
		{
			const wchar_t ch = name[index];
			if (index > 0 && iswupper(ch) && iswlower(name[index - 1])) {
				cleaned += L' ';
			}
			cleaned += ch;
		}

		return cleaned.empty() ? L"Perk" : cleaned;
	}

	void discoverPerkImagesFromFolder(const std::wstring& folder, std::vector<PerkIconData>& icons, std::set<std::wstring>& seenNames)
	{
		std::wstring normalizedFolder = folder;
		if (!normalizedFolder.empty() && normalizedFolder.back() != L'\\' && normalizedFolder.back() != L'/') {
			normalizedFolder += L"\\";
		}

		const std::wstring searchPattern = normalizedFolder + L"*.*";
		WIN32_FIND_DATAW findData = {};
		HANDLE findHandle = FindFirstFileW(searchPattern.c_str(), &findData);
		if (findHandle == INVALID_HANDLE_VALUE) {
			return;
		}

		do
		{
			const std::wstring fileName = findData.cFileName;
			if (fileName == L"." || fileName == L"..") {
				continue;
			}

			const std::wstring path = normalizedFolder + fileName;
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				discoverPerkImagesFromFolder(path, icons, seenNames);
				continue;
			}

			if (!hasImageExtension(fileName)) {
				continue;
			}

			const std::wstring rawPerkName = cleanPerkFileName(fileName);
			const std::wstring englishPerkName = getCanonicalPerkName(rawPerkName);
			std::wstring spanishPerkName = getSpanishPerkName(rawPerkName);
			const PerkMetadata* metadata = findPerkMetadata(rawPerkName, englishPerkName, spanishPerkName);
			if (metadata != nullptr && !metadata->spanishName.empty()) {
				spanishPerkName = metadata->spanishName;
			}
			const std::wstring nameKey = normalizePerkNameKey(englishPerkName);
			if (seenNames.find(nameKey) != seenNames.end()) {
				continue;
			}

			PerkIconData icon;
			icon.filePath = path;
			icon.displayName = spanishPerkName;
			icon.internalName = englishPerkName;
			icon.characterLabel = metadata != nullptr ? metadata->characterName : L"";
			icon.tooltipText = metadata != nullptr ? metadata->description : L"";
			icon.filterToken = normalizeSearchText(
				spanishPerkName + L" " +
				englishPerkName + L" " +
				rawPerkName + L" " +
				icon.characterLabel + L" " +
				icon.tooltipText);
			icons.push_back(icon);
			seenNames.insert(nameKey);
		} while (FindNextFileW(findHandle, &findData));

		FindClose(findHandle);
	}
}

LoadoutEditor::~LoadoutEditor()
{
	releaseGdi();
}

void LoadoutEditor::showForSide(LoadoutSide side, HWND ownerWnd)
{
	UNREFERENCED_PARAMETER(ownerWnd);

	resetRandomShuffle(true);
	currentSide = side;
	selectedBuildIndex_ = -1;
	openPerkSlot_ = -1;
	editingBuild = PerkSlot();
	editingBuild.label = side == LoadoutSide::SideSurvivor ? L"New SideSurvivor Build" : L"New SideKiller Build";

	if (!getActiveRoleList().empty()) {
		selectedBuildIndex_ = 0;
		editingBuild = getActiveRoleList()[0];
	}

	if (id() == nullptr)
	{
		const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
		const int windowWidth = (std::max)(820, screenWidth / 2);
		const int windowHeight = (std::max)(560, screenHeight / 2);
		const int x = (screenWidth - windowWidth) / 2;
		const int y = (screenHeight - windowHeight) / 2;
		const std::wstring title = getSideLabel() + L" Builds";

		if (spawn(
			x, y,
			windowWidth, windowHeight,
			WS_EX_APPWINDOW,
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX | WS_CLIPCHILDREN,
			nullptr,
			title.c_str()))
		{
			ShowWindow(id(), SW_SHOW);
			finishWindowSetup();
		}
		return;
	}

	SetWindowTextW(id(), (getSideLabel() + L" Builds").c_str());
	if (hNameField_ != nullptr) {
		SetWindowTextW(hNameField_, editingBuild.label.c_str());
	}
	closePerkSelector();
	computeWidgetLayout();
	InvalidateRect(nativeWindow_, nullptr, FALSE);
	ShowWindow(id(), SW_SHOW);
	SetForegroundWindow(id());
}

void LoadoutEditor::buildUI()
{
	allocateGdi();
	discoverPerkImages();

	hNameField_ = CreateWindowExW(
		0, WC_EDIT, editingBuild.label.c_str(),
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
		0, 0, 220, 26,
		nativeWindow_, nullptr, nullptr, nullptr);

	hNewBtn_ = CreateWindowExW(0, WC_BUTTON, L"New Build", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 100, 34, nativeWindow_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kNewBuildButtonId)), nullptr, nullptr);
	hSaveBtn_ = CreateWindowExW(0, WC_BUTTON, L"Save", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 86, 34, nativeWindow_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSaveBuildButtonId)), nullptr, nullptr);
	hDeleteBtn_ = CreateWindowExW(0, WC_BUTTON, L"Delete", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 86, 34, nativeWindow_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kDeleteBuildButtonId)), nullptr, nullptr);
	hRandomBtn_ = CreateWindowExW(0, WC_BUTTON, L"Aleatorio", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 132, 34, nativeWindow_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRandomBuildButtonId)), nullptr, nullptr);

	hSearchField_ = CreateWindowExW(
		0,
		WC_EDIT,
		L"",
		WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
		0, 0, 220, 28,
		nativeWindow_,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPerkSearchEditId)),
		nullptr,
		nullptr);

	SendMessageW(hNameField_, WM_SETFONT, reinterpret_cast<WPARAM>(interfaceFont_), TRUE);
	SendMessageW(hSearchField_, WM_SETFONT, reinterpret_cast<WPARAM>(interfaceFont_), TRUE);
	ShowWindow(hSearchField_, SW_HIDE);
	applySearchFilter();
	computeWidgetLayout();
}

void LoadoutEditor::allocateGdi()
{
	if (backgroundBrush_ == nullptr) {
		backgroundBrush_ = CreateSolidBrush(kBg);
	}

	if (inputFieldBrush_ == nullptr) {
		inputFieldBrush_ = CreateSolidBrush(RGB(8, 13, 20));
	}

	if (interfaceFont_ == nullptr)
	{
		interfaceFont_ = CreateFont(
			16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	}

	if (gdiToken_ == 0)
	{
		Gdiplus::GdiplusStartupInput startupInput;
		if (Gdiplus::GdiplusStartup(&gdiToken_, &startupInput, nullptr) != Gdiplus::Ok) {
			gdiToken_ = 0;
		}
	}
}

void LoadoutEditor::releaseGdi()
{
	purgeAllPerkImages();

	if (renderBuffer_ != nullptr)
	{
		delete renderBuffer_;
		renderBuffer_ = nullptr;
	}

	if (backgroundBrush_ != nullptr) {
		DeleteObject(backgroundBrush_);
		backgroundBrush_ = nullptr;
	}

	if (inputFieldBrush_ != nullptr) {
		DeleteObject(inputFieldBrush_);
		inputFieldBrush_ = nullptr;
	}

	if (interfaceFont_ != nullptr) {
		DeleteObject(interfaceFont_);
		interfaceFont_ = nullptr;
	}

	if (gdiToken_ != 0)
	{
		Gdiplus::GdiplusShutdown(gdiToken_);
		gdiToken_ = 0;
	}
}

void LoadoutEditor::purgeAllPerkImages()
{
	for (PerkIconData& icon : perkDb_)
	{
		delete icon.loadedImage;
		icon.loadedImage = nullptr;
	}
	perkDb_.clear();
}

void LoadoutEditor::discoverPerkImages()
{
	purgeAllPerkImages();

	if (gdiToken_ == 0) {
		return;
	}

	std::set<std::wstring> seenNames;
	discoverPerkImagesFromFolder(resolvePerksFolder(), perkDb_, seenNames);

	std::sort(perkDb_.begin(), perkDb_.end(), [](const PerkIconData& a, const PerkIconData& b) {
		return a.displayName < b.displayName;
	});
}

void LoadoutEditor::allocateBuffer(const int width, const int height)
{
	const int clampedWidth = (std::max)(1, width);
	const int clampedHeight = (std::max)(1, height);
	if (renderBuffer_ != nullptr &&
		cachedBufW_ == clampedWidth &&
		cachedBufH_ == clampedHeight)
	{
		return;
	}

	delete renderBuffer_;
	renderBuffer_ = new Gdiplus::Bitmap(clampedWidth, clampedHeight, PixelFormat32bppPARGB);
	cachedBufW_ = clampedWidth;
	cachedBufH_ = clampedHeight;
}

void LoadoutEditor::computeWidgetLayout()
{
	if (nativeWindow_ == nullptr) {
		return;
	}

	RECT clientRect = {};
	GetClientRect(nativeWindow_, &clientRect);
	const int clientWidth = static_cast<int>(clientRect.right - clientRect.left);
	const int clientHeight = static_cast<int>(clientRect.bottom - clientRect.top);
	const int leftWidth = (std::max)(280, (std::min)(360, clientWidth / 3));
	const int controlLeft = 34;
	const int controlWidth = leftWidth - 56;
	const RECT boardRect = { leftWidth + 12, 16, clientWidth - 16, clientHeight - 16 };

	if (hNameField_ != nullptr) {
		SetWindowPos(hNameField_, nullptr, controlLeft, 124, controlWidth, 28, SWP_NOZORDER);
	}

	const int buttonY = clientHeight - 50;
	const int newWidth = 96;
	const int saveWidth = 74;
	const int deleteWidth = 74;
	const int buttonGap = 14;
	const int totalButtonWidth = newWidth + saveWidth + deleteWidth + (buttonGap * 2);
	const int buttonX = 16 + ((leftWidth - 24 - totalButtonWidth) / 2);
	if (hNewBtn_ != nullptr) {
		SetWindowPos(hNewBtn_, nullptr, buttonX, buttonY, newWidth, 34, SWP_NOZORDER);
	}
	if (hSaveBtn_ != nullptr) {
		SetWindowPos(hSaveBtn_, nullptr, buttonX + newWidth + buttonGap, buttonY, saveWidth, 34, SWP_NOZORDER);
	}
	if (hDeleteBtn_ != nullptr) {
		SetWindowPos(hDeleteBtn_, nullptr, buttonX + newWidth + buttonGap + saveWidth + buttonGap, buttonY, deleteWidth, 34, SWP_NOZORDER);
	}

	if (hRandomBtn_ != nullptr)
	{
		const int randomWidth = 132;
		const int randomHeight = 34;
		SetWindowPos(
			hRandomBtn_,
			nullptr,
			boardRect.right - randomWidth - 24,
			boardRect.top + 18,
			randomWidth,
			randomHeight,
			SWP_NOZORDER);
	}
}

void LoadoutEditor::onPaint()
{
	PAINTSTRUCT ps = {};
	const HDC hdc = BeginPaint(nativeWindow_, &ps);

	RECT clientRect = {};
	GetClientRect(nativeWindow_, &clientRect);
	const int clientWidth = static_cast<int>(clientRect.right - clientRect.left);
	const int clientHeight = static_cast<int>(clientRect.bottom - clientRect.top);

	allocateBuffer(clientWidth, clientHeight);
	if (renderBuffer_ == nullptr)
	{
		EndPaint(nativeWindow_, &ps);
		return;
	}

	Gdiplus::Graphics bufferGraphics(renderBuffer_);
	bufferGraphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
	bufferGraphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
	bufferGraphics.Clear(Gdiplus::Color(255, GetRValue(kBg), GetGValue(kBg), GetBValue(kBg)));

	drawBuildCard(bufferGraphics, clientRect);
	drawBuildSlots(bufferGraphics, clientRect);
	drawPerkGrid(bufferGraphics);

	Gdiplus::Graphics graphics(hdc);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
	graphics.DrawImage(renderBuffer_, 0, 0);

	EndPaint(nativeWindow_, &ps);
}

void LoadoutEditor::drawBuildCard(Gdiplus::Graphics& graphics, const RECT& clientRect)
{
	const int clientWidth = static_cast<int>(clientRect.right - clientRect.left);
	const int clientHeight = static_cast<int>(clientRect.bottom - clientRect.top);
	const int leftWidth = (std::max)(280, (std::min)(360, clientWidth / 3));
	const RECT panelRect = { 16, 16, leftWidth - 8, clientHeight - 16 };
	drawRoundedPanel(graphics, panelRect, kPanel, kBorder, 16.0f);

	const RECT titleRect = { panelRect.left + 18, panelRect.top + 18, panelRect.right - 18, panelRect.top + 52 };
	drawText(graphics, L"CURRENT " + getSideLabel() + L" BUILD", titleRect, 18.0f, Gdiplus::FontStyleBold, kText, Gdiplus::StringAlignmentCenter);

	const RECT nameLabelRect = { panelRect.left + 18, 92, panelRect.right - 18, 116 };
	drawText(graphics, L"Build Name", nameLabelRect, 14.0f, Gdiplus::FontStyleBold, kAccentSoft);

	const int centerX = (panelRect.left + panelRect.right) / 2;
	const int centerY = 276;
	const int slotSize = 78;
	const int offset = 70;

	slotBounds_[0] = { centerX - (slotSize / 2), centerY - offset - (slotSize / 2), centerX + (slotSize / 2), centerY - offset + (slotSize / 2) };
	slotBounds_[1] = { centerX - offset - (slotSize / 2), centerY - (slotSize / 2), centerX - offset + (slotSize / 2), centerY + (slotSize / 2) };
	slotBounds_[2] = { centerX + offset - (slotSize / 2), centerY - (slotSize / 2), centerX + offset + (slotSize / 2), centerY + (slotSize / 2) };
	slotBounds_[3] = { centerX - (slotSize / 2), centerY + offset - (slotSize / 2), centerX + (slotSize / 2), centerY + offset + (slotSize / 2) };

	Gdiplus::Pen connectorPen(Gdiplus::Color(160, GetRValue(kAccentSoft), GetGValue(kAccentSoft), GetBValue(kAccentSoft)), 3.0f);
	graphics.DrawLine(&connectorPen, centerX, centerY - offset, centerX - offset, centerY);
	graphics.DrawLine(&connectorPen, centerX, centerY - offset, centerX + offset, centerY);
	graphics.DrawLine(&connectorPen, centerX - offset, centerY, centerX, centerY + offset);
	graphics.DrawLine(&connectorPen, centerX + offset, centerY, centerX, centerY + offset);

	for (int index = 0; index < 4; ++index) {
		drawPerkGem(graphics, slotBounds_[index], editingBuild.perks[index], openPerkSlot_ == index, 3.0f);
	}

	const RECT hintRect = { panelRect.left + 26, centerY + 118, panelRect.right - 26, centerY + 164 };
	drawText(graphics, L"Click a diamond to choose a perk from the list.", hintRect, 14.0f, Gdiplus::FontStyleRegular, kMutedText, Gdiplus::StringAlignmentCenter);

	const RECT countRect = { panelRect.left + 18, centerY + 166, panelRect.right - 18, centerY + 194 };
	std::wstring loadedText = std::to_wstring(perkDb_.size()) + L" perks loaded";
	drawText(graphics, loadedText, countRect, 13.0f, Gdiplus::FontStyleBold, kAccentSoft, Gdiplus::StringAlignmentCenter);
}

void LoadoutEditor::drawBuildSlots(Gdiplus::Graphics& graphics, const RECT& clientRect)
{
	cardBounds_.clear();

	const int clientWidth = static_cast<int>(clientRect.right - clientRect.left);
	const int clientHeight = static_cast<int>(clientRect.bottom - clientRect.top);
	const int leftWidth = (std::max)(280, (std::min)(360, clientWidth / 3));
	const RECT boardRect = { leftWidth + 12, 16, clientWidth - 16, clientHeight - 16 };
	drawRoundedPanel(graphics, boardRect, kPanelAlt, kBorder, 16.0f);

	const RECT titleRect = { boardRect.left + 22, boardRect.top + 18, boardRect.right - 178, boardRect.top + 54 };
	drawText(graphics, getSideLabel() + L" BUILDS", titleRect, 18.0f, Gdiplus::FontStyleBold, kText);

	const std::vector<PerkSlot>& buildConfig = getActiveRoleList();
	if (buildConfig.empty())
	{
		const RECT emptyRect = { boardRect.left + 24, boardRect.top + 78, boardRect.right - 24, boardRect.bottom - 24 };
		drawText(graphics, L"No buildConfig saved yet. Pick four perks, name the build, then press Save.", emptyRect, 18.0f, Gdiplus::FontStyleBold, kMutedText, Gdiplus::StringAlignmentCenter);
		return;
	}

	const int cardWidth = 184;
	const int cardHeight = 206;
	const int gap = 18;
	const int startX = boardRect.left + 24;
	const int startY = boardRect.top + 72;
	const int boardWidth = (std::max)(1, static_cast<int>(boardRect.right - boardRect.left - 48));
	const int columns = (std::max)(1, boardWidth / (cardWidth + gap));

	for (int index = 0; index < static_cast<int>(buildConfig.size()); ++index)
	{
		const int column = index % columns;
		const int row = index / columns;
		const int x = startX + column * (cardWidth + gap);
		const int y = startY + row * (cardHeight + gap);
		if (y + cardHeight > boardRect.bottom - 18) {
			break;
		}

		const RECT cardRect = { x, y, x + cardWidth, y + cardHeight };
		cardBounds_.push_back(cardRect);
		const bool selected = index == selectedBuildIndex_;
		const bool randomScanning = shuffleAnimActive_ && index == shuffleHighlightIdx_;
		const bool randomWinner = !shuffleAnimActive_ && index == shuffleFinalIdx_;
		const COLORREF cardFill = randomWinner
			? RGB(255, 61, 61)
			: randomScanning
				? RGB(37, 150, 190)
				: selected
					? RGB(23, 61, 86)
					: RGB(20, 25, 35);
		const COLORREF cardBorder = randomWinner
			? RGB(255, 61, 61)
			: randomScanning
				? RGB(37, 150, 190)
				: selected
					? kAccentSoft
					: RGB(48, 62, 82);
		const float cardBorderWidth = randomWinner || randomScanning ? 3.0f : (selected ? 2.5f : 1.5f);
		drawRoundedPanel(graphics, cardRect, cardFill, cardBorder, 12.0f, cardBorderWidth);

		const int miniSize = 56;
		const int miniCenterX = x + (cardWidth / 2);
		const int miniCenterY = y + 76;
		const int miniOffset = 39;
		const RECT miniRects[4] = {
			{ miniCenterX - (miniSize / 2), miniCenterY - miniOffset - (miniSize / 2), miniCenterX + (miniSize / 2), miniCenterY - miniOffset + (miniSize / 2) },
			{ miniCenterX - miniOffset - (miniSize / 2), miniCenterY - (miniSize / 2), miniCenterX - miniOffset + (miniSize / 2), miniCenterY + (miniSize / 2) },
			{ miniCenterX + miniOffset - (miniSize / 2), miniCenterY - (miniSize / 2), miniCenterX + miniOffset + (miniSize / 2), miniCenterY + (miniSize / 2) },
			{ miniCenterX - (miniSize / 2), miniCenterY + miniOffset - (miniSize / 2), miniCenterX + (miniSize / 2), miniCenterY + miniOffset + (miniSize / 2) }
		};

		for (int perkIndex = 0; perkIndex < 4; ++perkIndex) {
			drawPerkGem(graphics, miniRects[perkIndex], buildConfig[index].perks[perkIndex], false, 1.8f);
		}

		const RECT buildNameRect = { x + 12, y + cardHeight - 54, x + cardWidth - 12, y + cardHeight - 12 };
		drawText(graphics, buildConfig[index].label, buildNameRect, 15.0f, Gdiplus::FontStyleBold, randomWinner ? RGB(5, 26, 16) : kText, Gdiplus::StringAlignmentCenter);
	}
}

void LoadoutEditor::drawPerkGrid(Gdiplus::Graphics& graphics)
{
	if (openPerkSlot_ < 0) {
		return;
	}

	drawRoundedPanel(graphics, popupArea_, RGB(7, 12, 19), kBorder, 8.0f, 1.5f);

	if (filteredIdx_.empty())
	{
		const RECT emptyRect = {
			listArea_.left + 12,
			listArea_.top,
			listArea_.right - 12,
			listArea_.bottom
		};
		drawText(graphics, L"No perks found", emptyRect, 15.0f, Gdiplus::FontStyleBold, kMutedText, Gdiplus::StringAlignmentCenter);
		return;
	}

	const int visibleCount = countVisiblePerks();
	const int endIndex = (std::min)(listScrollTop_ + visibleCount, static_cast<int>(filteredIdx_.size()));
	for (int filteredIndex = listScrollTop_; filteredIndex < endIndex; ++filteredIndex)
	{
		const int row = filteredIndex - listScrollTop_;
		const int iconIndex = filteredIdx_[filteredIndex];
		const PerkIconData& icon = perkDb_[iconIndex];
		const RECT rowRect = {
			listArea_.left,
			listArea_.top + (row * kPerkRowHeight),
			listArea_.right,
			listArea_.top + ((row + 1) * kPerkRowHeight)
		};

		const bool hovered = iconIndex == hoveredPerkIdx_;
		const COLORREF rowFill = hovered ? RGB(17, 35, 54) : RGB(7, 12, 19);
		Gdiplus::SolidBrush rowBrush(Gdiplus::Color(255, GetRValue(rowFill), GetGValue(rowFill), GetBValue(rowFill)));
		graphics.FillRectangle(
			&rowBrush,
			static_cast<Gdiplus::REAL>(rowRect.left),
			static_cast<Gdiplus::REAL>(rowRect.top),
			static_cast<Gdiplus::REAL>(rowRect.right - rowRect.left),
			static_cast<Gdiplus::REAL>(rowRect.bottom - rowRect.top));

		Gdiplus::Pen separatorPen(Gdiplus::Color(255, 31, 48, 66), 1.0f);
		graphics.DrawLine(
			&separatorPen,
			static_cast<Gdiplus::REAL>(rowRect.left),
			static_cast<Gdiplus::REAL>(rowRect.bottom - 1),
			static_cast<Gdiplus::REAL>(rowRect.right),
			static_cast<Gdiplus::REAL>(rowRect.bottom - 1));

		RECT iconRect = rowRect;
		iconRect.left += 10;
		iconRect.top += 7;
		iconRect.bottom -= 7;
		iconRect.right = iconRect.left + (iconRect.bottom - iconRect.top);
		drawPerkGem(graphics, iconRect, icon.filePath, false, 1.6f);

		const RECT textRect = { iconRect.right + 12, rowRect.top, rowRect.right - 8, rowRect.bottom };
		drawText(graphics, icon.displayName, textRect, 14.0f, Gdiplus::FontStyleBold, kText);
	}

	const RECT trackRect = scrollbarArea_;
	drawRoundedPanel(graphics, trackRect, RGB(18, 30, 45), RGB(68, 111, 152), 7.0f, 1.2f);

	const RECT thumbRect = computeScrollThumbRect();
	drawRoundedPanel(graphics, thumbRect, RGB(51, 74, 99), RGB(120, 174, 220), 6.0f, 1.2f);

	const int thumbCenterY = (thumbRect.top + thumbRect.bottom) / 2;
	Gdiplus::Pen gripPen(Gdiplus::Color(180, 188, 219, 242), 1.0f);
	for (int index = -1; index <= 1; ++index) {
		graphics.DrawLine(&gripPen, thumbRect.left + 4, thumbCenterY + (index * 4), thumbRect.right - 4, thumbCenterY + (index * 4));
	}

	drawPerkTooltip(graphics);
}

void LoadoutEditor::drawPerkTooltip(Gdiplus::Graphics& graphics)
{
	if (hoveredPerkIdx_ < 0 || hoveredPerkIdx_ >= static_cast<int>(perkDb_.size())) {
		return;
	}

	const PerkIconData& icon = perkDb_[hoveredPerkIdx_];
	RECT clientRect = {};
	GetClientRect(nativeWindow_, &clientRect);

	const int width = 360;
	const int height = 230;
	int x = popupArea_.right + 12;
	if (x + width > clientRect.right - 10) {
		x = popupArea_.left - width - 12;
	}
	if (x < clientRect.left + 10) {
		x = clientRect.right - width - 10;
	}

	int y = cursorPosition_.y - 24;
	if (y + height > clientRect.bottom - 10) {
		y = clientRect.bottom - height - 10;
	}
	if (y < clientRect.top + 10) {
		y = clientRect.top + 10;
	}

	const RECT shadowRect = { x + 5, y + 6, x + width + 5, y + height + 6 };
	Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(130, 0, 0, 0));
	graphics.FillRectangle(
		&shadowBrush,
		static_cast<Gdiplus::REAL>(shadowRect.left),
		static_cast<Gdiplus::REAL>(shadowRect.top),
		static_cast<Gdiplus::REAL>(shadowRect.right - shadowRect.left),
		static_cast<Gdiplus::REAL>(shadowRect.bottom - shadowRect.top));

	const RECT tooltipRect = { x, y, x + width, y + height };
	drawRoundedPanel(graphics, tooltipRect, RGB(11, 17, 27), kAccentSoft, 10.0f, 1.4f);

	const RECT titleRect = { x + 16, y + 12, x + width - 16, y + 38 };
	drawText(graphics, icon.displayName, titleRect, 17.0f, Gdiplus::FontStyleBold, kAccentSoft);

	if (!icon.internalName.empty() && icon.internalName != icon.displayName)
	{
		const RECT englishRect = { x + 16, y + 38, x + width - 16, y + 58 };
		drawText(graphics, icon.internalName, englishRect, 12.0f, Gdiplus::FontStyleRegular, kMutedText);
	}

	std::wstring characterText = L"Personaje: ";
	characterText += icon.characterLabel.empty() ? L"No disponible" : icon.characterLabel;
	const RECT characterRect = { x + 16, y + 62, x + width - 16, y + 84 };
	drawText(graphics, characterText, characterRect, 13.0f, Gdiplus::FontStyleBold, kText);

	const std::wstring description = icon.tooltipText.empty()
		? L"Descripción no disponible en los datos descargados de la wiki."
		: icon.tooltipText;
	const RECT descriptionRect = { x + 16, y + 92, x + width - 16, y + height - 16 };
	drawWrappedText(graphics, description, descriptionRect, 12.5f, Gdiplus::FontStyleRegular, RGB(218, 232, 248));
}

void LoadoutEditor::drawPerkGem(Gdiplus::Graphics& graphics, const RECT& rect, const std::wstring& perkPath, const bool selected, const float borderWidth)
{
	const float left = static_cast<float>(rect.left);
	const float top = static_cast<float>(rect.top);
	const float width = static_cast<float>(rect.right - rect.left);
	const float height = static_cast<float>(rect.bottom - rect.top);
	const float centerX = left + width / 2.0f;
	const float centerY = top + height / 2.0f;

	Gdiplus::PointF points[4] = {
		Gdiplus::PointF(centerX, top),
		Gdiplus::PointF(left + width, centerY),
		Gdiplus::PointF(centerX, top + height),
		Gdiplus::PointF(left, centerY)
	};

	Gdiplus::GraphicsPath diamondPath;
	diamondPath.AddPolygon(points, 4);

	Gdiplus::SolidBrush bgBrush(Gdiplus::Color(255, 28, 18, 39));
	Gdiplus::Pen borderPen(
		selected
			? Gdiplus::Color(255, GetRValue(kAccentSoft), GetGValue(kAccentSoft), GetBValue(kAccentSoft))
			: Gdiplus::Color(255, 95, 76, 126),
		borderWidth);
	graphics.FillPath(&bgBrush, &diamondPath);

	PerkIconData* icon = findPerkByPath(perkPath);
	if (icon != nullptr && icon->loadedImage == nullptr && !icon->loadAttempted && !icon->filePath.empty())
	{
		icon->loadAttempted = true;
		icon->loadedImage = Gdiplus::Image::FromFile(icon->filePath.c_str(), FALSE);
		if (icon->loadedImage == nullptr || icon->loadedImage->GetLastStatus() != Gdiplus::Ok)
		{
			delete icon->loadedImage;
			icon->loadedImage = nullptr;
		}
	}

	if (icon != nullptr && icon->loadedImage != nullptr)
	{
		const Gdiplus::GraphicsState state = graphics.Save();
		graphics.SetClip(&diamondPath);
		graphics.DrawImage(icon->loadedImage, Gdiplus::RectF(left, top, width, height));
		graphics.Restore(state);
	}
	else
	{
		Gdiplus::FontFamily family(L"Segoe UI");
		Gdiplus::Font font(&family, width > 50.0f ? 14.0f : 8.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
		Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, GetRValue(kMutedText), GetGValue(kMutedText), GetBValue(kMutedText)));
		Gdiplus::StringFormat format;
		format.SetAlignment(Gdiplus::StringAlignmentCenter);
		format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
		Gdiplus::RectF rectF(left, top, width, height);
		graphics.DrawString(L"PERK", -1, &font, rectF, &format, &textBrush);
	}

	graphics.DrawPath(&borderPen, &diamondPath);
}

void LoadoutEditor::drawStyledButton(const DRAWITEMSTRUCT* drawItem) const
{
	if (drawItem == nullptr) {
		return;
	}

	const bool isPressed = (drawItem->itemState & ODS_SELECTED) != 0;
	const bool isSave = drawItem->CtlID == kSaveBuildButtonId;
	const bool isDelete = drawItem->CtlID == kDeleteBuildButtonId;
	const bool isRandom = drawItem->CtlID == kRandomBuildButtonId;
	COLORREF fill = isSave ? RGB(24, 111, 166) : RGB(18, 27, 40);
	COLORREF border = isSave ? kAccentSoft : kBorder;
	COLORREF text = kText;
	if (isDelete)
	{
		fill = RGB(83, 26, 38);
		border = kDanger;
	}
	else if (isRandom)
	{
		fill = shuffleAnimActive_ ? RGB(37, 150, 190) : RGB(18, 45, 62);
		border = shuffleAnimActive_ ? RGB(255, 61, 61) : kAccentSoft;
	}

	if (isPressed) {
		fill = adjustColor(fill, -18);
	}

	const HBRUSH bgBrush = CreateSolidBrush(isRandom ? kPanelAlt : kPanel);
	FillRect(drawItem->hDC, &drawItem->rcItem, bgBrush);
	DeleteObject(bgBrush);

	RECT rect = drawItem->rcItem;
	InflateRect(&rect, -2, -2);
	const HBRUSH fillBrush = CreateSolidBrush(fill);
	const HPEN borderPen = CreatePen(PS_SOLID, 2, border);
	const HGDIOBJ oldBrush = SelectObject(drawItem->hDC, fillBrush);
	const HGDIOBJ oldPen = SelectObject(drawItem->hDC, borderPen);
	RoundRect(drawItem->hDC, rect.left, rect.top, rect.right, rect.bottom, 12, 12);
	SelectObject(drawItem->hDC, oldPen);
	SelectObject(drawItem->hDC, oldBrush);
	DeleteObject(borderPen);
	DeleteObject(fillBrush);

	wchar_t buttonText[64] = {};
	GetWindowTextW(drawItem->hwndItem, buttonText, 64);
	SetBkMode(drawItem->hDC, TRANSPARENT);
	SetTextColor(drawItem->hDC, text);
	DrawTextW(drawItem->hDC, buttonText, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void LoadoutEditor::openPerkSelector(const int slotIndex)
{
	if (slotIndex < 0 || slotIndex >= 4 || hSearchField_ == nullptr) {
		return;
	}

	if (hNameField_ != nullptr) {
		ShowWindow(hNameField_, SW_HIDE);
	}

	openPerkSlot_ = slotIndex;
	hoveredPerkIdx_ = -1;
	listScrollTop_ = 0;
	isScrollDragging_ = false;
	const RECT slotRect = slotBounds_[slotIndex];
	RECT clientRect = {};
	GetClientRect(nativeWindow_, &clientRect);

	const int width = 360;
	const int height = 390;
	int x = slotRect.right + 12;
	int y = slotRect.top - 20;
	if (x + width > clientRect.right - 18) {
		x = slotRect.left - width - 12;
	}
	if (y + height > clientRect.bottom - 70) {
		y = clientRect.bottom - height - 70;
	}
	y = (std::max)(20, y);

	popupArea_ = { x, y, x + width, y + height };
	searchArea_ = { x + 12, y + 12, x + width - 12, y + 42 };
	listArea_ = { x + 12, y + 54, x + width - 34, y + height - 12 };
	scrollbarArea_ = { x + width - 24, y + 54, x + width - 10, y + height - 12 };

	SetWindowTextW(hSearchField_, L"");
	applySearchFilter();
	SetWindowPos(
		hSearchField_,
		HWND_TOP,
		searchArea_.left,
		searchArea_.top,
		searchArea_.right - searchArea_.left,
		searchArea_.bottom - searchArea_.top,
		SWP_SHOWWINDOW);
	SetFocus(hSearchField_);
	InvalidateRect(nativeWindow_, nullptr, FALSE);
}

void LoadoutEditor::closePerkSelector()
{
	openPerkSlot_ = -1;
	hoveredPerkIdx_ = -1;
	isScrollDragging_ = false;
	if (hNameField_ != nullptr) {
		ShowWindow(hNameField_, SW_SHOW);
	}
	if (hSearchField_ != nullptr) {
		ShowWindow(hSearchField_, SW_HIDE);
	}
}

void LoadoutEditor::applySearchFilter()
{
	filteredIdx_.clear();
	hoveredPerkIdx_ = -1;
	const std::wstring query = getSearchQuery();

	for (int index = 0; index < static_cast<int>(perkDb_.size()); ++index)
	{
		if (matchesSearchText(perkDb_[index].filterToken, query)) {
			filteredIdx_.push_back(index);
		}
	}

	scrollPerkList(0);
}

void LoadoutEditor::updateHoverState(const POINT point)
{
	cursorPosition_ = point;

	int nextHoveredIndex = -1;
	if (openPerkSlot_ >= 0 && PtInRect(&listArea_, point))
	{
		const int rowIndex = (point.y - listArea_.top) / kPerkRowHeight;
		const int filteredIndex = listScrollTop_ + rowIndex;
		if (filteredIndex >= 0 && filteredIndex < static_cast<int>(filteredIdx_.size())) {
			nextHoveredIndex = filteredIdx_[filteredIndex];
		}
	}

	if (nextHoveredIndex != hoveredPerkIdx_)
	{
		hoveredPerkIdx_ = nextHoveredIndex;
		InvalidateRect(nativeWindow_, nullptr, FALSE);
	}

	if (!mouseLeftList_)
	{
		TRACKMOUSEEVENT trackMouse = {};
		trackMouse.cbSize = sizeof(TRACKMOUSEEVENT);
		trackMouse.dwFlags = TME_LEAVE;
		trackMouse.hwndTrack = nativeWindow_;
		mouseLeftList_ = TrackMouseEvent(&trackMouse) != FALSE;
	}
}

void LoadoutEditor::selectPerkAt(const POINT point)
{
	if (openPerkSlot_ < 0 || openPerkSlot_ >= 4 || !PtInRect(&listArea_, point)) {
		return;
	}

	const int rowIndex = (point.y - listArea_.top) / kPerkRowHeight;
	const int filteredIndex = listScrollTop_ + rowIndex;
	if (filteredIndex < 0 || filteredIndex >= static_cast<int>(filteredIdx_.size())) {
		return;
	}

	const int iconIndex = filteredIdx_[filteredIndex];
	editingBuild.perks[openPerkSlot_] = perkDb_[iconIndex].filePath;
	closePerkSelector();
	InvalidateRect(nativeWindow_, nullptr, FALSE);
}

void LoadoutEditor::scrollPerkList(const int topIndex)
{
	listScrollTop_ = (std::max)(0, (std::min)(computeMaxScrollTop(), topIndex));
	if (hoveredPerkIdx_ >= 0) {
		InvalidateRect(nativeWindow_, nullptr, FALSE);
	}
	else {
		InvalidateRect(nativeWindow_, &popupArea_, FALSE);
	}
}

void LoadoutEditor::beginScrollDrag(const int mouseY)
{
	const RECT thumbRect = computeScrollThumbRect();
	const POINT thumbPoint = { scrollbarArea_.left, mouseY };
	isScrollDragging_ = true;
	scrollOffset_ = PtInRect(&thumbRect, thumbPoint)
		? mouseY - thumbRect.top
		: (thumbRect.bottom - thumbRect.top) / 2;
	SetCapture(nativeWindow_);
	updateScrollDrag(mouseY);
}

void LoadoutEditor::updateScrollDrag(const int mouseY)
{
	if (!isScrollDragging_) {
		return;
	}

	const RECT thumbRect = computeScrollThumbRect();
	const int trackPadding = 3;
	const int trackTop = scrollbarArea_.top + trackPadding;
	const int trackBottom = scrollbarArea_.bottom - trackPadding;
	const int thumbHeight = thumbRect.bottom - thumbRect.top;
	const int thumbTravel = (std::max)(1, (trackBottom - trackTop) - thumbHeight);
	const int maxTopIndex = computeMaxScrollTop();
	const int thumbTop = (std::max)(trackTop, (std::min)(trackBottom - thumbHeight, mouseY - scrollOffset_));
	const int targetTopIndex = maxTopIndex == 0
		? 0
		: MulDiv(thumbTop - trackTop, maxTopIndex, thumbTravel);
	scrollPerkList(targetTopIndex);
}

void LoadoutEditor::endScrollDrag()
{
	if (!isScrollDragging_) {
		return;
	}

	isScrollDragging_ = false;
	if (GetCapture() == nativeWindow_) {
		ReleaseCapture();
	}
}

RECT LoadoutEditor::computeScrollThumbRect() const
{
	const int trackPadding = 3;
	const int trackTop = scrollbarArea_.top + trackPadding;
	const int trackBottom = scrollbarArea_.bottom - trackPadding;
	const int trackHeight = (std::max)(1, trackBottom - trackTop);
	const int totalCount = (std::max)(1, static_cast<int>(filteredIdx_.size()));
	const int visibleCount = countVisiblePerks();
	const int maxTopIndex = computeMaxScrollTop();
	const int thumbHeight = maxTopIndex == 0
		? trackHeight
		: (std::max)(42, MulDiv(trackHeight, visibleCount, totalCount));
	const int thumbTravel = (std::max)(1, trackHeight - thumbHeight);
	const int thumbTop = maxTopIndex == 0
		? trackTop
		: trackTop + MulDiv(listScrollTop_, thumbTravel, maxTopIndex);

	return { scrollbarArea_.left + 2, thumbTop, scrollbarArea_.right - 2, thumbTop + thumbHeight };
}

int LoadoutEditor::countVisiblePerks() const
{
	const int listHeight = (std::max)(1, static_cast<int>(listArea_.bottom - listArea_.top));
	return (std::max)(1, listHeight / kPerkRowHeight);
}

int LoadoutEditor::computeMaxScrollTop() const
{
	return (std::max)(0, static_cast<int>(filteredIdx_.size()) - countVisiblePerks());
}

int LoadoutEditor::countShuffleCandidates() const
{
	const int savedCount = static_cast<int>(getActiveRoleList().size());
	if (savedCount <= 0) {
		return 0;
	}

	if (!cardBounds_.empty()) {
		return (std::min)(savedCount, static_cast<int>(cardBounds_.size()));
	}

	return savedCount;
}

void LoadoutEditor::resetRandomShuffle(const bool clearFinal)
{
	if (nativeWindow_ != nullptr) {
		KillTimer(nativeWindow_, kRandomBuildTimerId);
	}

	shuffleAnimActive_ = false;
	shuffleHighlightIdx_ = -1;
	shuffleTickCount_ = 0;
	shuffleTotalTicks_ = 0;
	if (clearFinal) {
		shuffleFinalIdx_ = -1;
	}

	if (hRandomBtn_ != nullptr) {
		InvalidateRect(hRandomBtn_, nullptr, FALSE);
	}
}

void LoadoutEditor::startRandomShuffle()
{
	const int buildCount = countShuffleCandidates();
	if (buildCount <= 0) {
		return;
	}

	closePerkSelector();

	std::uniform_int_distribution<int> buildDistribution(0, buildCount - 1);
	std::uniform_int_distribution<int> stepDistribution(20, 32);
	shuffleAnimActive_ = true;
	shuffleHighlightIdx_ = buildDistribution(rng_);
	shuffleFinalIdx_ = buildDistribution(rng_);
	shuffleTickCount_ = 0;
	shuffleTotalTicks_ = stepDistribution(rng_);

	playBuildAudio(kBuildRandomStartSoundPath, kBuildRandomStartSoundAlias);
	SetTimer(nativeWindow_, kRandomBuildTimerId, 82, nullptr);
	InvalidateRect(nativeWindow_, nullptr, FALSE);
	if (hRandomBtn_ != nullptr) {
		InvalidateRect(hRandomBtn_, nullptr, FALSE);
	}
}

void LoadoutEditor::advanceRandomShuffle()
{
	const int buildCount = countShuffleCandidates();
	if (buildCount <= 0)
	{
		resetRandomShuffle(true);
		InvalidateRect(nativeWindow_, nullptr, FALSE);
		return;
	}

	if (shuffleTickCount_ >= shuffleTotalTicks_)
	{
		shuffleAnimActive_ = false;
		shuffleHighlightIdx_ = -1;
		shuffleFinalIdx_ = (std::max)(0, (std::min)(buildCount - 1, shuffleFinalIdx_));
		selectedBuildIndex_ = shuffleFinalIdx_;
		editingBuild = getActiveRoleList()[shuffleFinalIdx_];
		if (hNameField_ != nullptr) {
			SetWindowTextW(hNameField_, editingBuild.label.c_str());
		}

		KillTimer(nativeWindow_, kRandomBuildTimerId);
		playBuildAudio(kBuildRandomEndSoundPath, kBuildRandomEndSoundAlias);
		InvalidateRect(nativeWindow_, nullptr, FALSE);
		if (hRandomBtn_ != nullptr) {
			InvalidateRect(hRandomBtn_, nullptr, FALSE);
		}
		return;
	}

	std::uniform_int_distribution<int> buildDistribution(0, buildCount - 1);
	int nextIndex = buildDistribution(rng_);
	if (buildCount > 1)
	{
		for (int attempts = 0; attempts < 4 && nextIndex == shuffleHighlightIdx_; ++attempts) {
			nextIndex = buildDistribution(rng_);
		}
	}

	shuffleHighlightIdx_ = nextIndex;
	++shuffleTickCount_;
	InvalidateRect(nativeWindow_, nullptr, FALSE);
}

void LoadoutEditor::selectBuild(const int buildIndex)
{
	const std::vector<PerkSlot>& buildConfig = getActiveRoleList();
	if (buildIndex < 0 || buildIndex >= static_cast<int>(buildConfig.size())) {
		return;
	}

	resetRandomShuffle(true);
	selectedBuildIndex_ = buildIndex;
	editingBuild = buildConfig[buildIndex];
	if (hNameField_ != nullptr) {
		SetWindowTextW(hNameField_, editingBuild.label.c_str());
	}
	closePerkSelector();
	InvalidateRect(nativeWindow_, nullptr, FALSE);
}

void LoadoutEditor::createNewBuild()
{
	resetRandomShuffle(true);
	selectedBuildIndex_ = -1;
	editingBuild = PerkSlot();
	editingBuild.label = currentSide == LoadoutSide::SideSurvivor ? L"New SideSurvivor Build" : L"New SideKiller Build";
	if (hNameField_ != nullptr) {
		SetWindowTextW(hNameField_, editingBuild.label.c_str());
	}
	closePerkSelector();
	InvalidateRect(nativeWindow_, nullptr, FALSE);
}

void LoadoutEditor::readNameFieldValue()
{
	if (hNameField_ == nullptr) {
		return;
	}

	const int length = GetWindowTextLengthW(hNameField_);
	std::wstring name(length + 1, L'\0');
	GetWindowTextW(hNameField_, &name[0], length + 1);
	name.resize(length);
	if (name.empty()) {
		name = currentSide == LoadoutSide::SideSurvivor ? L"SideSurvivor Build" : L"SideKiller Build";
	}

	editingBuild.label = name;
}

void LoadoutEditor::saveCurrentBuild()
{
	readNameFieldValue();
	std::vector<PerkSlot>& buildConfig = getActiveRoleList();

	if (selectedBuildIndex_ >= 0 && selectedBuildIndex_ < static_cast<int>(buildConfig.size())) {
		buildConfig[selectedBuildIndex_] = editingBuild;
	}
	else
	{
		buildConfig.push_back(editingBuild);
		selectedBuildIndex_ = static_cast<int>(buildConfig.size()) - 1;
	}

	saveConfig(appSettings_);
	closePerkSelector();
	InvalidateRect(nativeWindow_, nullptr, FALSE);
}

void LoadoutEditor::deleteCurrentBuild()
{
	resetRandomShuffle(true);
	std::vector<PerkSlot>& buildConfig = getActiveRoleList();
	if (selectedBuildIndex_ < 0 || selectedBuildIndex_ >= static_cast<int>(buildConfig.size())) {
		return;
	}

	buildConfig.erase(buildConfig.begin() + selectedBuildIndex_);
	selectedBuildIndex_ = -1;
	if (!buildConfig.empty())
	{
		selectedBuildIndex_ = (std::min)(static_cast<int>(buildConfig.size()) - 1, 0);
		editingBuild = buildConfig[selectedBuildIndex_];
	}
	else
	{
		editingBuild = PerkSlot();
		editingBuild.label = currentSide == LoadoutSide::SideSurvivor ? L"New SideSurvivor Build" : L"New SideKiller Build";
	}

	if (hNameField_ != nullptr) {
		SetWindowTextW(hNameField_, editingBuild.label.c_str());
	}

	saveConfig(appSettings_);
	closePerkSelector();
	InvalidateRect(nativeWindow_, nullptr, FALSE);
}

std::vector<PerkSlot>& LoadoutEditor::getActiveRoleList()
{
	return currentSide == LoadoutSide::SideSurvivor ? appSettings_.loadoutData.survivor : appSettings_.loadoutData.killer;
}

const std::vector<PerkSlot>& LoadoutEditor::getActiveRoleList() const
{
	return currentSide == LoadoutSide::SideSurvivor ? appSettings_.loadoutData.survivor : appSettings_.loadoutData.killer;
}

PerkIconData* LoadoutEditor::findPerkByPath(const std::wstring& perkPath)
{
	for (PerkIconData& icon : perkDb_)
	{
		if (icon.filePath == perkPath) {
			return &icon;
		}
	}

	return nullptr;
}

std::wstring LoadoutEditor::getSearchQuery() const
{
	if (hSearchField_ == nullptr) {
		return L"";
	}

	const int length = GetWindowTextLengthW(hSearchField_);
	std::wstring text(length + 1, L'\0');
	GetWindowTextW(hSearchField_, &text[0], length + 1);
	text.resize(length);
	return text;
}

std::wstring LoadoutEditor::getSideLabel() const
{
	return currentSide == LoadoutSide::SideSurvivor ? L"SURVIVOR" : L"KILLER";
}

std::wstring LoadoutEditor::normalizePerkName(const std::wstring& fileName)
{
	std::wstring name = fileName;
	const size_t dotPos = name.find_last_of(L'.');
	if (dotPos != std::wstring::npos) {
		name = name.substr(0, dotPos);
	}

	const std::wstring prefix = L"iconPerks_";
	if (name.find(prefix) == 0) {
		name = name.substr(prefix.size());
	}

	std::wstring cleaned;
	for (size_t index = 0; index < name.size(); ++index)
	{
		const wchar_t ch = name[index];
		if (index > 0 && iswupper(ch) && iswlower(name[index - 1])) {
			cleaned += L' ';
		}
		cleaned += ch;
	}

	return cleaned.empty() ? L"Perk" : cleaned;
}

LRESULT LoadoutEditor::processMessage(UINT windowMessage, WPARAM wideParameter, LPARAM longParameter)
{
	switch (windowMessage)
	{
	case WM_CREATE:
		buildUI();
		return 0;
	case WM_DESTROY:
		endScrollDrag();
		resetRandomShuffle(true);
		stopBuildAudio(kBuildRandomEndSoundAlias);
		releaseGdi();
		nativeWindow_ = nullptr;
		return 0;
	case WM_SIZE:
		if (renderBuffer_ != nullptr)
		{
			delete renderBuffer_;
			renderBuffer_ = nullptr;
			cachedBufW_ = 0;
			cachedBufH_ = 0;
		}
		computeWidgetLayout();
		InvalidateRect(nativeWindow_, nullptr, FALSE);
		return 0;
	case WM_COMMAND:
		switch (LOWORD(wideParameter))
		{
		case kNewBuildButtonId:
			createNewBuild();
			return 0;
		case kSaveBuildButtonId:
			saveCurrentBuild();
			return 0;
		case kDeleteBuildButtonId:
			deleteCurrentBuild();
			return 0;
		case kRandomBuildButtonId:
			startRandomShuffle();
			return 0;
		case kPerkSearchEditId:
			if (HIWORD(wideParameter) == EN_CHANGE && openPerkSlot_ >= 0) {
				applySearchFilter();
				InvalidateRect(nativeWindow_, nullptr, FALSE);
			}
			return 0;
		default:
			break;
		}
		break;
	case WM_TIMER:
		if (wideParameter == kRandomBuildTimerId)
		{
			advanceRandomShuffle();
			return 0;
		}
		break;
	case WM_LBUTTONDOWN:
	{
		const POINT point = { GET_X_LPARAM(longParameter), GET_Y_LPARAM(longParameter) };
		if (openPerkSlot_ >= 0)
		{
			if (PtInRect(&scrollbarArea_, point)) {
				beginScrollDrag(point.y);
				return 0;
			}

			if (PtInRect(&listArea_, point)) {
				selectPerkAt(point);
				return 0;
			}

			if (PtInRect(&popupArea_, point)) {
				return 0;
			}
		}

		for (int index = 0; index < 4; ++index)
		{
			if (PtInRect(&slotBounds_[index], point))
			{
				openPerkSelector(index);
				return 0;
			}
		}

		for (int index = 0; index < static_cast<int>(cardBounds_.size()); ++index)
		{
			if (PtInRect(&cardBounds_[index], point))
			{
				selectBuild(index);
				return 0;
			}
		}

		closePerkSelector();
		InvalidateRect(nativeWindow_, nullptr, FALSE);
		return 0;
	}
	case WM_MOUSEMOVE:
		updateHoverState({ GET_X_LPARAM(longParameter), GET_Y_LPARAM(longParameter) });
		if (isScrollDragging_) {
			updateScrollDrag(GET_Y_LPARAM(longParameter));
			return 0;
		}
		break;
	case WM_MOUSELEAVE:
		mouseLeftList_ = false;
		hoveredPerkIdx_ = -1;
		InvalidateRect(nativeWindow_, nullptr, FALSE);
		return 0;
	case WM_LBUTTONUP:
		endScrollDrag();
		break;
	case WM_MOUSEWHEEL:
		if (openPerkSlot_ >= 0)
		{
			POINT point = { GET_X_LPARAM(longParameter), GET_Y_LPARAM(longParameter) };
			ScreenToClient(nativeWindow_, &point);
			if (PtInRect(&popupArea_, point))
			{
				const int delta = GET_WHEEL_DELTA_WPARAM(wideParameter);
				const int direction = delta > 0 ? -3 : 3;
				scrollPerkList(listScrollTop_ + direction);
				return 0;
			}
		}
		break;
	case WM_DRAWITEM:
		if (wideParameter == kNewBuildButtonId ||
			wideParameter == kSaveBuildButtonId ||
			wideParameter == kDeleteBuildButtonId ||
			wideParameter == kRandomBuildButtonId)
		{
			drawStyledButton(reinterpret_cast<const DRAWITEMSTRUCT*>(longParameter));
			return TRUE;
		}
		break;
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	{
		const HDC hdc = reinterpret_cast<HDC>(wideParameter);
		SetTextColor(hdc, kText);
		SetBkColor(hdc, RGB(8, 13, 20));
		return reinterpret_cast<INT_PTR>(inputFieldBrush_);
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
		onPaint();
		return 0;
	default:
		break;
	}

	return DefWindowProc(id(), windowMessage, wideParameter, longParameter);
}
