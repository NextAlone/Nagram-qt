#pragma once

class HistoryItem;
namespace Ui { class PopupMenu; }
namespace Window { class SessionController; }
namespace Settings {
namespace Builder { class SectionBuilder; }
void BuildNagramFilters(Builder::SectionBuilder &builder);
void AddNagramFilterMenu(
	not_null<Ui::PopupMenu*> menu,
	not_null<HistoryItem*> item,
	not_null<Window::SessionController*> controller);
} // namespace Settings
