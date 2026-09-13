#include "nagram/nagram_folders.h"

#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_chat_filters.h"
#include "data/data_folder.h"
#include "data/data_session.h"
#include "dialogs/dialogs_indexed_list.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_row.h"
#include "history/history.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "ui/widgets/popup_menu.h"
#include "ui/layers/show.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace Nagram {

std::optional<ManagedFolders> ParseManagedFolders(
		const QByteArray &serialized) {
	auto result = ManagedFolders();
	if (serialized.isEmpty()) {
		return result;
	} else if (serialized.size() > 16384) {
		return std::nullopt;
	}
	const auto document = QJsonDocument::fromJson(serialized);
	const auto value = document.object();
	if (!document.isObject()
		|| value.keys() != QStringList{ u"folders"_q, u"version"_q }
		|| value.value(u"version"_q) != QJsonValue(1)
		|| !value.value(u"folders"_q).isArray()) {
		return std::nullopt;
	}
	const auto folders = value.value(u"folders"_q).toArray();
	if (folders.size() > 1000) {
		return std::nullopt;
	}
	for (const auto &folder : folders) {
		const auto id = folder.toInt(-1);
		if (!folder.isDouble() || id <= 0 || folder != QJsonValue(id)
			|| result.contains(id)) {
			return std::nullopt;
		}
		result.insert(id);
	}
	return result;
}

QByteArray SerializeManagedFolders(const ManagedFolders &ids) {
	if (ids.empty()) {
		return {};
	}
	auto folders = QJsonArray();
	for (const auto id : ids) {
		folders.push_back(id);
	}
	return QJsonDocument(QJsonObject{
		{ u"version"_q, 1 },
		{ u"folders"_q, folders },
	}).toJson(QJsonDocument::Compact);
}

bool IsManagedPeer(not_null<PeerData*> peer) {
	if (const auto chat = peer->asChat()) {
		return chat->amCreator() || chat->hasAdminRights();
	} else if (const auto channel = peer->asChannel()) {
		return channel->amCreator() || channel->hasAdminRights();
	}
	return false;
}

void AddManagedFolderAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<Main::Session*> session,
		std::shared_ptr<Ui::Show> show,
		FilterId id) {
	Expects(id > 0);
	const auto checked = session->settings().managedFolders().contains(id);
	const auto action = menu->addAction(
		tr::lng_nagram_managed_folder(tr::now),
		crl::guard(session, [=] {
			const auto &filters = session->data().chatsFilters().list();
			if (!ranges::any_of(filters, [=](const Data::ChatFilter &filter) {
					return filter.id() == id;
				})) {
				show->showToast(tr::lng_nagram_managed_folder_missing(tr::now));
				return;
			}
			auto &settings = session->settings();
			if (!settings.managedFoldersValid()
				|| settings.managedFolders().contains(id) != checked) {
				show->showToast(tr::lng_nagram_managed_folder_invalid(tr::now));
				return;
			}
			auto ids = settings.managedFolders();
			if (checked) {
				ids.remove(id);
			} else {
				ids.insert(id);
			}
			if (!settings.setManagedFolders(SerializeManagedFolders(ids))) {
				show->showToast(tr::lng_nagram_managed_folder_invalid(tr::now));
				return;
			}
			session->saveSettingsDelayed();
			auto histories = std::vector<not_null<History*>>();
			const auto collect = [&](not_null<Dialogs::MainList*> list) {
				for (const auto &row : list->indexed()->all()) {
					if (const auto history = row->history()) {
						histories.push_back(history);
					}
				}
			};
			collect(session->data().chatsList());
			if (const auto archive = session->data().folderLoaded(
					Data::Folder::kId)) {
				collect(archive->chatsList());
			}
			for (const auto history : histories) {
				session->data().chatsFilters().refreshHistory(history);
			}
		}));
	action->setCheckable(true);
	action->setChecked(checked);
}

} // namespace Nagram
