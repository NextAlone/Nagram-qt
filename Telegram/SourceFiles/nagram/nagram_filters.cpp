#include "nagram/nagram_filters.h"

#include "core/application.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_groups.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "nagram/nagram_translation.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QUuid>
#include <numeric>

namespace Nagram {
namespace {

constexpr auto kMaximumRules = 32;
constexpr auto kMaximumText = 16384;
constexpr auto kMaximumMatches = 256;
constexpr auto kMaximumWorkMs = 20;

QString Prefix() {
	return u"(*NO_JIT)(*LIMIT_MATCH=10000)(*LIMIT_DEPTH=64)(*LIMIT_HEAP=1024)"_q;
}

QRegularExpression Compile(const QJsonObject &rule) {
	return QRegularExpression(
		Prefix() + rule.value(u"pattern"_q).toString(),
		QRegularExpression::UseUnicodePropertiesOption
			| (rule.value(u"caseInsensitive"_q).toBool()
				? QRegularExpression::CaseInsensitiveOption
				: QRegularExpression::NoPatternOption));
}

bool Text(const QJsonValue &value, int maximum, bool empty = true) {
	if (!value.isString()) {
		return false;
	}
	const auto text = value.toString();
	return (empty || !text.isEmpty()) && text.size() <= maximum
		&& !text.contains(QChar(0))
		&& QString::fromUtf8(text.toUtf8()) == text;
}

bool PeerList(const QJsonValue &value) {
	if (!value.isArray() || value.toArray().size() > 1000) {
		return false;
	}
	auto seen = QSet<QString>();
	for (const auto &entry : value.toArray()) {
		const auto id = entry.toString();
		auto ok = false;
		const auto number = id.toULongLong(&ok);
		if (!entry.isString() || !ok || !number
			|| QString::number(number) != id || seen.contains(id)) {
			return false;
		}
		seen.insert(id);
	}
	return true;
}

ReadingProjection Identity(const TextWithEntities &text) {
	auto result = ReadingProjection{ .text = text };
	result.fromDisplay.resize(text.text.size() + 1);
	std::iota(result.fromDisplay.begin(), result.fromDisplay.end(), 0);
	result.originalStarts = result.fromDisplay;
	result.originalEnds = result.fromDisplay;
	return result;
}

struct Edit {
	int from = 0;
	int to = 0;
	QString replacement;
};

bool Boundary(const QString &text, int offset) {
	return offset >= 0 && offset <= text.size()
		&& (!offset || offset == text.size()
			|| !text[offset - 1].isHighSurrogate()
			|| !text[offset].isLowSurrogate());
}

bool ApplyEdits(ReadingProjection &projection, const std::vector<Edit> &edits) {
	const auto source = projection.text;
	auto next = ReadingProjection();
	auto starts = std::vector<int>(source.text.size() + 1, -1);
	auto ends = starts;
	auto from = 0;
	const auto copy = [&](int end) {
		while (from < end) {
			starts[from] = ends[from] = next.text.text.size();
			next.fromDisplay.push_back(from);
			next.text.text += source.text[from++];
		}
	};
	for (const auto &edit : edits) {
		if (edit.from < from || edit.to <= edit.from
			|| !Boundary(source.text, edit.from)
			|| !Boundary(source.text, edit.to)) {
			return false;
		}
		copy(edit.from);
		const auto begin = int(next.text.text.size());
		next.text.text += edit.replacement;
		const auto end = int(next.text.text.size());
		if (end > kMaximumText) {
			return false;
		}
		for (auto i = edit.from; i <= edit.to; ++i) {
			starts[i] = end;
			ends[i] = begin;
		}
		starts[edit.from] = ends[edit.from] = begin;
		starts[edit.to] = ends[edit.to] = end;
		for (auto i = begin; i < end; ++i) {
			next.fromDisplay.push_back(-1);
		}
		from = edit.to;
	}
	copy(source.text.size());
	starts.back() = ends.back() = next.text.text.size();
	next.fromDisplay.push_back(source.text.size());
	for (const auto &entity : source.entities) {
		const auto begin = entity.offset();
		const auto end = begin + entity.length();
		if (!Boundary(source.text, begin) || !Boundary(source.text, end)) {
			return false;
		}
		const auto overlaps = ranges::any_of(edits, [&](const Edit &edit) {
			return edit.from < end && edit.to > begin;
		});
		const auto type = entity.type();
		const auto format = type == EntityType::Bold
			|| type == EntityType::Italic || type == EntityType::Underline
			|| type == EntityType::StrikeOut || type == EntityType::Spoiler
			|| type == EntityType::Blockquote;
		if (overlaps && !format) {
			continue;
		}
		const auto offset = starts[begin];
		const auto length = ends[end] - offset;
		if (length > 0) {
			next.text.entities.emplace_back(type, offset, length, entity.data());
		}
	}
	for (auto &offset : next.fromDisplay) {
		offset = offset < 0 ? -1 : projection.fromDisplay[offset];
	}
	next.originalStarts = projection.originalStarts;
	next.originalEnds = projection.originalEnds;
	for (auto &offset : next.originalStarts) {
		offset = offset < 0 ? -1 : starts[offset];
	}
	for (auto &offset : next.originalEnds) {
		offset = offset < 0 ? -1 : ends[offset];
	}
	projection = std::move(next);
	return true;
}

QString Failure() {
	return u"limit"_q;
}

} // namespace

QJsonObject FilterDefaults() {
	return {
		{ u"version"_q, 1 },
		{ u"enabled"_q, false },
		{ u"filterOutgoing"_q, false },
		{ u"hideBlocked"_q, false },
		{ u"stripZalgo"_q, false },
		{ u"hiddenAuthors"_q, QJsonArray() },
		{ u"excludedPeers"_q, QJsonArray() },
		{ u"rules"_q, QJsonArray() },
	};
}

namespace {

QString ValidateFiltersImpl(const QJsonObject &value) {
	const auto invalid = [] { return u"invalid"_q; };
	if (value.keys() != FilterDefaults().keys()
		|| value.value(u"version"_q) != 1
		|| QJsonDocument(value).toJson().size() > 128 * 1024) {
		return invalid();
	}
	for (const auto &key : { u"enabled"_q, u"filterOutgoing"_q,
			u"hideBlocked"_q, u"stripZalgo"_q }) {
		if (!value.value(key).isBool()) {
			return invalid();
		}
	}
	if (!PeerList(value.value(u"hiddenAuthors"_q))
		|| !PeerList(value.value(u"excludedPeers"_q))
		|| !value.value(u"rules"_q).isArray()
		|| value.value(u"rules"_q).toArray().size() > kMaximumRules) {
		return invalid();
	}
	auto seen = QSet<QString>();
	for (const auto &entry : value.value(u"rules"_q).toArray()) {
		const auto rule = entry.toObject();
		const auto id = rule.value(u"id"_q).toString();
		const auto uuid = QUuid(id);
		const auto action = rule.value(u"action"_q).toString();
		if (!entry.isObject() || rule.size() != 8
			|| uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != id
			|| seen.contains(id)
			|| !Text(rule.value(u"title"_q), 128, false)
			|| !Text(rule.value(u"pattern"_q), 2048, false)
			|| !Text(rule.value(u"replacement"_q), 4096)
			|| !rule.value(u"enabled"_q).isBool()
			|| !rule.value(u"caseInsensitive"_q).isBool()
			|| !rule.value(u"reversed"_q).isBool()
			|| (action != u"mask"_q && action != u"replace"_q
				&& action != u"maskMessage"_q && action != u"hide"_q)
			|| (rule.value(u"reversed"_q).toBool()
				&& action != u"maskMessage"_q && action != u"hide"_q)) {
			return invalid();
		}
		seen.insert(id);
		const auto expression = Compile(rule);
		if (!expression.isValid()) {
			return u"pattern\n%1\n%2"_q.arg(
				QString::number(std::max(qsizetype(0),
					expression.patternErrorOffset() - Prefix().size())),
				expression.errorString());
		}
	}
	return QString();
}

} // namespace

QString ValidateFilters(const QJsonObject &value) {
	const auto error = ValidateFiltersImpl(value);
	if (error.startsWith(u"pattern\n"_q)) {
		return tr::lng_nagram_filter_pattern_error(tr::now,
			lt_index, error.section('\n', 1, 1),
			lt_error, error.section('\n', 2));
	}
	return error.isEmpty() ? QString() : tr::lng_nagram_filter_invalid(tr::now);
}

FilterResult ApplyFilters(
		const QJsonObject &config,
		const TextWithEntities &original,
		QString author,
		bool blocked,
		bool outgoing,
		const QString &searchable) {
	auto result = FilterResult();
	result.projection = (original.text.size() <= kMaximumText)
		? Identity(original) : ReadingProjection{ .text = original };
	if (const auto error = ValidateFiltersImpl(config); !error.isEmpty()) {
		result.error = error;
		result.hidden = true;
		return result;
	}
	if (!config.value(u"enabled"_q).toBool()
		|| (outgoing && !config.value(u"filterOutgoing"_q).toBool())) {
		return result;
	}
	if ((blocked && config.value(u"hideBlocked"_q).toBool())
		|| config.value(u"hiddenAuthors"_q).toArray().contains(author)) {
		result.hidden = true;
		return result;
	}
	if (original.text.size() > kMaximumText || !PlanTranslation(original)) {
		result.hidden = true;
		result.error = Failure();
		return result;
	}
	auto timer = QElapsedTimer();
	timer.start();
	auto matches = 0;
	if (config.value(u"stripZalgo"_q).toBool()) {
		auto marks = 0;
		auto edits = std::vector<Edit>();
		for (auto i = 0; i < original.text.size();) {
			const auto start = i;
			auto scalar = uint(original.text[i++].unicode());
			if (QChar::isHighSurrogate(scalar) && i < original.text.size()) {
				scalar = QChar::surrogateToUcs4(QChar(scalar), original.text[i++]);
			}
			const auto category = QChar::category(scalar);
			const auto combining = category == QChar::Mark_NonSpacing
				|| category == QChar::Mark_SpacingCombining
				|| category == QChar::Mark_Enclosing;
			marks = combining ? marks + 1 : 0;
			if (marks > 3 && scalar != 0xFE0E && scalar != 0xFE0F
				&& scalar != 0x20E3 && !(scalar >= 0xE0100 && scalar <= 0xE01EF)) {
				edits.push_back({ start, i, QString() });
			}
		}
		if (!ApplyEdits(result.projection, edits)) {
			result.error = Failure();
		}
	}
	for (const auto &entry : config.value(u"rules"_q).toArray()) {
		const auto rule = entry.toObject();
		if (!rule.value(u"enabled"_q).toBool()) {
			continue;
		}
		const auto expression = Compile(rule);
		const auto action = rule.value(u"action"_q).toString();
		const auto whole = action == u"hide"_q || action == u"maskMessage"_q;
		const auto &text = (whole && !searchable.isNull())
			? searchable : result.projection.text.text;
		if (text.size() > kMaximumText) {
			result.error = Failure();
			break;
		}
		auto edits = std::vector<Edit>();
		auto matched = false;
		for (auto offset = 0; offset <= text.size();) {
			if (timer.elapsed() >= kMaximumWorkMs) {
				result.error = Failure();
				break;
			}
			const auto match = expression.match(text, offset,
				QRegularExpression::NormalMatch,
				QRegularExpression::AnchorAtOffsetMatchOption);
			if (!match.isValid()) {
				result.error = Failure();
				break;
			}
			if (match.hasMatch()) {
				if (++matches > kMaximumMatches) {
					result.error = Failure();
					break;
				}
				matched = true;
				if (whole) {
					break;
				}
				const auto end = int(match.capturedEnd());
				if (end <= offset || !Boundary(text, end)
					|| match.capturedStart() != offset) {
					result.error = Failure();
					break;
				}
				edits.push_back({ offset, end,
					action == u"mask"_q ? u"•••"_q
						: rule.value(u"replacement"_q).toString() });
				offset = end;
			} else {
				offset += (offset < text.size() && text[offset].isHighSurrogate()) ? 2 : 1;
			}
		}
		if (!result.error.isEmpty()) {
			break;
		}
		if (whole) {
			if (matched != rule.value(u"reversed"_q).toBool()) {
				result.hidden = true;
				break;
			}
		} else if (!ApplyEdits(result.projection, edits)) {
			result.error = Failure();
			break;
		}
	}
	if (!result.error.isEmpty()) {
		result.hidden = true;
	}
	return result;
}

namespace {

QString SearchableText(not_null<HistoryItem*> item) {
	auto result = u""_q;
	const auto append = [&](const QString &text) {
		if (result.size() <= kMaximumText) {
			result += text.left(kMaximumText + 1 - result.size());
		}
	};
	const auto add = [&](not_null<HistoryItem*> part) {
		const auto &original = part->originalText();
		append(original.text);
		for (const auto &entity : original.entities) {
			if (entity.type() == EntityType::CustomUrl) {
				append(u"\n"_q + entity.data());
			}
		}
		if (const auto markup = part->Get<HistoryMessageReplyMarkup>()) {
			for (const auto &row : markup->data.rows) {
				for (const auto &button : row) {
					append(u"\n<button>"_q + button.text + u" "_q
						+ QString::fromUtf8(button.data) + u"</button>"_q);
				}
			}
		}
	};
	if (const auto group = item->history()->owner().groups().find(item)) {
		for (const auto part : group->items) {
			if (!result.isEmpty()) {
				append(u"\n"_q);
			}
			add(part);
		}
	} else {
		add(item);
	}
	return result;
}

} // namespace

std::shared_ptr<FilterState> FilterMessage(not_null<HistoryItem*> item) {
	if (item->isService() || item->nagramOriginalShown()) {
		return nullptr;
	}
	const auto session = &item->history()->session();
	const auto bytes = session->settings().nagramFilters();
	if (bytes.isEmpty()) {
		return nullptr;
	}
	const auto config = QJsonDocument::fromJson(bytes).object();
	if (config.value(u"enabled"_q).isBool()
		&& (!config.value(u"enabled"_q).toBool()
			|| config.value(u"excludedPeers"_q).toArray().contains(
				QString::number(SerializePeerId(item->history()->peer->id))))) {
		return nullptr;
	}
	const auto original = item->translatedTextWithLocalEntities();
	const auto searchable = SearchableText(item);
	const auto forwarded = item->Get<HistoryMessageForwarded>();
	const auto sources = std::array<PeerData*, 3>{
		(item->from() != item->history()->peer) ? item->from().get() : nullptr,
		forwarded ? forwarded->originalSender : nullptr,
		item->viaBot(),
	};
	auto blocked = false;
	auto author = QString();
	const auto hidden = config.value(u"hiddenAuthors"_q).toArray();
	for (const auto source : sources) {
		if (!source) {
			continue;
		}
		blocked = blocked || source->isBlocked();
		const auto id = QString::number(SerializePeerId(source->id));
		if (hidden.contains(id)) {
			author = id;
		}
	}
	const auto outgoing = item->out();
	auto &cached = item->nagramFilterState();
	if (cached && cached->configuration == bytes
		&& cached->searchable == searchable
		&& cached->original.text == original.text
		&& cached->original.entities == original.entities && cached->blocked == blocked
		&& cached->author == author && cached->outgoing == outgoing) {
		return cached;
	}
	cached = std::make_shared<FilterState>();
	cached->configuration = bytes;
	cached->original = original;
	cached->searchable = searchable;
	cached->blocked = blocked;
	cached->author = author;
	cached->outgoing = outgoing;
	const auto state = cached;
	const auto id = item->fullId();
	const auto apply = crl::guard(session, [=](FilterResult result) {
		if (const auto item = session->data().message(id)) {
			if (item->nagramFilterState() == state) {
				state->result = std::move(result);
				state->ready = true;
				if (const auto group = session->data().groups().find(item)) {
					for (const auto part : group->items) {
						session->data().requestItemViewRefresh(part);
					}
				} else {
					session->data().requestItemViewRefresh(item);
				}
			}
		}
	});
	crl::async([=] {
		auto result = ApplyFilters(config, original, author, blocked, outgoing, searchable);
		crl::on_main([apply, result = std::move(result)]() mutable {
			apply(std::move(result));
		});
	});
	return state;
}

QString FilterPlaceholder(const FilterState &state) {
	return !state.ready ? tr::lng_nagram_filter_pending(tr::now)
		: state.result.error == u"limit"_q ? tr::lng_nagram_filter_limit(tr::now)
		: !state.result.error.isEmpty() ? tr::lng_nagram_filter_invalid(tr::now)
		: tr::lng_nagram_filter_hidden(tr::now);
}

ReadingProjection FilterProjection(const FilterState &state) {
	if (state.ready && !state.result.hidden) {
		return state.result.projection;
	}
	auto result = ReadingProjection();
	result.text = { FilterPlaceholder(state) };
	result.fromDisplay.assign(result.text.text.size() + 1, -1);
	if (state.original.text.size() <= kMaximumText) {
		result.originalStarts.assign(state.original.text.size() + 1, -1);
		result.originalEnds = result.originalStarts;
	}
	return result;
}

void SetFilters(not_null<Main::Session*> session, QJsonObject value) {
	Expects(ValidateFilters(value).isEmpty());
	auto rules = value.value(u"rules"_q).toArray();
	for (auto i = 0; i != rules.size(); ++i) {
		auto rule = rules[i].toObject();
		if (rule.value(u"action"_q) == u"maskMessage"_q) {
			rule.insert(u"action"_q, u"hide"_q);
			rules[i] = rule;
		}
	}
	value.insert(u"rules"_q, rules);
	session->settings().setNagramFilters(value == FilterDefaults()
		? QByteArray() : QJsonDocument(value).toJson(QJsonDocument::Compact));
	session->saveSettingsDelayed();
}

} // namespace Nagram
