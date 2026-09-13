#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QUrl>

namespace Core { class Settings; }
struct ClickHandlerContext;

namespace Nagram {

inline constexpr auto kLinkRulesKey = std::string_view("nagram.linkRules");
[[nodiscard]] QJsonObject LinkRulesDefaults();
[[nodiscard]] bool ValidLinkRules(const QJsonObject &value);
[[nodiscard]] std::optional<QJsonObject> LinkRules(Core::Settings &settings);
void SetLinkRules(Core::Settings &settings, const QJsonObject &value);
[[nodiscard]] std::variant<QUrl, QString> RewriteLink(
	const QJsonObject &config,
	const QString &original);
[[nodiscard]] bool HandleExternalLink(
	const QString &url,
	const ClickHandlerContext &context);

} // namespace Nagram
