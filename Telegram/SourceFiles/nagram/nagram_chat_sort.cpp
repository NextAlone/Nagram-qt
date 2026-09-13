#include "nagram/nagram_chat_sort.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "dialogs/dialogs_entry.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Nagram {
namespace {

constexpr auto kIds = std::array{ "unread", "unmuted", "users", "contacts" };

} // namespace

QString ChatSortRuleTitle(const QString &id) {
	return id == u"unread"_q
		? tr::lng_nagram_sort_unread(tr::now)
		: id == u"unmuted"_q
		? tr::lng_nagram_sort_unmuted(tr::now)
		: id == u"users"_q
		? tr::lng_nagram_sort_users(tr::now)
		: tr::lng_nagram_sort_contacts(tr::now);
}

QJsonObject ChatSortDefaults() {
	return { { u"version"_q, 1 }, { u"order"_q, QJsonArray() } };
}

bool ValidChatSort(const QJsonObject &value) {
	if (value.keys() != ChatSortDefaults().keys()
		|| value.value(u"version"_q) != QJsonValue(1)
		|| !value.value(u"order"_q).isArray()) {
		return false;
	}
	const auto order = value.value(u"order"_q).toArray();
	auto seen = QSet<QString>();
	for (const auto &rule : order) {
		const auto id = rule.toString();
		if (!rule.isString() || seen.contains(id)
			|| !ranges::any_of(kIds, [&](const char *known) {
				return id == QLatin1String(known);
			})) {
			return false;
		}
		seen.insert(id);
	}
	return true;
}

std::optional<QJsonObject> ChatSort(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kChatSortKey);
	if (bytes.isEmpty()) {
		return ChatSortDefaults();
	}
	const auto document = QJsonDocument::fromJson(bytes);
	if (!document.isObject() || !ValidChatSort(document.object())) {
		LOG(("Nagram Error: Invalid chatSort configuration; native order retained."));
		return std::nullopt;
	}
	return document.object();
}

void SetChatSort(Core::Settings &settings, const QJsonObject &value) {
	Expects(ValidChatSort(value));
	if (value == ChatSortDefaults()) {
		settings.clearPref(kChatSortKey);
	} else {
		settings.writePref<QByteArray>(
			kChatSortKey,
			QJsonDocument(value).toJson(QJsonDocument::Compact));
	}
}

std::vector<QString> ChatSortOrder(Core::Settings &settings) {
	auto result = std::vector<QString>();
	if (const auto config = ChatSort(settings)) {
		for (const auto &id : config->value(u"order"_q).toArray()) {
			result.push_back(id.toString());
		}
	}
	return result;
}

uint8 ChatSortPriority(not_null<Dialogs::Entry*> entry) {
	const auto &order = entry->owner().nagramChatSort();
	const auto history = entry->asHistory();
	if (order.empty() || !history) {
		return 0;
	}
	const auto unread = history->chatListUnreadState();
	const auto user = history->peer->asUser();
	auto rank = uint8(0);
	for (const auto &id : order) {
		const auto matches = id == u"unread"_q
			? (unread.messages || unread.marks || unread.mentions
				|| unread.reactions)
			: id == u"unmuted"_q
			? !history->muted()
			: id == u"users"_q
			? (user != nullptr)
			: (user && user->isContact());
		rank = (rank << 1) | (matches ? 1 : 0);
	}
	return rank + 1;
}

void ChatSortBox(not_null<Ui::GenericBox*> box) {
	box->setTitle(tr::lng_nagram_chat_sort());

	const auto current = ChatSort(Core::App().settings());
	if (!current) {
		box->addRow(object_ptr<Ui::FlatLabel>(
			box, tr::lng_nagram_sort_invalid(), st::boxLabel));
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		return;
	}
	box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_nagram_sort_about(), st::boxLabel));
	const auto order = box->lifetime().make_state<QJsonArray>(
		current->value(u"order"_q).toArray());
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto refresh = box->lifetime().make_state<Fn<void()>>();
	*refresh = [=] {
		rows->clear();
		auto all = *order;
		for (const auto id : kIds) {
			const auto text = QString::fromLatin1(id);
			if (!all.contains(text)) {
				all.push_back(text);
			}
		}
		for (const auto &value : all) {
			const auto id = value.toString();
			const auto index = int(ranges::find(*order, value) - order->begin());
			const auto enabled = index < order->size();
			const auto row = rows->add(object_ptr<Ui::SettingsButton>(
				rows,
				rpl::single(ChatSortRuleTitle(id) + u" · "_q + (enabled
					? QString::number(index + 1)
					: tr::lng_nagram_config_off(tr::now))),
				st::settingsButtonNoIcon));
			row->setClickedCallback([=] {
				const auto menu = Ui::CreateChild<Ui::PopupMenu>(box);

				const auto changed = [=] {
					InvokeQueued(box, *refresh);
				};
				menu->addAction(enabled
					? tr::lng_nagram_sort_disable(tr::now)
					: tr::lng_nagram_sort_enable(tr::now), [=] {
					if (enabled) {
						order->removeAt(index);
					} else {
						order->push_back(id);
					}
					changed();
				});
				for (const auto delta : { -1, 1 }) {
					if (!enabled || index + delta < 0
						|| index + delta >= order->size()) {
						continue;
					}
					menu->addAction(delta < 0
						? tr::lng_link_move_up(tr::now)
						: tr::lng_link_move_down(tr::now), [=] {
						order->removeAt(index);
						order->insert(index + delta, id);
						changed();
					});
				}
				menu->popup(QCursor::pos());
			});
		}
	};
	box->addButton(tr::lng_settings_save(), [=] {
		if (ChatSort(Core::App().settings()) != current) {
			box->showToast(tr::lng_nagram_config_changed_error(tr::now));
			return;
		}
		auto result = *current;
		result.insert(u"order"_q, *order);
		SetChatSort(Core::App().settings(), result);
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	(*refresh)();
}

} // namespace Nagram
