#include "nagram/nagram_main_menu.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Nagram {
namespace {

constexpr auto kIds = std::array{
	"profile", "bots", "newGroup", "newChannel", "contacts", "calls",
	"savedMessages", "settings", "nightMode",
};

} // namespace

QString MainMenuActionTitle(const QString &id) {
	if (id == u"profile"_q) return tr::lng_nagram_main_menu_action_profile(tr::now);
	if (id == u"bots"_q) return tr::lng_nagram_main_menu_bots(tr::now);
	if (id == u"newGroup"_q) return tr::lng_nagram_main_menu_action_new_group(tr::now);
	if (id == u"newChannel"_q) return tr::lng_nagram_main_menu_action_new_channel(tr::now);
	if (id == u"contacts"_q) return tr::lng_nagram_main_menu_action_contacts(tr::now);
	if (id == u"calls"_q) return tr::lng_nagram_main_menu_action_calls(tr::now);
	if (id == u"savedMessages"_q) return tr::lng_nagram_main_menu_action_saved_messages(tr::now);
	if (id == u"settings"_q) return tr::lng_nagram_main_menu_action_settings(tr::now);
	return tr::lng_nagram_main_menu_action_night_mode(tr::now);
}

QJsonObject MainMenuDefaults() {
	return {
		{ u"version"_q, 1 },
		{ u"order"_q, QJsonArray() },
		{ u"hidden"_q, QJsonArray() },
		{ u"title"_q, QString() },
		{ u"seasonalDecorations"_q, true },
	};
}

bool ValidMainMenu(const QJsonObject &value) {
	if (value.keys() != MainMenuDefaults().keys()
		|| value.value(u"version"_q) != QJsonValue(1)
		|| !value.value(u"title"_q).isString()
		|| !value.value(u"seasonalDecorations"_q).isBool()) {
		return false;
	}
	const auto title = value.value(u"title"_q).toString();
	if (title.size() > 96 || QString::fromUtf8(title.toUtf8()) != title
		|| ranges::any_of(title, [](QChar ch) {
			return ch.category() == QChar::Other_Control
				|| ch.category() == QChar::Separator_Line
				|| ch.category() == QChar::Separator_Paragraph;
		})) {
		return false;
	}
	for (const auto &key : { u"order"_q, u"hidden"_q }) {
		if (!value.value(key).isArray()) {
			return false;
		}
		auto seen = QSet<QString>();
		for (const auto &item : value.value(key).toArray()) {
			const auto id = item.toString();
			if (!item.isString() || seen.contains(id)
				|| (key == u"hidden"_q && id == u"settings"_q)
				|| !ranges::any_of(kIds, [&](const char *known) {
					return id == QLatin1String(known);
				})) {
				return false;
			}
			seen.insert(id);
		}
	}
	return true;
}

std::optional<QJsonObject> MainMenu(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kMainMenuKey);
	if (bytes.isEmpty()) {
		return MainMenuDefaults();
	}
	const auto document = QJsonDocument::fromJson(bytes);
	if (!document.isObject() || !ValidMainMenu(document.object())) {
		LOG(("Nagram Error: Invalid mainMenu configuration; native menu retained."));
		return std::nullopt;
	}
	return document.object();
}

void SetMainMenu(Core::Settings &settings, const QJsonObject &value) {
	Expects(ValidMainMenu(value));
	if (value == MainMenuDefaults()) {
		settings.clearPref(kMainMenuKey);
	} else {
		settings.writePref<QByteArray>(
			kMainMenuKey,
			QJsonDocument(value).toJson(QJsonDocument::Compact));
	}
}

QString MainMenuTitle(Core::Settings &settings) {
	const auto value = MainMenu(settings);
	return value ? value->value(u"title"_q).toString() : QString();
}

bool MainMenuSeasonal(Core::Settings &settings) {
	const auto value = MainMenu(settings);
	return !value || value->value(u"seasonalDecorations"_q).toBool();
}

bool MainMenuCustomOrder(Core::Settings &settings) {
	const auto value = MainMenu(settings);
	return value && (!value->value(u"order"_q).toArray().isEmpty()
		|| !value->value(u"hidden"_q).toArray().isEmpty());
}

