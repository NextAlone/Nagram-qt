#pragma once

#include "base/flat_map.h"

#include <QtCore/QJsonObject>
#include <QtCore/QStringList>

namespace Core {
class Settings;
} // namespace Core

namespace Nagram {

inline constexpr auto kConfigVersion = 2;
inline constexpr auto kConfigMaxBytes = 1024 * 1024;

enum class ConfigError {
	None,
	TooLarge,
	InvalidJson,
	InvalidFormat,
	UnsupportedVersion,
	InvalidValue,
	ReadFailed,
	WriteFailed,
	Changed,
};

struct ConfigData {
	QJsonObject values;
	QStringList unknown;
	QStringList invalid;
	int migratedFrom = 0;
	ConfigError error = ConfigError::None;
};

using ConfigRaw = base::flat_map<QByteArray, QByteArray>;

[[nodiscard]] QJsonObject ConfigDefaults();
[[nodiscard]] ConfigRaw ReadConfigRaw(Core::Settings &settings);
[[nodiscard]] ConfigData ReadConfig(Core::Settings &settings);
[[nodiscard]] ConfigData ParseConfig(const QByteArray &bytes);
[[nodiscard]] QByteArray EncodeConfig(const QJsonObject &values);
[[nodiscard]] ConfigData LoadConfig(const QString &path);
[[nodiscard]] ConfigError SaveConfig(
	const QString &path,
	const QJsonObject &values);
[[nodiscard]] ConfigError ApplyConfig(
	Core::Settings &settings,
	const ConfigData &data,
	const ConfigRaw &baseline);

} // namespace Nagram
