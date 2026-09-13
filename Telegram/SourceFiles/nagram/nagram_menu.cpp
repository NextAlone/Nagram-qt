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
};
static_assert(kIds.size() == int(MenuAction::Count));

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
	case MenuAction::Count: Unexpected("Menu action count.");
	}
	Unexpected("Menu action.");
}

QJsonObject MenuToolsDefaults() {
	return {
		{ u"version"_q, 1 },
		{ u"order"_q, QJsonArray() },
		{ u"revealWithModifier"_q, QJsonArray() },
	};
}

bool ValidMenuTools(const QJsonObject &value) {
	if (value.keys() != MenuToolsDefaults().keys()
		|| value.value(u"version"_q) != QJsonValue(1)) {
		return false;
	}
	for (const auto &key : { u"order"_q, u"revealWithModifier"_q }) {
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
			if (key == u"revealWithModifier"_q
				&& !ranges::any_of(kOptions, [&](const OptionDefinition &definition) {
					const auto action = HiddenAction(definition.option);
					return action && MenuActionId(*action) == id;
				})) {
				return false;
			}
		}
	}
	return true;
}

std::optional<QJsonObject> MenuTools(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kMenuToolsKey);
	if (bytes.isEmpty()) {
		return MenuToolsDefaults();
	}
	const auto document = QJsonDocument::fromJson(bytes);
	if (!document.isObject() || !ValidMenuTools(document.object())) {
		LOG(("Nagram Error: Invalid messageMenu configuration; menu customization not applied."));
		return std::nullopt;
	}
	return document.object();
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

bool MenuHidden(Core::Settings &settings, Option option) {
	return MenuHidden(settings, option, QGuiApplication::keyboardModifiers());
}

bool MenuHidden(
		Core::Settings &settings,
		Option option,
		Qt::KeyboardModifiers modifiers) {
	if (!Get(settings, option)) {
		return false;
	} else if (!(modifiers & Qt::AltModifier)) {
		return true;
	}
	const auto action = HiddenAction(option);
	const auto tools = action ? MenuTools(settings) : std::nullopt;
	return !tools
		|| !tools->value(u"revealWithModifier"_q).toArray().contains(MenuActionId(*action));
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
