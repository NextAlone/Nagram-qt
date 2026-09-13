#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QUrl>

namespace Core {
class Settings;
} // namespace Core

namespace Nagram {

inline constexpr auto kServicesKey = std::string_view("nagram.services");

enum class ServiceKind {
	Translation,
	Transcription,
};

struct ServiceDefinition {
	QString id;
	QString name;
	ServiceKind kind = ServiceKind::Translation;
	QString protocol;
	QUrl baseUrl;
	QString endpoint;
	QString model;
	QString credentialRef;
	bool useKey = true;
	QString systemPrompt;
	QString prompt;
	QString language;
	std::optional<double> temperature;
};

[[nodiscard]] QJsonObject ServicesDefaults();
[[nodiscard]] std::optional<ServiceDefinition> ParseService(const QJsonObject &value);
[[nodiscard]] QJsonObject SerializeService(const ServiceDefinition &value);
[[nodiscard]] bool ValidServices(const QJsonObject &value);
[[nodiscard]] std::optional<QJsonObject> Services(Core::Settings &settings);
void SetServices(Core::Settings &settings, const QJsonObject &value);
[[nodiscard]] std::optional<ServiceDefinition> FindService(
	const QJsonObject &settings,
	const QString &id);
[[nodiscard]] QString CredentialAccount(const ServiceDefinition &service);
[[nodiscard]] QUrl ServiceEndpoint(const ServiceDefinition &service);

} // namespace Nagram
