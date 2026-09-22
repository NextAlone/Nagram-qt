#include "nagram/nagram_menu.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "lang/lang_keys.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/menu/menu.h"
#include "ui/widgets/popup_menu.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtGui/QGuiApplication>

namespace Nagram {
namespace {

constexpr auto kIds = std::array{
	"reply", "edit", "copy", "copyLink", "forward", "translate", "pin",
	"select", "statistics", "report", "block", "save", "delete", "emojiPacks",
	"repeat", "repeatNoQuote", "forwardNoQuote",
	"selectAuthor", "snapshot", "batch", "filter",
	"stickerSet", "saveSticker", "openGif", "saveGif", "showInFolder",
	"openWith", "transcribe", "offer", "sendNow", "reschedule", "todoAdd",
	"viewReplies", "factcheck", "goToMessage", "pollRetract", "pollStop",
	"saveRingtone", "messageAuthor", "sendGift",
};
static_assert(kIds.size() == int(MenuAction::Count));

constexpr auto kDefaultHidden = std::array{
	MenuAction::Repeat,
	MenuAction::RepeatNoQuote,
	MenuAction::ForwardNoQuote,
	MenuAction::SelectAuthor,
	MenuAction::Snapshot,
	MenuAction::Batch,
	MenuAction::Filter,
};

std::optional<MenuAction> HiddenAction(Option option) {
	switch (option) {
	case Option::HideMenuPin: return MenuAction::Pin;
	case Option::HideMenuReport: return MenuAction::Report;
	case Option::HideMenuBlock: return MenuAction::Block;
	case Option::HideMenuStatistics: return MenuAction::Statistics;
	case Option::HideMenuCopyLink: return MenuAction::CopyLink;
	case Option::HideMenuForward: return MenuAction::Forward;
	case Option::HideMenuTranslate: return MenuAction::Translate;
	case Option::HideMenuSelect: return MenuAction::Select;
	case Option::HideMenuEmojiPacks: return MenuAction::EmojiPacks;
	default: return std::nullopt;
	}
}

} // namespace

QString MenuActionId(MenuAction action) {
	Expects(action != MenuAction::Count);
	return QString::fromLatin1(kIds[int(action)]);
}

QString MenuActionTitle(MenuAction action) {
	switch (action) {
	case MenuAction::Reply: return tr::lng_nagram_action_reply(tr::now);
	case MenuAction::Edit: return tr::lng_nagram_action_edit(tr::now);
	case MenuAction::Copy: return tr::lng_nagram_action_copy(tr::now);
	case MenuAction::CopyLink: return tr::lng_nagram_action_copy_link(tr::now);
	case MenuAction::Forward: return tr::lng_nagram_action_forward(tr::now);
	case MenuAction::Translate: return tr::lng_nagram_action_translate(tr::now);
	case MenuAction::Pin: return tr::lng_nagram_action_pin(tr::now);
	case MenuAction::Select: return tr::lng_nagram_action_select(tr::now);
	case MenuAction::Statistics: return tr::lng_nagram_action_statistics(tr::now);
	case MenuAction::Report: return tr::lng_nagram_action_report(tr::now);
	case MenuAction::Block: return tr::lng_nagram_action_block(tr::now);
	case MenuAction::Save: return tr::lng_nagram_action_save(tr::now);
	case MenuAction::Delete: return tr::lng_nagram_action_delete(tr::now);
	case MenuAction::EmojiPacks: return tr::lng_nagram_menu_emoji_packs(tr::now);
	case MenuAction::Repeat: return tr::lng_nagram_action_repeat(tr::now);
	case MenuAction::RepeatNoQuote:
		return tr::lng_nagram_action_repeat_no_quote(tr::now);
	case MenuAction::ForwardNoQuote:
		return tr::lng_nagram_action_forward_no_quote(tr::now);
	case MenuAction::SelectAuthor: return tr::lng_nagram_select_author(tr::now);
	case MenuAction::Snapshot: return tr::lng_nagram_snapshot(tr::now);
	case MenuAction::Batch: return tr::lng_nagram_batch_title(tr::now);
	case MenuAction::Filter: return tr::lng_nagram_action_filter(tr::now);
	case MenuAction::StickerSet: return tr::lng_nagram_action_sticker_set(tr::now);
	case MenuAction::SaveSticker:
		return tr::lng_nagram_action_save_sticker(tr::now);
	case MenuAction::OpenGif: return tr::lng_nagram_action_open_gif(tr::now);
	case MenuAction::SaveGif: return tr::lng_nagram_action_save_gif(tr::now);
	case MenuAction::ShowInFolder:
		return tr::lng_nagram_action_show_in_folder(tr::now);
	case MenuAction::OpenWith: return tr::lng_nagram_action_open_with(tr::now);
	case MenuAction::Transcribe: return tr::lng_nagram_action_transcribe(tr::now);
	case MenuAction::Offer: return tr::lng_nagram_action_offer(tr::now);
	case MenuAction::SendNow: return tr::lng_nagram_action_send_now(tr::now);
	case MenuAction::Reschedule: return tr::lng_nagram_action_reschedule(tr::now);
	case MenuAction::TodoAdd: return tr::lng_nagram_action_todo_add(tr::now);
	case MenuAction::ViewReplies:
		return tr::lng_nagram_action_view_replies(tr::now);
	case MenuAction::Factcheck: return tr::lng_nagram_action_factcheck(tr::now);
	case MenuAction::GoToMessage:
		return tr::lng_nagram_action_go_to_message(tr::now);
	case MenuAction::PollRetract:
		return tr::lng_nagram_action_poll_retract(tr::now);
	case MenuAction::PollStop: return tr::lng_nagram_action_poll_stop(tr::now);
	case MenuAction::SaveRingtone:
		return tr::lng_nagram_action_save_ringtone(tr::now);
	case MenuAction::MessageAuthor:
		return tr::lng_nagram_action_message_author(tr::now);
	case MenuAction::SendGift: return tr::lng_nagram_action_send_gift(tr::now);
	case MenuAction::Count: Unexpected("Menu action count.");
	}
	Unexpected("Menu action.");
}

bool MenuActionDefaultHidden(MenuAction action) {
	return ranges::contains(kDefaultHidden, action);
}

QJsonArray DefaultHiddenIds() {
	auto result = QJsonArray();
	for (const auto action : kDefaultHidden) {
		result.push_back(MenuActionId(action));
	}
	return result;
}

QJsonObject MenuToolsDefaults() {
	return {
		{ u"version"_q, 2 },
		{ u"order"_q, QJsonArray() },
		{ u"hidden"_q, DefaultHiddenIds() },
		{ u"revealWithModifier"_q, QJsonArray() },
	};
}

bool ValidMenuTools(const QJsonObject &value) {
	if (value.keys() != MenuToolsDefaults().keys()
		|| value.value(u"version"_q) != QJsonValue(2)) {
		return false;
	}
	auto hidden = base::flat_set<QString>();
	for (const auto &key : { u"order"_q, u"hidden"_q, u"revealWithModifier"_q }) {
		if (!value.value(key).isArray()) {
			return false;
		}
		auto seen = base::flat_set<QString>();
		for (const auto &entry : value.value(key).toArray()) {
			const auto id = entry.toString();
			if (!entry.isString()
				|| !ranges::any_of(kIds, [&](const char *known) { return id == QLatin1String(known); })
				|| !seen.emplace(id).second) {
				return false;
			}
			if (key == u"hidden"_q) {
				hidden.emplace(id);
			} else if (key == u"revealWithModifier"_q
				&& !hidden.contains(id)) {
				return false;
			}
		}
	}
	return true;
}

QJsonObject MigrateFromV1(Core::Settings &settings, const QJsonObject &v1) {
	// v1 stored only order[] + revealWithModifier[]; the hidden state lived in
	// the nine HideMenu* options. Fold both sources into a v2 hidden[] set and
	// merge with the default-hidden enhancements. Read-only: never writes back.
	auto hidden = base::flat_set<QString>();
	for (const auto &definition : kOptions) {
		const auto action = HiddenAction(definition.option);
		if (action && Get(settings, definition.option)) {
			hidden.emplace(MenuActionId(*action));
		}
	}
	for (const auto action : kDefaultHidden) {
		hidden.emplace(MenuActionId(action));
	}
	auto hiddenArray = QJsonArray();
	for (const auto &id : hidden) {
		hiddenArray.push_back(id);
	}
	auto reveal = QJsonArray();
	for (const auto &entry : v1.value(u"revealWithModifier"_q).toArray()) {
		if (hidden.contains(entry.toString())) {
			reveal.push_back(entry);
		}
	}
	return {
		{ u"version"_q, 2 },
		{ u"order"_q, v1.value(u"order"_q).toArray() },
		{ u"hidden"_q, hiddenArray },
		{ u"revealWithModifier"_q, reveal },
	};
}

std::optional<QJsonObject> MenuTools(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kMenuToolsKey);
	if (bytes.isEmpty()) {
		return MenuToolsDefaults();
	}
	const auto document = QJsonDocument::fromJson(bytes);
	if (!document.isObject()) {
		LOG(("Nagram Error: Invalid messageMenu configuration; menu customization not applied."));
		return std::nullopt;
	}
	const auto object = document.object();
	if (object.value(u"version"_q) == QJsonValue(1)) {
		auto migrated = MigrateFromV1(settings, object);
		if (!ValidMenuTools(migrated)) {
			LOG(("Nagram Error: Invalid messageMenu configuration; menu customization not applied."));
			return std::nullopt;
		}
		return migrated;
	}
	if (!ValidMenuTools(object)) {
		LOG(("Nagram Error: Invalid messageMenu configuration; menu customization not applied."));
		return std::nullopt;
	}
	return object;
}

