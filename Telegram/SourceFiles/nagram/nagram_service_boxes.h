#pragma once

class HistoryItem;

namespace Main {
class SessionShow;
} // namespace Main

namespace Ui {
class InputField;
} // namespace Ui

namespace Nagram {

void ShowDraftTranslation(
	std::shared_ptr<Main::SessionShow> show,
	not_null<Ui::InputField*> field);
[[nodiscard]] bool CustomTranscriptionSelected();
void ShowCustomTranscription(
	std::shared_ptr<Main::SessionShow> show,
	not_null<HistoryItem*> item,
	bool manage = false);

} // namespace Nagram
