#pragma once

#include <QtCore/QJsonObject>

namespace Core { class Settings; }
namespace Ui {
class GenericBox;
class VerticalLayout;
} // namespace Ui

namespace Nagram {

inline constexpr auto kMainMenuKey = std::string_view("nagram.mainMenu");

[[nodiscard]] QString MainMenuActionTitle(const QString &id);
[[nodiscard]] QJsonObject MainMenuDefaults();
[[nodiscard]] bool ValidMainMenu(const QJsonObject &value);
[[nodiscard]] std::optional<QJsonObject> MainMenu(Core::Settings &settings);
void SetMainMenu(Core::Settings &settings, const QJsonObject &value);
[[nodiscard]] QString MainMenuTitle(Core::Settings &settings);
[[nodiscard]] bool MainMenuSeasonal(Core::Settings &settings);
[[nodiscard]] bool MainMenuCustomOrder(Core::Settings &settings);
[[nodiscard]] not_null<Ui::VerticalLayout*> AddMainMenuGroup(
	not_null<Ui::VerticalLayout*> menu,
	const QString &id);
void MainMenuBox(not_null<Ui::GenericBox*> box);

} // namespace Nagram
