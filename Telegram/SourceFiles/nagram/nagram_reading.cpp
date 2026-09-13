#include "nagram/nagram_reading.h"

#include "core/core_settings.h"
#include "core/application.h"
#include "history/history_item.h"
#include "history/history.h"
#include "data/data_session.h"
#include "lang/lang_keys.h"
#include "nagram/nagram_menu.h"
#include "nagram/nagram_filters.h"
#include "ui/widgets/popup_menu.h"
#include "nagram/nagram_settings.h"
#include "nagram/nagram_text.h"
#include "nagram/nagram_translation.h"

#include <numeric>

namespace Nagram {

void AddReadingMenu(not_null<Ui::PopupMenu*> menu, not_null<HistoryItem*> item) {
	const auto filtered = FilterMessage(item);
	if (!item->nagramOriginalShown() && !filtered
		&& (item->translatedRichPage() || item->originalText().empty()
			|| !ProjectReading(Core::App().settings(), item->translatedTextWithLocalEntities()))) {
		return;
	}
	const auto session = &item->history()->owner();
	const auto id = item->fullId();
	AddOrderedMenuAction(menu, MenuAction::Translate,
		item->nagramOriginalShown()
			? tr::lng_nagram_reading_apply(tr::now)
			: tr::lng_nagram_reading_original(tr::now), [=] {
			if (const auto item = session->message(id)) {
				item->setNagramOriginalShown(!item->nagramOriginalShown());
			}
		});
}

QString ReadingChinese(Core::Settings &settings) {
	const auto value = QString::fromUtf8(
		settings.readPref<QByteArray>(kReadingChineseKey));
	if (value.isEmpty() || value == u"simplified"_q || value == u"traditional"_q) {
		return value;
	}
	LOG(("Nagram Error: Invalid reading Chinese preference; conversion not applied."));
	return QString();
}

void SetReadingChinese(Core::Settings &settings, const QString &mode) {
	Expects(mode.isEmpty() || mode == u"simplified"_q || mode == u"traditional"_q);
	if (mode.isEmpty()) {
		settings.clearPref(kReadingChineseKey);
	} else {
		settings.writePref<QByteArray>(kReadingChineseKey, mode.toUtf8());
	}
}

TextSelection ReadingProjection::toOriginal(TextSelection selection) const {
	if (selection.from > selection.to || selection.from >= fromDisplay.size()
		|| selection.to >= fromDisplay.size()
		|| fromDisplay[selection.from] < 0 || fromDisplay[selection.to] < 0) {
		return {};
	}
	for (auto i = int(selection.from); i <= selection.to; ++i) {
		if (fromDisplay[i] < 0 || (i > selection.from
			&& (fromDisplay[i] < fromDisplay[i - 1]
				|| fromDisplay[i] > fromDisplay[i - 1] + 1))) {
			return {};
		}
	}
	return { uint16(fromDisplay[selection.from]), uint16(fromDisplay[selection.to]) };
}

TextSelection ReadingProjection::toDisplay(TextSelection selection) const {
	if (selection.from > selection.to || selection.from >= originalStarts.size()
		|| selection.to >= originalEnds.size()
		|| originalStarts[selection.from] < 0 || originalEnds[selection.to] < 0) {
		return {};
	}
	const auto result = TextSelection{
		uint16(originalStarts[selection.from]),
		uint16(originalEnds[selection.to]),
	};
	return toOriginal(result) == selection ? result : TextSelection();
}

ReadingProjection ComposeReading(
		ReadingProjection first,
		ReadingProjection second) {
	for (auto &offset : second.fromDisplay) {
		offset = (offset < 0 || offset >= first.fromDisplay.size())
			? -1 : first.fromDisplay[offset];
	}
	for (auto &offset : first.originalStarts) {
		offset = (offset < 0 || offset >= second.originalStarts.size())
			? -1 : second.originalStarts[offset];
	}
	for (auto &offset : first.originalEnds) {
		offset = (offset < 0 || offset >= second.originalEnds.size())
			? -1 : second.originalEnds[offset];
	}
	second.originalStarts = std::move(first.originalStarts);
	second.originalEnds = std::move(first.originalEnds);
	return second;
}

std::optional<ReadingProjection> ProjectReading(
		Core::Settings &settings,
		const TextWithEntities &original) {
	const auto spacing = Get(settings, Option::PanguOnReading);
	const auto chinese = ReadingChinese(settings);
	if ((!spacing && chinese.isEmpty()) || original.empty() || original.text.size() >= 0xFFFF) {
		return std::nullopt;
	}
	auto result = ReadingProjection{ .text = original };
	result.fromDisplay.resize(original.text.size() + 1);
	std::iota(result.fromDisplay.begin(), result.fromDisplay.end(), 0);
	result.originalStarts = result.fromDisplay;
	result.originalEnds = result.fromDisplay;
	if (!chinese.isEmpty()) {
		const auto plan = PlanTranslation(original);
		auto translated = QStringList();
		if (!plan) {
			return std::nullopt;
		}
		for (const auto &text : plan->texts) {
			const auto converted = ConvertChineseText(text, chinese == u"traditional"_q);
			if (!converted) {
				LOG(("Nagram Error: System Chinese conversion unavailable."));
				return std::nullopt;
			}
			translated.push_back(*converted);
		}
		const auto converted = ApplyTranslation(*plan, translated);
		if (!converted) {
			return std::nullopt;
		}
		result.text = *converted;
		result.fromDisplay.assign(result.text.text.size() + 1, -1);
		result.originalStarts.assign(original.text.size() + 1, -1);
		auto offset = 0;
		for (const auto &part : plan->parts) {
			const auto length = part.index < 0 ? part.length : int(translated[part.index].size());
			result.fromDisplay[offset] = part.start;
			result.originalStarts[part.start] = offset;
			if (length == part.length) {
				for (auto i = 0; i < length; ++i) {
					result.fromDisplay[offset + i] = part.start + i;
					result.originalStarts[part.start + i] = offset + i;
				}
			}
			offset += length;
			result.fromDisplay[offset] = part.start + part.length;
			result.originalStarts[part.start + part.length] = offset;
		}
		result.originalEnds = result.originalStarts;
	}
	if (spacing) {
		const auto spaced = AddTextSpacing(result.text);
		if (spaced.text != result.text.text) {
			auto starts = std::vector<int>(result.text.text.size() + 1);
			auto ends = starts;
			auto from = std::vector<int>(spaced.text.size() + 1, -1);
			auto offset = 0;
			for (auto i = 0; i < result.text.text.size(); ++i) {
				ends[i] = offset;
				from[offset] = result.fromDisplay[i];
				if (spaced.text[offset] != result.text.text[i]) {
					++offset;
					from[offset] = result.fromDisplay[i];
				}
				starts[i] = offset;
				++offset;
			}
			starts.back() = ends.back() = offset;
			from.back() = result.fromDisplay.back();
			for (auto &value : result.originalStarts) {
				value = value < 0 ? -1 : starts[value];
			}
			for (auto &value : result.originalEnds) {
				value = value < 0 ? -1 : ends[value];
			}
			result.fromDisplay = std::move(from);
			result.text = spaced;
		}
	}
	if (result.text.text == original.text || result.text.text.size() >= 0xFFFF) {
		return std::nullopt;
	}
	return result;
}

} // namespace Nagram