void SetMenuTools(Core::Settings &settings, const QJsonObject &value) {
	Expects(ValidMenuTools(value));
	if (value == MenuToolsDefaults()) {
		settings.clearPref(kMenuToolsKey);
	} else {
		settings.writePref<QByteArray>(kMenuToolsKey,
			QJsonDocument(value).toJson(QJsonDocument::Compact));
	}
}

bool MenuHidden(Core::Settings &settings, MenuAction action) {
	return MenuHidden(settings, action, QGuiApplication::keyboardModifiers());
}

bool MenuHidden(
		Core::Settings &settings,
		MenuAction action,
		Qt::KeyboardModifiers modifiers) {
	const auto tools = MenuTools(settings);
	if (!tools) {
		return false;
	}
	const auto id = MenuActionId(action);
	if (!tools->value(u"hidden"_q).toArray().contains(id)) {
		return false;
	} else if (!(modifiers & Qt::AltModifier)) {
		return true;
	}
	return !tools->value(u"revealWithModifier"_q).toArray().contains(id);
}

bool MenuHidden(Core::Settings &settings, Option option) {
	return MenuHidden(settings, option, QGuiApplication::keyboardModifiers());
}

bool MenuHidden(
		Core::Settings &settings,
		Option option,
		Qt::KeyboardModifiers modifiers) {
	// Thin shim: legacy Option-based call sites map to their MenuAction and
	// query the unified v2 hidden[] set.
	const auto action = HiddenAction(option);
	if (!action) {
		return false;
	}
	return MenuHidden(settings, *action, modifiers);
}

