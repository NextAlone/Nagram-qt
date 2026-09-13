#pragma once

#include "nagram/nagram_reading.h"
#include <QtCore/QJsonObject>

class HistoryItem;
namespace Main { class Session; }

namespace Nagram {

struct FilterResult {
	ReadingProjection projection;
	bool hidden = false;
	QString error;
};

struct FilterState {
	QByteArray configuration;
	TextWithEntities original;
	QString searchable;
	QString author;
	bool blocked = false;
	bool outgoing = false;
	bool ready = false;
	FilterResult result;
};

[[nodiscard]] QJsonObject FilterDefaults();
[[nodiscard]] QString ValidateFilters(const QJsonObject &value);
[[nodiscard]] FilterResult ApplyFilters(
	const QJsonObject &config,
	const TextWithEntities &original,
	QString author,
	bool blocked,
	bool outgoing,
	const QString &searchable = QString());
[[nodiscard]] std::shared_ptr<FilterState> FilterMessage(
	not_null<HistoryItem*> item);
[[nodiscard]] ReadingProjection FilterProjection(
	const FilterState &state);
[[nodiscard]] QString FilterPlaceholder(const FilterState &state);
void SetFilters(not_null<Main::Session*> session, QJsonObject value);

} // namespace Nagram
