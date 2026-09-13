#pragma once

#include "ui/text/text_entity.h"

class HistoryItem;
namespace Ui { class PopupMenu; }

namespace Core {
class Settings;
} // namespace Core

namespace Nagram {

inline constexpr auto kReadingChineseKey = std::string_view("nagram.readingChinese");

void AddReadingMenu(not_null<Ui::PopupMenu*> menu, not_null<HistoryItem*> item);

[[nodiscard]] QString ReadingChinese(Core::Settings &settings);
void SetReadingChinese(Core::Settings &settings, const QString &mode);

struct ReadingProjection {
	TextWithEntities text;
	std::vector<int> fromDisplay;
	std::vector<int> originalStarts;
	std::vector<int> originalEnds;

	[[nodiscard]] TextSelection toOriginal(TextSelection selection) const;
	[[nodiscard]] TextSelection toDisplay(TextSelection selection) const;
};

[[nodiscard]] ReadingProjection ComposeReading(
	ReadingProjection first,
	ReadingProjection second);

[[nodiscard]] std::optional<ReadingProjection> ProjectReading(
	Core::Settings &settings,
	const TextWithEntities &original);

} // namespace Nagram
