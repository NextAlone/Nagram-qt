#include "nagram/nagram_services.h"

#include "core/core_settings.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QUuid>

#include <cmath>

namespace Nagram {
namespace {

bool ValidId(const QString &value) {
	const auto id = QUuid(value);
	return !id.isNull() && id.toString(QUuid::WithoutBraces) == value;
}

bool ValidText(const QJsonValue &value, int maximum, bool multiline = false) {
	const auto text = value.toString();
	return value.isString()
		&& text.size() <= maximum
		&& QString::fromUtf8(text.toUtf8()) == text
		&& !text.contains(QChar(0))
		&& (multiline || (!text.contains('\n') && !text.contains('\r')));
}

} // namespace

QJsonObject ServicesDefaults() {
	return {
		{ u"version"_q, 1 },
		{ u"translation"_q, QString() },
		{ u"transcription"_q, QString() },
		{ u"instances"_q, QJsonArray() },
	};
}

QJsonObject SerializeService(const ServiceDefinition &value) {
	return {
		{ u"id"_q, value.id },
		{ u"name"_q, value.name },
		{ u"kind"_q, value.kind == ServiceKind::Translation
			? u"translation"_q : u"transcription"_q },
		{ u"protocol"_q, value.protocol },
		{ u"baseUrl"_q, value.baseUrl.toString(QUrl::FullyEncoded) },
		{ u"endpoint"_q, value.endpoint },
		{ u"model"_q, value.model },
		{ u"credentialRef"_q, value.credentialRef },
		{ u"useKey"_q, value.useKey },
		{ u"systemPrompt"_q, value.systemPrompt },
		{ u"prompt"_q, value.prompt },
		{ u"language"_q, value.language },
		{ u"temperature"_q, value.temperature
			? QJsonValue(*value.temperature) : QJsonValue() },
	};
}

std::optional<ServiceDefinition> ParseService(const QJsonObject &value) {
	if (value.keys() != SerializeService({}).keys()
		|| !value.value(u"useKey"_q).isBool()) {
		return std::nullopt;
	}
	for (const auto &key : { u"id"_q, u"credentialRef"_q }) {
		if (!value.value(key).isString() || !ValidId(value.value(key).toString())) {
			return std::nullopt;
		}
	}
	for (const auto &key : { u"name"_q, u"model"_q, u"language"_q }) {
		if (!ValidText(value.value(key), 256)) {
			return std::nullopt;
		}
	}
	for (const auto &key : { u"baseUrl"_q, u"endpoint"_q }) {
		if (!ValidText(value.value(key), 2048)) {
			return std::nullopt;
		}
	}
	for (const auto &key : { u"systemPrompt"_q, u"prompt"_q }) {
		if (!ValidText(value.value(key), 16384, true)) {
			return std::nullopt;
		}
	}
	const auto kind = value.value(u"kind"_q).toString();
	const auto protocol = value.value(u"protocol"_q).toString();
	if ((kind != u"translation"_q && kind != u"transcription"_q)
		|| (protocol != u"openai"_q && protocol != u"deepl"_q)
		|| (kind == u"transcription"_q && protocol != u"openai"_q)
		|| value.value(u"name"_q).toString().trimmed().isEmpty()
		|| (protocol == u"openai"_q && value.value(u"model"_q).toString().trimmed().isEmpty())) {
		return std::nullopt;
	}
	if ((protocol == u"deepl"_q
		&& (!value.value(u"model"_q).toString().isEmpty()
			|| !value.value(u"systemPrompt"_q).toString().isEmpty()
			|| !value.value(u"prompt"_q).toString().isEmpty()
			|| !value.value(u"temperature"_q).isNull()))
		|| (kind == u"transcription"_q
			&& !value.value(u"systemPrompt"_q).toString().isEmpty())
		|| (kind == u"translation"_q
			&& !value.value(u"language"_q).toString().isEmpty())) {
		return std::nullopt;
	}
	const auto url = QUrl(value.value(u"baseUrl"_q).toString(), QUrl::StrictMode);
	const auto endpoint = value.value(u"endpoint"_q).toString();
	const auto relative = QUrl(endpoint, QUrl::StrictMode);
	const auto loopback = (url.host() == u"localhost"_q
		|| url.host() == u"127.0.0.1"_q
		|| url.host() == u"::1"_q);
	if (!url.isValid() || url.host().isEmpty()
		|| (url.scheme() != u"https"_q && !(loopback && url.scheme() == u"http"_q))
		|| !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()
		|| !relative.isValid() || !relative.isRelative()
		|| endpoint.isEmpty() || endpoint.startsWith('/')
		|| endpoint.contains('\\') || !relative.authority().isEmpty()
		|| QUrl::fromPercentEncoding(endpoint.toUtf8()).contains('\\')
		|| relative.hasQuery() || relative.hasFragment()
		|| endpoint.split('/').contains(u".."_q)
		|| QUrl::fromPercentEncoding(endpoint.toUtf8()).split('/').contains(u".."_q)) {
		return std::nullopt;
	}
	const auto temperature = value.value(u"temperature"_q);
	if (!temperature.isNull()) {
		if (!temperature.isDouble() || !std::isfinite(temperature.toDouble())
			|| temperature.toDouble() < 0 || temperature.toDouble() > 2) {
			return std::nullopt;
		}
		if (kind == u"transcription"_q && temperature.toDouble() > 1) {
			return std::nullopt;
		}
	}
	static const auto language = QRegularExpression(u"\\A(?:[a-z]{2})?\\z"_q);
	if (!language.match(value.value(u"language"_q).toString()).hasMatch()) {
		return std::nullopt;
	}
	return ServiceDefinition{
		.id = value.value(u"id"_q).toString(),
		.name = value.value(u"name"_q).toString(),
		.kind = kind == u"translation"_q ? ServiceKind::Translation : ServiceKind::Transcription,
		.protocol = protocol,
		.baseUrl = url,
		.endpoint = endpoint,
		.model = value.value(u"model"_q).toString(),
		.credentialRef = value.value(u"credentialRef"_q).toString(),
		.useKey = value.value(u"useKey"_q).toBool(),
		.systemPrompt = value.value(u"systemPrompt"_q).toString(),
		.prompt = value.value(u"prompt"_q).toString(),
		.language = value.value(u"language"_q).toString(),
		.temperature = temperature.isNull() ? std::nullopt : std::make_optional(temperature.toDouble()),
	};
}

bool ValidServices(const QJsonObject &value) {
	if (value.keys() != ServicesDefaults().keys()
		|| value.value(u"version"_q) != QJsonValue(1)
		|| !value.value(u"instances"_q).isArray()) {
		return false;
	}
	auto ids = base::flat_map<QString, ServiceKind>();
	for (const auto &entry : value.value(u"instances"_q).toArray()) {
		const auto parsed = entry.isObject() ? ParseService(entry.toObject()) : std::nullopt;
		if (!parsed || !ids.emplace(parsed->id, parsed->kind).second) {
			return false;
		}
	}
	for (const auto &key : { u"translation"_q, u"transcription"_q }) {
		if (!value.value(key).isString()) {
			return false;
		}
		const auto id = value.value(key).toString();
		if (id.isEmpty() || id == u"telegram"_q) {
			continue;
		} else if (key == u"translation"_q && id == u"system"_q) {
			continue;
		}
		const auto i = ids.find(id);
		const auto kind = key == u"translation"_q ? ServiceKind::Translation : ServiceKind::Transcription;
		if (i == ids.end() || i->second != kind) {
			return false;
		}
	}
	return true;
}

std::optional<QJsonObject> Services(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kServicesKey);
	if (bytes.isEmpty()) {
		return ServicesDefaults();
	}
	const auto document = QJsonDocument::fromJson(bytes);
	if (!document.isObject() || !ValidServices(document.object())) {
		LOG(("Nagram Error: Invalid services configuration; external services disabled."));
		return std::nullopt;
	}
	return document.object();
}

