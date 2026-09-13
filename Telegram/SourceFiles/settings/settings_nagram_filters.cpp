#include "settings/settings_nagram_filters.h"

#include "base/unique_qptr.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "nagram/nagram_filters.h"
#include "settings/settings_builder.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QUuid>
#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

using namespace Nagram;

QJsonObject ReadFilters(not_null<Main::Session*> session) {
	const auto &bytes = session->settings().nagramFilters();
	return bytes.isEmpty() ? FilterDefaults()
		: QJsonDocument::fromJson(bytes).object();
}

bool Save(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		const QJsonObject &expected,
		const QJsonObject &value) {
	if (ReadFilters(session) != expected) {
		box->showToast(tr::lng_nagram_config_changed_error(tr::now));
		return false;
	}
	if (const auto error = ValidateFilters(value); !error.isEmpty()) {
		box->showToast(error);
		return false;
	}
	SetFilters(session, value);
	return true;
}

void PreviewBox(not_null<Ui::GenericBox*> box, QJsonObject config) {
	box->setTitle(tr::lng_nagram_filter_preview());

	const auto input = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField, Ui::InputField::Mode::MultiLine));
	input->setMaxLength(16384);
	const auto result = box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_nagram_filter_preview_about(), st::boxLabel));
	result->setSelectable(true);
	const auto busy = box->lifetime().make_state<bool>(false);
	box->addButton(tr::lng_nagram_filter_preview(), [=] {
		if (*busy) {
			return;
		}
		if (const auto error = ValidateFilters(config); !error.isEmpty()) {
			box->showToast(error);
			return;
		}
		*busy = true;
		result->setText(tr::lng_nagram_filter_pending(tr::now));
		auto active = config;
		active.insert(u"enabled"_q, true);
		const auto text = TextWithEntities{ input->getLastText() };
		const auto done = crl::guard(box, [=](FilterResult value) {
			*busy = false;
			const auto state = FilterState{ .ready = true, .result = std::move(value) };
			result->setText(state.result.hidden
				? FilterPlaceholder(state) : state.result.projection.text.text);
		});
		crl::async([active, text, done] {
			auto value = ApplyFilters(active, text, QString(), false, false);
			crl::on_main([done, value = std::move(value)]() mutable {
				done(std::move(value));
			});
		});
	});
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void RuleBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Session*> session,
		QJsonObject current,
		int index,
		Fn<void(QJsonObject)> changed) {
	box->setTitle(tr::lng_nagram_filter_rule());

	const auto rules = current.value(u"rules"_q).toArray();
	const auto original = index < rules.size() ? rules[index].toObject() : QJsonObject{
		{ u"id"_q, QUuid::createUuid().toString(QUuid::WithoutBraces) },
		{ u"title"_q, QString() },
		{ u"pattern"_q, QString() },
		{ u"enabled"_q, false },
		{ u"caseInsensitive"_q, false },
		{ u"reversed"_q, false },
		{ u"action"_q, u"mask"_q },
		{ u"replacement"_q, QString() },
	};
	const auto field = [&](rpl::producer<QString> label, QString key, int maximum) {
		box->addRow(object_ptr<Ui::FlatLabel>(box, std::move(label), st::boxLabel));
		const auto input = box->addRow(object_ptr<Ui::InputField>(
			box, st::defaultInputField, Ui::InputField::Mode::SingleLine,
			rpl::single(QString()), original.value(key).toString()));
		input->setMaxLength(maximum);
		return input;
	};
	const auto title = field(tr::lng_nagram_filter_title(), u"title"_q, 128);
	const auto pattern = field(tr::lng_nagram_filter_pattern(), u"pattern"_q, 2048);
	const auto replacement = field(tr::lng_nagram_filter_replacement(), u"replacement"_q, 4096);
	const auto flags = std::array{
		std::pair(u"enabled"_q, tr::lng_nagram_filter_rule_enabled(tr::now)),
		std::pair(u"caseInsensitive"_q, tr::lng_nagram_filter_case(tr::now)),
		std::pair(u"reversed"_q, tr::lng_nagram_filter_reverse(tr::now)),
	};
	auto toggles = std::vector<Ui::Checkbox*>();
	for (const auto &[key, label] : flags) {
		toggles.push_back(box->addRow(object_ptr<Ui::Checkbox>(
			box, label, original.value(key).toBool())));
	}
	const auto actions = QStringList{ u"mask"_q, u"replace"_q, u"hide"_q };
	const auto group = std::make_shared<Ui::RadiobuttonGroup>(
		actions.indexOf(original.value(u"action"_q) == u"maskMessage"_q
			? u"hide"_q : original.value(u"action"_q).toString()));
	const auto labels = std::array{
		tr::lng_nagram_filter_mask(tr::now), tr::lng_nagram_filter_replace(tr::now),
		tr::lng_nagram_filter_hide(tr::now),
	};
	for (auto i = 0; i != labels.size(); ++i) {
		box->addRow(object_ptr<Ui::Radiobutton>(box, group, i, labels[i]));
	}
	const auto collect = [=] {
		auto rule = original;
		rule.insert(u"title"_q, title->getLastText().trimmed());
		rule.insert(u"pattern"_q, pattern->getLastText());
		rule.insert(u"replacement"_q, replacement->getLastText());
		for (auto i = 0; i != flags.size(); ++i) {
			rule.insert(flags[i].first, toggles[i]->checked());
		}
		rule.insert(u"action"_q, actions[group->current()]);
		auto updated = current;
		auto list = rules;
		if (index < list.size()) {
			list[index] = rule;
		} else {
			list.push_back(rule);
		}
		updated.insert(u"rules"_q, list);
		return updated;
	};
	box->addButton(tr::lng_settings_save(), [=] {
		const auto updated = collect();
		if (Save(box, session, current, updated)) {
			changed(ReadFilters(session));
			box->closeBox();
		}
	});
	box->addButton(tr::lng_nagram_filter_preview(), [=] {
		auto updated = collect();
		auto rule = updated.value(u"rules"_q).toArray()[index].toObject();
		rule.insert(u"enabled"_q, true);
		updated.insert(u"rules"_q, QJsonArray{ rule });
		box->uiShow()->showBox(Box(PreviewBox, updated));
	});
	if (index < rules.size()) {
		box->addButton(tr::lng_box_delete(), [=] {
			auto updated = current;
			auto list = rules;
			list.removeAt(index);
			updated.insert(u"rules"_q, list);
			if (Save(box, session, current, updated)) {
				changed(ReadFilters(session));
				box->closeBox();
			}
		});
	}
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void FiltersBox(not_null<Ui::GenericBox*> box, not_null<Main::Session*> session) {
	box->setTitle(tr::lng_nagram_filters());

	const auto initial = ReadFilters(session);
	if (const auto error = ValidateFilters(initial); !error.isEmpty()) {
		box->addRow(object_ptr<Ui::FlatLabel>(box, rpl::single(error), st::boxLabel));
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		return;
	}
	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_nagram_filters_about(), st::boxLabel));
	const auto state = box->lifetime().make_state<rpl::variable<QJsonObject>>(initial);
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto popup = box->lifetime().make_state<base::unique_qptr<Ui::PopupMenu>>();
	const auto changed = crl::guard(box, [=](QJsonObject value) {
		crl::on_main(box, [=] { *state = value; });
	});
	state->value() | rpl::on_next([=](const QJsonObject &current) {
		rows->clear();
		const auto add = [&](QString text, Fn<void()> click) {
			const auto row = rows->add(object_ptr<Ui::SettingsButton>(
				rows, rpl::single(std::move(text)), st::settingsButtonNoIcon));
			row->setClickedCallback(std::move(click));
			return row;
		};
		for (const auto &[key, label] : std::array{
			std::pair(u"enabled"_q, tr::lng_nagram_filter_enabled(tr::now)),
			std::pair(u"filterOutgoing"_q, tr::lng_nagram_filter_outgoing(tr::now)),
			std::pair(u"hideBlocked"_q, tr::lng_nagram_filter_blocked(tr::now)),
			std::pair(u"stripZalgo"_q, tr::lng_nagram_filter_zalgo(tr::now)),
		}) {
			const auto row = add(label, nullptr);
			row->toggleOn(rpl::single(current.value(key).toBool()));
			row->toggledChanges() | rpl::on_next([=](bool value) {
				auto updated = current;
				updated.insert(key, value);
				if (Save(box, session, current, updated)) {
					changed(ReadFilters(session));
				}
			}, row->lifetime());
		}
		const auto rules = current.value(u"rules"_q).toArray();
		for (auto i = 0; i < rules.size(); ++i) {
			const auto rule = rules[i].toObject();
			add(QString::number(i + 1) + u". "_q + rule.value(u"title"_q).toString(), [=] {
				*popup = base::make_unique_q<Ui::PopupMenu>(box);
				(*popup)->addAction(tr::lng_nagram_filter_rule(tr::now), [=] {
					box->uiShow()->showBox(Box(RuleBox, session, current, i, changed));
				});
				for (const auto delta : { -1, 1 }) {
					if (i + delta < 0 || i + delta >= rules.size()) {
						continue;
					}
					(*popup)->addAction(delta < 0 ? tr::lng_link_move_up(tr::now)
						: tr::lng_link_move_down(tr::now), [=] {
						auto list = rules;
						list.removeAt(i);
						list.insert(i + delta, rule);
						auto updated = current;
						updated.insert(u"rules"_q, list);
						if (Save(box, session, current, updated)) {
							changed(ReadFilters(session));
						}
					});
				}
				(*popup)->popup(QCursor::pos());
			});
		}
		add(tr::lng_nagram_filter_add(tr::now), [=] {
			box->uiShow()->showBox(Box(RuleBox, session, current, rules.size(), changed));
		});
		add(tr::lng_nagram_filter_preview(tr::now), [=] {
			box->uiShow()->showBox(Box(PreviewBox, current));
		});
		add(tr::lng_nagram_filter_export(tr::now), [=] {
			auto list = rules;
			for (auto i = 0; i < list.size(); ++i) {
				auto rule = list[i].toObject();
				rule.insert(u"enabled"_q, false);
				list[i] = rule;
			}
			const auto data = QJsonObject{ { u"version"_q, 1 }, { u"rules"_q, list } };
			QApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(data).toJson()));
			box->showToast(tr::lng_nagram_filter_exported(tr::now));
		});
		add(tr::lng_nagram_filter_import(tr::now), [=] {
			const auto text = QApplication::clipboard()->text();
			if (text.size() > 128 * 1024) {
				box->showToast(tr::lng_nagram_filter_invalid(tr::now));
				return;
			}
			const auto data = QJsonDocument::fromJson(text.toUtf8()).object();
			if (data.size() != 2
				|| data.value(u"version"_q) != 1 || !data.value(u"rules"_q).isArray()) {
				box->showToast(tr::lng_nagram_filter_invalid(tr::now));
				return;
			}
			auto imported = data.value(u"rules"_q).toArray();
			auto validated = FilterDefaults();
			validated.insert(u"rules"_q, imported);
			if (const auto error = ValidateFilters(validated); !error.isEmpty()) {
				box->showToast(error);
				return;
			}
			for (auto i = 0; i < imported.size(); ++i) {
				auto rule = imported[i].toObject();
				rule.insert(u"id"_q, QUuid::createUuid().toString(QUuid::WithoutBraces));
				rule.insert(u"enabled"_q, false);
				imported[i] = rule;
			}
			auto updated = current;
			auto list = rules;
			for (const auto &rule : imported) {
				list.push_back(rule);
			}
			updated.insert(u"rules"_q, list);
			if (const auto error = ValidateFilters(updated); !error.isEmpty()) {
				box->showToast(error);
				return;
			}
			auto names = QStringList();
			for (const auto &rule : imported) {
				names.push_back(rule.toObject().value(u"title"_q).toString());
			}
			box->uiShow()->showBox(Ui::MakeConfirmBox({
				.text = tr::lng_nagram_filter_import_about(tr::now) + u"\n\n"_q + names.join('\n'),
				.confirmed = crl::guard(box, [=](Fn<void()> close) {
					if (Save(box, session, current, updated)) {
						changed(ReadFilters(session));
						close();
					}
				}),
			}));
		});
	}, box->lifetime());
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace

