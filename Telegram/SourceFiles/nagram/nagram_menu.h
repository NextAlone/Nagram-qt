#pragma once

#include "nagram/nagram_settings.h"
#include "base/unique_qptr.h"
#include "ui/style/style_core_icon.h"

#include <QtCore/QJsonObject>
#include <QtCore/Qt>

namespace Ui {
class PopupMenu;
namespace Menu {
class ItemBase;
} // namespace Menu
} // namespace Ui

namespace Nagram {

inline constexpr auto kMenuToolsKey = std::string_view("nagram.messageMenu");

enum class MenuAction {
	Reply,
	Edit,
	Copy,
	CopyLink,
	Forward,
	Translate,
	Pin,
	Select,
	Statistics,
	Report,
	Block,
	Save,
	Delete,
	EmojiPacks,
	// Order of the entries above is frozen: stored `order[]` arrays rely on
	// their string ids, and new entries must always be appended before Count.
	Repeat,
	RepeatNoQuote,
	ForwardNoQuote,
	SelectAuthor,
	Snapshot,
	Batch,
	Filter,
	StickerSet,
	SaveSticker,
	OpenGif,
	SaveGif,
	ShowInFolder,
	OpenWith,
	Transcribe,
	Offer,
	SendNow,
	Reschedule,
	TodoAdd,
	ViewReplies,
	Factcheck,
	GoToMessage,
	PollRetract,
	PollStop,
	SaveRingtone,
	MessageAuthor,
	SendGift,
	Count,
};

// Actions hidden by default (Nagram enhancements and the repeat family). Every
// other action is shown by default in its native position.
[[nodiscard]] bool MenuActionDefaultHidden(MenuAction action);

[[nodiscard]] QString MenuActionId(MenuAction action);
[[nodiscard]] QString MenuActionTitle(MenuAction action);
[[nodiscard]] QJsonObject MenuToolsDefaults();
[[nodiscard]] bool ValidMenuTools(const QJsonObject &value);
[[nodiscard]] std::optional<QJsonObject> MenuTools(Core::Settings &settings);
void SetMenuTools(Core::Settings &settings, const QJsonObject &value);
[[nodiscard]] bool MenuHidden(Core::Settings &settings, Option option);
[[nodiscard]] bool MenuHidden(
	Core::Settings &settings,
	Option option,
	Qt::KeyboardModifiers modifiers);
[[nodiscard]] bool MenuHidden(Core::Settings &settings, MenuAction action);
[[nodiscard]] bool MenuHidden(
	Core::Settings &settings,
	MenuAction action,
	Qt::KeyboardModifiers modifiers);
not_null<QAction*> AddOrderedMenuAction(
	not_null<Ui::PopupMenu*> menu,
	MenuAction id,
	const QString &text,
	Fn<void()> callback,
	const style::icon *icon = nullptr,
	const style::icon *iconOver = nullptr);
not_null<QAction*> AddOrderedMenuAction(
	not_null<Ui::PopupMenu*> menu,
	MenuAction id,
	base::unique_qptr<Ui::Menu::ItemBase> item);

} // namespace Nagram
