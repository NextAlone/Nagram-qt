#pragma once

#include "ui/text/text_entity.h"

#include <QtCore/QJsonObject>
#include <rpl/producer.h>

class PeerData;

namespace Core {
class Settings;
} // namespace Core

namespace Nagram {

inline constexpr auto kTextToolsKey = std::string_view("nagram.textTools");

[[nodiscard]] QJsonObject TextToolsDefaults();
[[nodiscard]] bool ValidTextTools(const QJsonObject &value);
[[nodiscard]] std::optional<QJsonObject> TextTools(Core::Settings &settings);
void SetTextTools(Core::Settings &settings, const QJsonObject &value);

inline constexpr auto kInputPlaceholderKey = std::string_view("nagram.inputPlaceholder");
[[nodiscard]] QString InputPlaceholderMode(Core::Settings &settings);
[[nodiscard]] rpl::producer<QString> InputPlaceholderModeValue(Core::Settings &settings);
void SetInputPlaceholderMode(Core::Settings &settings, const QString &mode);
[[nodiscard]] rpl::producer<QString> InputPlaceholder(
	Core::Settings &settings,
	not_null<PeerData*> peer);

[[nodiscard]] QString NarrowInterfaceSymbols(QString text);

[[nodiscard]] TextWithEntities AddTextSpacing(const TextWithEntities &text);
[[nodiscard]] TextWithEntities PrepareText(
	Core::Settings &settings,
	const TextWithEntities &text,
	bool editing);

} // namespace Nagram
