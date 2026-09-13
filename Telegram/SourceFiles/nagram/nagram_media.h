#pragma once

class HistoryItem;

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Nagram {

[[nodiscard]] bool ShouldRevealMediaSpoiler(not_null<HistoryItem*> item);
[[nodiscard]] QString MediaDetailsText(not_null<HistoryItem*> item);
void AddMediaDetailsAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<Window::SessionController*> controller,
	HistoryItem *item);

} // namespace Nagram
