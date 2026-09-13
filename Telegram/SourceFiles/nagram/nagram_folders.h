#pragma once

#include "base/flat_set.h"
#include "data/data_types.h"

class PeerData;

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class PopupMenu;
class Show;
} // namespace Ui

namespace Nagram {

using ManagedFolders = base::flat_set<FilterId>;

[[nodiscard]] std::optional<ManagedFolders> ParseManagedFolders(
	const QByteArray &serialized);
[[nodiscard]] QByteArray SerializeManagedFolders(const ManagedFolders &ids);
[[nodiscard]] bool IsManagedPeer(not_null<PeerData*> peer);
void AddManagedFolderAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<Main::Session*> session,
	std::shared_ptr<Ui::Show> show,
	FilterId id);

} // namespace Nagram
