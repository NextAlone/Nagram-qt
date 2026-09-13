#pragma once

#include <QtCore/QJsonObject>

namespace Core { class Settings; }
namespace Dialogs { class Entry; }
namespace Ui { class GenericBox; }

namespace Nagram {

inline constexpr auto kChatSortKey = std::string_view("nagram.chatSort");

[[nodiscard]] QString ChatSortRuleTitle(const QString &id);
[[nodiscard]] QJsonObject ChatSortDefaults();
[[nodiscard]] bool ValidChatSort(const QJsonObject &value);
[[nodiscard]] std::optional<QJsonObject> ChatSort(Core::Settings &settings);
void SetChatSort(Core::Settings &settings, const QJsonObject &value);
[[nodiscard]] std::vector<QString> ChatSortOrder(Core::Settings &settings);
[[nodiscard]] uint8 ChatSortPriority(not_null<Dialogs::Entry*> entry);
void ChatSortBox(not_null<Ui::GenericBox*> box);

} // namespace Nagram