not_null<Ui::VerticalLayout*> AddMainMenuGroup(
		not_null<Ui::VerticalLayout*> menu,
		const QString &id) {
	const auto value = MainMenu(Core::App().settings());
	const auto config = value.value_or(MainMenuDefaults());
	const auto group = menu->add(object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
		menu,
		object_ptr<Ui::VerticalLayout>(menu)));
	group->setProperty("nagramMainMenuAction", id);
	group->toggle(
		!config.value(u"hidden"_q).toArray().contains(id),
		anim::type::instant);
	const auto order = config.value(u"order"_q).toArray();
	const auto rank = [&](const QString &key) {
		return int(ranges::find(order, QJsonValue(key)) - order.begin());
	};
	for (auto i = 0; i + 1 < menu->count(); ++i) {
		const auto other = menu->widgetAt(i)->property(
			"nagramMainMenuAction").toString();
		if (!other.isEmpty() && rank(other) > rank(id)) {
			menu->reorderRows(menu->count() - 1, i);
			break;
		}
	}
	return group->entity();
}

void MainMenuBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_main_menu());

	const auto current = MainMenu(Core::App().settings());
	if (!current) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box, tr::lng_nagram_main_menu_invalid(), st::boxLabel));
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		return;
	}
	box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_nagram_main_menu_about(), st::boxLabel));
	const auto title = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		Ui::InputField::Mode::SingleLine,
		tr::lng_nagram_main_menu_title(),
		current->value(u"title"_q).toString()));
	title->setMaxLength(96);
	const auto seasonal = box->addRow(object_ptr<Ui::Checkbox>(
		box,
		tr::lng_nagram_main_menu_seasonal(tr::now),
		current->value(u"seasonalDecorations"_q).toBool()));
	struct State {
		QJsonArray order;
		QJsonArray hidden;
		Fn<void()> refresh;
	};
	const auto state = box->lifetime().make_state<State>();
	state->order = current->value(u"order"_q).toArray();
	state->hidden = current->value(u"hidden"_q).toArray();
	for (const auto id : kIds) {
		const auto text = QString::fromLatin1(id);
		if (!state->order.contains(text)) {
			state->order.push_back(text);
		}
	}
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	state->refresh = [=] {
		rows->clear();
		for (auto index = 0; index != state->order.size(); ++index) {
			const auto id = state->order[index].toString();
			const auto hidden = state->hidden.contains(id);
			const auto row = rows->add(object_ptr<Ui::SettingsButton>(
				rows,
				rpl::single(MainMenuActionTitle(id) + (hidden
					? u" · "_q + tr::lng_nagram_main_menu_hidden(tr::now)
					: QString())),
				st::settingsButtonNoIcon));
			row->setClickedCallback([=] {
				const auto menu = Ui::CreateChild<Ui::PopupMenu>(box);

				if (id != u"settings"_q) {
					menu->addAction(hidden
						? tr::lng_nagram_main_menu_show(tr::now)
						: tr::lng_nagram_main_menu_hide(tr::now), [=] {
						if (hidden) {
							state->hidden.removeAt(int(ranges::find(
								state->hidden, QJsonValue(id)) - state->hidden.begin()));
						} else {
							state->hidden.push_back(id);
						}
						InvokeQueued(box, state->refresh);
					});
				}
				for (const auto delta : { -1, 1 }) {
					if (index + delta < 0 || index + delta >= state->order.size()) {
						continue;
					}
					menu->addAction(delta < 0
						? tr::lng_link_move_up(tr::now)
						: tr::lng_link_move_down(tr::now), [=] {
						state->order.removeAt(index);
						state->order.insert(index + delta, id);
						InvokeQueued(box, state->refresh);
					});
				}
				menu->popup(QCursor::pos());
			});
		}
	};
	box->addButton(tr::lng_settings_save(), [=] {
		if (MainMenu(Core::App().settings()) != current) {
			box->showToast(tr::lng_nagram_config_changed_error(tr::now));
			return;
		}
		auto result = *current;
		result.insert(u"title"_q, title->getLastText().trimmed());
		result.insert(u"seasonalDecorations"_q, seasonal->checked());
		auto natural = QJsonArray();
		for (const auto id : kIds) {
			natural.push_back(QLatin1String(id));
		}
		result.insert(u"order"_q, state->order == natural
			? QJsonArray() : state->order);
		result.insert(u"hidden"_q, state->hidden);
		if (!ValidMainMenu(result)) {
			box->showToast(tr::lng_nagram_main_menu_invalid(tr::now));
			return;
		}
		SetMainMenu(Core::App().settings(), result);
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	state->refresh();
}

} // namespace Nagram