void SetServices(Core::Settings &settings, const QJsonObject &value) {
	Expects(ValidServices(value));
	if (value == ServicesDefaults()) {
		settings.clearPref(kServicesKey);
	} else {
		settings.writePref<QByteArray>(kServicesKey,
			QJsonDocument(value).toJson(QJsonDocument::Compact));
	}
}

std::optional<ServiceDefinition> FindService(const QJsonObject &settings, const QString &id) {
	for (const auto &entry : settings.value(u"instances"_q).toArray()) {
		if (entry.toObject().value(u"id"_q) == id) {
			return ParseService(entry.toObject());
		}
	}
	return std::nullopt;
}

QUrl ServiceEndpoint(const ServiceDefinition &service) {
	auto base = service.baseUrl;
	if (!base.path().endsWith('/')) {
		base.setPath(base.path() + '/');
	}
	return base.resolved(QUrl(service.endpoint, QUrl::StrictMode));
}

QString CredentialAccount(const ServiceDefinition &service) {
	const auto binding = QJsonDocument(QJsonArray{
		ServiceEndpoint(service).toString(QUrl::FullyEncoded),
		service.protocol,
		service.kind == ServiceKind::Translation ? u"translation"_q : u"transcription"_q,
	}).toJson(QJsonDocument::Compact);
	return service.credentialRef + '.' + QString::fromLatin1(
		QCryptographicHash::hash(binding, QCryptographicHash::Sha256).toHex());
}

} // namespace Nagram
