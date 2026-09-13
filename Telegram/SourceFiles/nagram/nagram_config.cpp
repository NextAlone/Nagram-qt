#include "nagram/nagram_config.h"

#include "nagram/nagram_chat_sort.h"
#include "nagram/nagram_main_menu.h"

#include "core/core_settings.h"
#include "nagram/nagram_settings.h"
#include "nagram/nagram_text.h"
#include "nagram/nagram_menu.h"
#include "nagram/nagram_services.h"
#include "nagram/nagram_reading.h"
#include "nagram/nagram_snapshot.h"
#include "nagram/nagram_links.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QSaveFile>

#include <cmath>

namespace Nagram {
namespace {

QString Key(std::string_view key) {
	return QString::fromUtf8(key.data(), key.size());
}

bool Exportable(Option option) {
	switch (option) {
	case Option::HidePhoneNumber:
	case Option::TranslateBeforeSend:
	case Option::AutoTranslate:
	case Option::RegexFilters:
		return false;
	default:
		return true;
	}
}

bool ValidValue(
		const QString &key,
		const QJsonValue &value,
		const QJsonValue &fallback) {
	if (key == Key(kMonospaceFontKey)) {
		return value.isString() && ValidMonospaceFont(value.toString());
	} else if (key == Key(kMainMenuKey)) {
		return value.isObject() && ValidMainMenu(value.toObject());
	} else if (key == Key(kChatSortKey)) {
		return value.isObject() && ValidChatSort(value.toObject());
	} else if (key == Key(kSnapshotKey)) {
		return value.isObject() && ValidSnapshot(value.toObject());
	} else if (key == Key(kLinkRulesKey)) {
		return value.isObject() && ValidLinkRules(value.toObject());
	} else if (key == Key(kInputPlaceholderKey)) {
		return value.isString() && (value.toString().isEmpty()
			|| value == u"chat"_q || value == u"sender"_q);
	} else if (key == Key(kReadingChineseKey)) {
		return value.isString() && (value.toString().isEmpty()
			|| value == u"simplified"_q || value == u"traditional"_q);
	} else if (key == Key(kServicesKey)) {
		return value.isObject() && ValidServices(value.toObject());
	} else if (key == Key(kMenuToolsKey)) {
		return value.isObject() && ValidMenuTools(value.toObject());
	} else if (key == Key(kTextToolsKey)) {
		return value.isObject() && ValidTextTools(value.toObject());
	} else if (key == Key(kEditedMarkKey)) {
		const auto text = value.toString();
		return value.isString()
			&& QString::fromUtf8(text.toUtf8()) == text
			&& !text.contains('\n')
			&& !text.contains('\r');
	} else if (fallback.isBool()) {
		return value.isBool();
	} else if (!value.isDouble()) {
		return false;
	}
	const auto number = value.toDouble();
	if (!std::isfinite(number) || std::floor(number) != number) {
		return false;
	} else if (key == Key(kStickerScaleKey)) {
		return ranges::contains(kStickerScales, number);
	} else if (key == Key(kMessageWidthKey)) {
		return !number || (number >= 50 && number <= 400);
	} else if (key == Key(kBubbleRoundnessKey)
		|| key == Key(kAvatarRoundnessKey)) {
		return !number || (number >= 10 && number <= 100);
	} else if (key == Key(kChatPreviewLinesKey)) {
		return number >= 0 && number <= 3;
	} else if (key == Key(kRecentStickerLimitKey)) {
		return number >= 0 && number <= 200;
	} else if (key == Key(kNotificationDelayKey)
		|| key == Key(kCloudNotificationDelayKey)) {
		return number >= 0 && number <= kMaxNotificationDelay;
	}
	return false;
}

ConfigData Validate(const QJsonObject &values) {
	auto result = ConfigData();
	const auto defaults = ConfigDefaults();
	for (auto i = values.begin(); i != values.end(); ++i) {
		if (!defaults.contains(i.key())) {
			result.unknown.push_back(i.key());
		} else if (!ValidValue(i.key(), i.value(), defaults.value(i.key()))) {
			result.invalid.push_back(i.key());
		} else {
			result.values.insert(i.key(), i.value());
		}
	}
	if (!result.invalid.empty()) {
		result.error = ConfigError::InvalidValue;
	}
	return result;
}

} // namespace

QJsonObject ConfigDefaults() {
	auto result = QJsonObject();
	for (const auto &definition : kOptions) {
		if (Exportable(definition.option)) {
			result.insert(Key(definition.key), definition.defaultValue);
		}
	}
	result.insert(Key(kStickerScaleKey), 100);
	result.insert(Key(kMessageWidthKey), 0);
	result.insert(Key(kBubbleRoundnessKey), 0);
	result.insert(Key(kAvatarRoundnessKey), 0);
	result.insert(Key(kChatPreviewLinesKey), 0);
	result.insert(Key(kRecentStickerLimitKey), 0);
	result.insert(Key(kEditedMarkKey), QString());
	result.insert(Key(kTextToolsKey), TextToolsDefaults());
	result.insert(Key(kMenuToolsKey), MenuToolsDefaults());
	result.insert(Key(kServicesKey), ServicesDefaults());
	result.insert(Key(kReadingChineseKey), QString());
	result.insert(Key(kMainMenuKey), MainMenuDefaults());
	result.insert(Key(kMonospaceFontKey), QString());
	result.insert(Key(kChatSortKey), ChatSortDefaults());
	result.insert(Key(kSnapshotKey), SnapshotDefaults());
	result.insert(Key(kLinkRulesKey), LinkRulesDefaults());
	result.insert(Key(kInputPlaceholderKey), QString());
	result.insert(Key(kNotificationDelayKey), 0);
	result.insert(Key(kCloudNotificationDelayKey), 0);
	return result;
}

ConfigRaw ReadConfigRaw(Core::Settings &settings) {
	auto result = ConfigRaw();
	const auto defaults = ConfigDefaults();
	for (auto i = defaults.begin(); i != defaults.end(); ++i) {
		const auto key = i.key().toUtf8();
		result.emplace(key, settings.readPref<QByteArray>(key.toStdString()));
	}
	return result;
}

ConfigData ReadConfig(Core::Settings &settings) {
	auto result = ConfigData();
	const auto defaults = ConfigDefaults();
	for (const auto &[rawKey, bytes] : ReadConfigRaw(settings)) {
		const auto key = QString::fromUtf8(rawKey);
		const auto fallback = defaults.value(key);
		auto value = fallback;
		if (!bytes.isEmpty()) {
			if (fallback.isBool()) {
				value = (bytes == QByteArray(1, '\x01'))
					? QJsonValue(true)
					: QJsonValue();
			} else if (fallback.isString()) {
				const auto text = QString::fromUtf8(bytes);
				value = (text.toUtf8() == bytes)
					? QJsonValue(text)
					: QJsonValue();
			} else if (fallback.isObject()) {
				const auto document = QJsonDocument::fromJson(bytes);
				value = document.isObject()
					? QJsonValue(document.object())
					: QJsonValue();
			} else {
				auto ok = false;
				const auto number = bytes.toInt(&ok);
				value = ok ? QJsonValue(number) : QJsonValue();
			}
		}
		result.values.insert(key, value);
		if (!ValidValue(key, value, fallback)) {
			result.invalid.push_back(key);
			result.error = ConfigError::InvalidValue;
		}
	}
	return result;
}

QByteArray EncodeConfig(const QJsonObject &values) {
	return QJsonDocument(QJsonObject{
		{ u"format"_q, u"nagram-desktop-settings"_q },
		{ u"version"_q, kConfigVersion },
		{ u"settings"_q, values },
	}).toJson();
}

ConfigData ParseConfig(const QByteArray &bytes) {
	if (bytes.size() > kConfigMaxBytes) {
		return { .error = ConfigError::TooLarge };
	}
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(bytes, &error);
	if (error.error != QJsonParseError::NoError) {
		return { .error = ConfigError::InvalidJson };
	}
	const auto root = document.object();
	if (!document.isObject()
		|| root.value(u"format"_q) != u"nagram-desktop-settings"_q
		|| !root.value(u"version"_q).isDouble()
		|| !root.value(u"settings"_q).isObject()) {
		return { .error = ConfigError::InvalidFormat };
	}
	const auto version = root.value(u"version"_q).toDouble();
	if (version != 1 && version != kConfigVersion) {
		return { .error = ConfigError::UnsupportedVersion };
	}
	auto values = root.value(u"settings"_q).toObject();
	if (version == 1) {
		const auto defaults = ConfigDefaults();
		for (auto i = values.begin(); i != values.end(); ++i) {
			if (defaults.value(i.key()).isObject()) {
				return { .error = ConfigError::InvalidFormat };
			}
		}
	}
	auto result = Validate(values);
	result.migratedFrom = (version == kConfigVersion) ? 0 : int(version);
	return result;
}

ConfigData LoadConfig(const QString &path) {
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return { .error = ConfigError::ReadFailed };
	} else if (file.size() > kConfigMaxBytes) {
		return { .error = ConfigError::TooLarge };
	}
	const auto bytes = file.read(kConfigMaxBytes + 1);
	return (file.error() == QFileDevice::NoError)
		? ParseConfig(bytes)
		: ConfigData{ .error = ConfigError::ReadFailed };
}

