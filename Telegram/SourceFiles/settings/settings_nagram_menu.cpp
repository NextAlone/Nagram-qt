#include "settings/settings_nagram_menu.h"

#include "core/application.h"
#include "lang/lang_keys.h"
#include "nagram/nagram_menu.h"
#include "settings/settings_builder.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/vertical_layout_reorder.h"
#include "window/window_session_controller.h"

#include <QtCore/QJsonArray>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

using namespace Nagram;

[[nodiscard]] MenuAction ActionById(const QString &id) {
	for (auto i = 0; i != int(MenuAction::Count); ++i) {
		if (MenuActionId(MenuAction(i)) == id) {
			return MenuAction(i);
		}
	}
	return MenuAction::Reply;
}

void MenuBox(not_null<Ui::GenericBox*> box, QJsonObject current) {
	box->setTitle(tr::lng_nagram_menu_tools());

	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_menu_order_about(),
		st::boxLabel));

	// Unified list: every action is one draggable row with a show/hide toggle.
	auto ordered = current.value(u"order"_q).toArray();
	for (auto i = 0; i != int(MenuAction::Count); ++i) {
		const auto id = MenuActionId(MenuAction(i));
		if (!ordered.contains(id)) {
			ordered.push_back(id);
		}
	}
	const auto hidden = box->lifetime().make_state<QJsonArray>(
		current.value(u"hidden"_q).toArray());
	const auto reveals = box->lifetime().make_state<QJsonArray>(
		current.value(u"revealWithModifier"_q).toArray());
	const auto contains = [](const QJsonArray &array, const QString &id) {
		return array.contains(id);
	};
	const auto remove = [](QJsonArray &array, const QString &id) {
		const auto i = ranges::find(array, QJsonValue(id));
		if (i != array.end()) {
			array.erase(i);
		}
	};

	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto reorder = box->lifetime().make_state<Ui::VerticalLayoutReorder>(rows);
	const auto collect = [=] {
		auto result = QJsonArray();
		for (auto i = 0; i != rows->count(); ++i) {
			result.push_back(rows->widgetAt(i)->property("nagramMenuAction").toString());
		}
		return result;
	};
	for (const auto &value : ordered) {
		const auto id = value.toString();
		const auto action = ActionById(id);
		const auto row = rows->add(object_ptr<Ui::SettingsButton>(
			rows,
			rpl::single(MenuActionTitle(action)),
			st::settingsButtonNoIcon));
		row->setProperty("nagramMenuAction", id);
		row->toggleOn(rpl::single(!contains(*hidden, id)));
		row->toggledChanges() | rpl::on_next([=](bool shown) {
			if (shown) {
				remove(*hidden, id);
				remove(*reveals, id);
			} else if (!contains(*hidden, id)) {
				hidden->push_back(id);
			}
		}, row->lifetime());
	}
	reorder->start();

	// Modifier reveal: for actions hidden when the box was opened, allow
	// temporarily showing them while Alt/Option is held. Reopen the box to
	// configure reveal for newly hidden actions.
	auto initialHidden = current.value(u"hidden"_q).toArray();
	if (!initialHidden.empty()) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_nagram_menu_modifier_about(),
			st::boxLabel));
		for (const auto &value : initialHidden) {
			const auto id = value.toString();
			const auto action = ActionById(id);
			const auto row = box->addRow(object_ptr<Ui::SettingsButton>(
				box,
				rpl::single(MenuActionTitle(action)),
				st::settingsButtonNoIcon));
			row->toggleOn(rpl::single(contains(*reveals, id)));
			row->toggledChanges() | rpl::on_next([=](bool enabled) {
				if (enabled && !contains(*reveals, id)) {
					reveals->push_back(id);
				} else if (!enabled) {
					remove(*reveals, id);
				}
			}, row->lifetime());
		}
	}

	box->addButton(tr::lng_settings_save(), [=] {
		reorder->finishReordering();
		auto updated = current;
		const auto order = collect();
		if (order != ordered) {
			updated.insert(u"order"_q, order);
		}
		updated.insert(u"hidden"_q, *hidden);
		// reveal ⊆ hidden: drop any reveal entry that is no longer hidden.
		auto reveal = QJsonArray();
		for (const auto &entry : *reveals) {
			if (contains(*hidden, entry.toString())) {
				reveal.push_back(entry);
			}
		}
		updated.insert(u"revealWithModifier"_q, reveal);
		SetMenuTools(Core::App().settings(), updated);
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace

void BuildNagramMenu(Builder::SectionBuilder &builder) {
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"nagram.messageMenu"_q,
		.title = tr::lng_nagram_menu_tools(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			const auto current = MenuTools(Core::App().settings());
			if (!current) {
				controller->show(Ui::MakeInformBox(
					tr::lng_nagram_config_value_error(tr::now)
					+ u"\n\nnagram.messageMenu"_q));
				return;
			}
			controller->show(Box(MenuBox, *current));
		},
		.keywords = { u"Nagram"_q },
	});
}

} // namespace Settings