void BuildNagramFilters(Builder::SectionBuilder &builder) {
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"nagram.filters"_q,
		.title = tr::lng_nagram_filters(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { controller->show(Box(FiltersBox, &controller->session())); },
		.keywords = { u"Nagram"_q, u"regex"_q },
	});
}

void AddNagramFilterMenu(
		not_null<Ui::PopupMenu*> menu,
		not_null<HistoryItem*> item,
		not_null<Window::SessionController*> controller) {
	const auto session = &controller->session();
	const auto current = ReadFilters(session);
	if (!ValidateFilters(current).isEmpty()) {
		return;
	}
	for (const auto &key : { u"hiddenAuthors"_q, u"excludedPeers"_q }) {
		const auto author = key == u"hiddenAuthors"_q;
		const auto id = QString::number(SerializePeerId(
			author ? item->from()->id : item->history()->peer->id));
		const auto contains = current.value(key).toArray().contains(id);
		if (author && item->from() == item->history()->peer && !contains) {
			continue;
		}
		const auto label = author
			? (contains ? tr::lng_nagram_filter_author_show(tr::now)
				: tr::lng_nagram_filter_author_hide(tr::now))
			: (contains ? tr::lng_nagram_filter_chat_enable(tr::now)
				: tr::lng_nagram_filter_chat_disable(tr::now));
		menu->addAction(label, crl::guard(controller, [=] {
			auto updated = ReadFilters(session);
			if (!ValidateFilters(updated).isEmpty()) {
				controller->showToast(tr::lng_nagram_filter_invalid(tr::now));
				return;
			}
			auto list = updated.value(key).toArray();
			if (const auto index = list.toVariantList().indexOf(id); index >= 0) {
				list.removeAt(index);
			} else {
				list.push_back(id);
			}
			updated.insert(key, list);
			if (const auto error = ValidateFilters(updated); !error.isEmpty()) {
				controller->showToast(error);
				return;
			}
			SetFilters(session, updated);
			controller->showToast(tr::lng_nagram_filter_menu_saved(tr::now));
		}));
	}
}

} // namespace Settings