ConfigError SaveConfig(const QString &path, const QJsonObject &values) {
	const auto data = Validate(values);
	if (data.error != ConfigError::None || !data.unknown.empty()) {
		return ConfigError::InvalidValue;
	}
	const auto bytes = EncodeConfig(values);
	if (bytes.size() > kConfigMaxBytes) {
		return ConfigError::TooLarge;
	}
	auto file = QSaveFile(path);
	if (!file.open(QIODevice::WriteOnly)
		|| file.write(bytes) != bytes.size()
		|| !file.commit()) {
		return ConfigError::WriteFailed;
	}
	return ConfigError::None;
}

ConfigError ApplyConfig(
		Core::Settings &settings,
		const ConfigData &data,
		const ConfigRaw &baseline) {
	if (data.error != ConfigError::None) {
		return data.error;
	}
	const auto validated = Validate(data.values);
	if (validated.error != ConfigError::None || !validated.unknown.empty()) {
		return ConfigError::InvalidValue;
	}
	const auto raw = ReadConfigRaw(settings);
	const auto current = ReadConfig(settings);
	const auto defaults = ConfigDefaults();
	auto changes = base::flat_map<QByteArray, std::optional<QByteArray>>();
	for (auto i = data.values.begin(); i != data.values.end(); ++i) {
		const auto key = i.key().toUtf8();
		const auto before = baseline.find(key);
		if (before == baseline.end() || before->second != raw.find(key)->second) {
			return ConfigError::Changed;
		} else if (current.values.value(i.key()) == i.value()) {
			continue;
		}
		const auto value = i.value();
		if (value == defaults.value(i.key())) {
			changes.emplace(key, std::nullopt);
		} else {
			changes.emplace(key, value.isBool()
				? QByteArray(1, '\x01')
				: value.isString()
				? value.toString().toUtf8()
				: value.isObject()
				? QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact)
				: QByteArray::number(value.toInt()));
		}
	}
	settings.applyPrefChanges(changes);
	return ConfigError::None;
}

} // namespace Nagram
