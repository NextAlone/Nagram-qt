#pragma once

namespace Window {
class SessionController;
} // namespace Window

namespace Ui {
class PopupMenu;
} // namespace Ui

namespace Data {
class Thread;
} // namespace Data

namespace Nagram {

void AddRepeatActions(
	not_null<Ui::PopupMenu*> menu,
	not_null<Window::SessionController*> controller,
	not_null<HistoryItem*> item);

[[nodiscard]] bool MessageForwardable(not_null<HistoryItem*> item);

} // namespace Nagram
