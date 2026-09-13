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

void MenuBox(not_null<Ui::GenericBox*> box, QJsonObject current) {
	box->setTitle(tr::lng_nagram_menu_tools());

	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_menu_order_about(),
		st::boxLabel));
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto reorder = box->lifetime().make_state<Ui::VerticalLayoutReorder>(rows);
	const auto popup = box->lifetime().make_state<base::unique_qptr<Ui::PopupMenu>>();
	auto ordered = current.value(u"order"_q).toArray();
	for (auto i = 0; i != int(MenuAction::Count); ++i) {
		const auto id = MenuActionId(MenuAction(i));
		if (!ordered.contains(id)) {
			ordered.push_back(id);
		}
	}
	const auto collect = [=] {
		auto result = QJsonArray();
		for (auto i = 0; i != rows->count(); ++i) {
			result.push_back(rows->widgetAt(i)->property("nagramMenuAction").toString());
		}
		return result;
	};
	for (const auto &value : ordered) {
		const auto id = value.toString();
		auto action = MenuAction::Reply;
		for (auto i = 0; i != int(MenuAction::Count); ++i) {
			if (MenuActionId(MenuAction(i)) == id) {
				action = MenuAction(i);
				break;
			}
		}
		const auto row = rows->add(object_ptr<Ui::SettingsButton>(
			rows,
			rpl::single(MenuActionTitle(action)),
			st::settingsButtonNoIcon));
		row->setProperty("nagramMenuAction", id);
		row->setClickedCallback([=] {
			*popup = base::make_unique_q<Ui::PopupMenu>(box);

			const auto values = collect();
			const auto index = int(ranges::find(values, QJsonValue(id)) - values.begin());
			const auto move = [=](int to) {
				reorder->cancel();
				rows->reorderRows(index, to);

				reorder->start();
			};
			if (index > 0) {
				(*popup)->addAction(tr::lng_link_move_up(tr::now), [=] { move(index - 1); });
			}
			if (index + 1 < rows->count()) {
				(*popup)->addAction(tr::lng_link_move_down(tr::now), [=] { move(index + 1); });
			}
			(*popup)->popup(row->mapToGlobal(row->rect().bottomLeft()));
		});
	}
	reorder->start();
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_nagram_menu_modifier_about(),
		st::boxLabel));
	const auto reveals = box->lifetime().make_state<QJsonArray>(
		current.value(u"revealWithModifier"_q).toArray());
	for (const auto action : {
			MenuAction::Pin, MenuAction::Report, MenuAction::Block,
			MenuAction::Statistics, MenuAction::CopyLink, MenuAction::Forward,
			MenuAction::Translate, MenuAction::Select, MenuAction::EmojiPacks }) {
		const auto id = MenuActionId(action);
		const auto row = box->addRow(object_ptr<Ui::SettingsButton>(
			box,
			rpl::single(MenuActionTitle(action)),
			st::settingsButtonNoIcon));
		row->toggleOn(rpl::single(reveals->contains(id)));
		row->toggledChanges() | rpl::on_next([=](bool enabled) {
			const auto index = ranges::find(*reveals, QJsonValue(id));
			if (enabled && index == reveals->end()) {
				reveals->push_back(id);
			} else if (!enabled && index != reveals->end()) {
				reveals->erase(index);
			}
		}, row->lifetime());
	}
	box->addButton(tr::lng_settings_save(), [=] {
		reorder->finishReordering();
		auto updated = current;
		const auto order = collect();
		if (order != ordered) {
			updated.insert(u"order"_q, order);
		}
		updated.insert(u"revealWithModifier"_q, *reveals);
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
