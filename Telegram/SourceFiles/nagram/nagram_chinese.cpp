#include "nagram/nagram_translation.h"

#ifdef Q_OS_MAC
#include <CoreFoundation/CoreFoundation.h>
#elif defined Q_OS_WIN
#include <windows.h>
#endif

namespace Nagram {

std::optional<QString> ConvertChineseText(const QString &text, bool traditional) {
	if (text.isEmpty()) {
		return text;
	}
#ifdef Q_OS_MAC
	const auto string = CFStringCreateMutable(nullptr, 0);
	CFStringAppendCharacters(
		string,
		reinterpret_cast<const UniChar*>(text.utf16()),
		text.size());
	const auto ok = CFStringTransform(
		string,
		nullptr,
		CFSTR("Traditional-Simplified"),
		traditional);
	if (!ok) {
		CFRelease(string);
		return std::nullopt;
	}
	auto result = QString(CFStringGetLength(string), Qt::Uninitialized);
	CFStringGetCharacters(
		string,
		CFRangeMake(0, result.size()),
		reinterpret_cast<UniChar*>(result.data()));
	CFRelease(string);
	return result;
#elif defined Q_OS_WIN
	const auto flags = traditional ? LCMAP_TRADITIONAL_CHINESE : LCMAP_SIMPLIFIED_CHINESE;
	const auto source = reinterpret_cast<LPCWSTR>(text.utf16());
	const auto length = LCMapStringEx(
		L"zh-CN", flags, source, text.size(), nullptr, 0, nullptr, nullptr, 0);
	if (!length) {
		return std::nullopt;
	}
	auto result = QString(length, Qt::Uninitialized);
	if (LCMapStringEx(L"zh-CN", flags, source, text.size(),
			reinterpret_cast<LPWSTR>(result.data()), length, nullptr, nullptr, 0) != length) {
		return std::nullopt;
	}
	return result;
#else
	return std::nullopt;
#endif
}

bool ChineseConversionAvailable() {
	static const auto result = ConvertChineseText(u"简体"_q, true) == u"簡體"_q
		&& ConvertChineseText(u"繁體"_q, false) == u"繁体"_q;
	return result;
}

std::optional<TextWithEntities> ConvertChinese(
		TextWithEntities text,
		bool traditional) {
	const auto plan = PlanTranslation(std::move(text));
	if (!plan) {
		return std::nullopt;
	}
	auto translated = QStringList();
	translated.reserve(plan->texts.size());
	for (const auto &part : plan->texts) {
		const auto converted = ConvertChineseText(part, traditional);
		if (!converted) {
			return std::nullopt;
		}
		translated.push_back(*converted);
	}
	return ApplyTranslation(*plan, translated);
}

} // namespace Nagram