not_null<QAction*> AddOrderedMenuAction(
		not_null<Ui::PopupMenu*> menu,
		MenuAction id,
		const QString &text,
		Fn<void()> callback,
		const style::icon *icon,
		const style::icon *iconOver) {
	const auto action = Ui::Menu::CreateAction(menu, text, std::move(callback));
	return AddOrderedMenuAction(menu, id, base::make_unique_q<Ui::Menu::Action>(
		menu->menu(), menu->menu()->st(), action, icon, iconOver ? iconOver : icon));
}

not_null<QAction*> AddOrderedMenuAction(
		not_null<Ui::PopupMenu*> menu,
		MenuAction id,
		base::unique_qptr<Ui::Menu::ItemBase> item) {
	const auto actionId = MenuActionId(id);
	item->action()->setProperty("nagramMenuAction", actionId);
	const auto tools = MenuTools(Core::App().settings());
	const auto order = tools ? tools->value(u"order"_q).toArray() : QJsonArray();
	const auto rank = [&](const QString &id) {
		const auto i = ranges::find(order, QJsonValue(id));
		return int(i - order.begin());
	};
	const auto actions = menu->actions();
	auto position = int(actions.size());
	for (auto i = 0; i != actions.size(); ++i) {
		const auto other = actions[i]->property("nagramMenuAction").toString();
		if (!other.isEmpty() && rank(other) > rank(actionId)) {
			position = i;
			break;
		}
	}
	if (position < actions.size() && actions.back()->isSeparator()) {
		menu->menu()->clearLastSeparator();
	}
	return menu->insertAction(position, std::move(item));
}

} // namespace Nagram
