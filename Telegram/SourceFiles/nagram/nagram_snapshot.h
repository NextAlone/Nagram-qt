#pragma once

#include <QtCore/QJsonObject>
#include <QtGui/QImage>

class HistoryItem;
namespace Ui { class PopupMenu; }
namespace Window { class SessionController; }

namespace Nagram {

inline constexpr auto kSnapshotKey = std::string_view("nagram.snapshot");

[[nodiscard]] QJsonObject SnapshotDefaults();
[[nodiscard]] bool ValidSnapshot(const QJsonObject &value);
[[nodiscard]] std::variant<QImage, QString> RenderSnapshot(
	not_null<Window::SessionController*> controller,
	const MessageIdsList &ids,
	const QJsonObject &options,
	bool revealSpoilers);
void AddSnapshotAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<Window::SessionController*> controller,
	MessageIdsList ids);

} // namespace Nagram
