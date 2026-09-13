#include "nagram/nagram_text.h"

#include "nagram/nagram_settings.h"
#include "core/core_settings.h"
#include "data/data_peer.h"
#include "data/data_changes.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/session/send_as_peers.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>

namespace Nagram {
namespace {

bool IsHan(char32_t ch) {
	return QChar::script(ch) == QChar::Script_Han;
}

bool IsLatinOrDigit(char32_t ch) {
	return (ch >= 'a' && ch <= 'z')
		|| (ch >= 'A' && ch <= 'Z')
		|| (ch >= '0' && ch <= '9');
}

bool Protected(EntityType type) {
	switch (type) {
	case EntityType::Url:
	case EntityType::CustomUrl:
	case EntityType::Email:
	case EntityType::Hashtag:
	case EntityType::Cashtag:
	case EntityType::Mention:
	case EntityType::MentionName:
	case EntityType::CustomEmoji:
	case EntityType::BotCommand:
	case EntityType::MediaTimestamp:
	case EntityType::Phone:
	case EntityType::BankCard:
	case EntityType::Code:
	case EntityType::Pre:
	case EntityType::FormattedDate:
		return true;
	default:
		return false;
	}
}

} // namespace

QString NarrowInterfaceSymbols(QString text) {
	for (auto i = qsizetype(0); i != text.size(); ++i) {
		const auto ch = text.at(i).unicode();
		if (ch >= 0xFF01 && ch <= 0xFF5E) {
			text[i] = QChar(ch - 0xFEE0);
		} else if (ch == 0x3000) {
			text[i] = QChar(' ');
		} else if (ch == 0x3002) {
			text[i] = QChar('.');
		}
	}
	return text;
}

QString InputPlaceholderMode(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kInputPlaceholderKey);
	if (bytes.isEmpty() || bytes == "chat" || bytes == "sender") {
		return QString::fromUtf8(bytes);
	}
	LOG(("Nagram Error: Invalid input placeholder preference; using inherited value."));
	return {};
}

rpl::producer<QString> InputPlaceholderModeValue(Core::Settings &settings) {
	return rpl::single(rpl::empty) | rpl::then(
		settings.saveDelayedRequests()
	) | rpl::map([&settings] {
		return InputPlaceholderMode(settings);
	}) | rpl::distinct_until_changed();
}

void SetInputPlaceholderMode(Core::Settings &settings, const QString &mode) {
	Expects(mode.isEmpty() || mode == u"chat"_q || mode == u"sender"_q);
	if (mode.isEmpty()) {
		settings.clearPref(kInputPlaceholderKey);
	} else {
		settings.writePref<QByteArray>(kInputPlaceholderKey, mode.toUtf8());
	}
}

rpl::producer<QString> InputPlaceholder(
		Core::Settings &settings,
		not_null<PeerData*> peer) {
	return rpl::single(rpl::empty) | rpl::then(rpl::merge(
		settings.saveDelayedRequests(),
		peer->session().sendAsPeers().updated() | rpl::map([] { return rpl::empty; }),
		peer->session().changes().peerUpdates(Data::PeerUpdate::Flag::Name)
			| rpl::map([] { return rpl::empty; }),
		tr::lng_message_ph() | rpl::map([] { return rpl::empty; })
	)) | rpl::map([&settings, peer] {
		const auto mode = InputPlaceholderMode(settings);
		if (mode == u"chat"_q) {
			return tr::lng_nagram_input_chat_hint(tr::now, lt_name, peer->name());
		} else if (mode == u"sender"_q) {
			return tr::lng_nagram_input_sender_hint(
				tr::now,
				lt_name,
				peer->session().sendAsPeers().resolveChosen(peer)->name());
		}
		return tr::lng_message_ph(tr::now);
	}) | rpl::distinct_until_changed();
}

QJsonObject TextToolsDefaults() {
	return {
		{ u"version"_q, 1 },
		{ u"codeLanguage"_q, QString() },
		{ u"quickReplies"_q, QJsonArray{ QString(), QString() } },
	};
}

bool ValidTextTools(const QJsonObject &value) {
	if (value.keys() != TextToolsDefaults().keys()
		|| value.value(u"version"_q) != QJsonValue(1)
		|| !value.value(u"codeLanguage"_q).isString()
		|| !value.value(u"quickReplies"_q).isArray()) {
		return false;
	}
	static const auto language = QRegularExpression(u"\\A[a-zA-Z0-9+\\-]{0,32}\\z"_q);
	if (!language.match(value.value(u"codeLanguage"_q).toString()).hasMatch()) {
		return false;
	}
	const auto replies = value.value(u"quickReplies"_q).toArray();
	return replies.size() == 2 && ranges::all_of(replies, [](const auto &reply) {
		const auto text = reply.toString();
		return reply.isString() && QString::fromUtf8(text.toUtf8()) == text;
	});
}

std::optional<QJsonObject> TextTools(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kTextToolsKey);
	if (bytes.isEmpty()) {
		return TextToolsDefaults();
	}
	const auto document = QJsonDocument::fromJson(bytes);
	if (!document.isObject() || !ValidTextTools(document.object())) {
		LOG(("Nagram Error: Invalid textTools configuration; text tools not applied."));
		return std::nullopt;
	}
	return document.object();
}

void SetTextTools(Core::Settings &settings, const QJsonObject &value) {
	Expects(ValidTextTools(value));
	if (value == TextToolsDefaults()) {
		settings.clearPref(kTextToolsKey);
	} else {
		settings.writePref<QByteArray>(kTextToolsKey,
			QJsonDocument(value).toJson(QJsonDocument::Compact));
	}
}

TextWithEntities AddTextSpacing(const TextWithEntities &text) {
	const auto length = int(text.text.size());
	if (length < 2) {
		return text;
	}
	if (ranges::any_of(text.entities, [&](const EntityInText &entity) {
			return !entity.validForText(length);
		})) {
		LOG(("Nagram Error: Invalid text entities; spacing not applied."));
		return text;
	}
	auto protectedBoundaries = std::vector<int>(length + 1);
	const auto protect = [&](const EntitiesInText &entities) {
		for (const auto &entity : entities) {
			if (Protected(entity.type()) && entity.validForText(length)) {
				++protectedBoundaries[entity.offset() + 1];
				--protectedBoundaries[entity.offset() + entity.length()];
			}
		}
	};
	protect(text.entities);
	protect(TextUtilities::ParseEntities(
		text.text,
		TextParseLinks | TextParseMentions | TextParseHashtags | TextParseBotCommands
	).entities);
	auto codeStart = -1;
	auto codeTicks = 0;
	for (auto i = 0; i < length;) {
		if (text.text.at(i) == '\\' && codeStart < 0) {
			i += std::min(2, length - i);
		} else if (text.text.at(i) != '`') {
			++i;
		} else {
			const auto start = i;
			while (i < length && text.text.at(i) == '`') {
				++i;
			}
			if (codeStart < 0) {
				codeStart = start;
				codeTicks = i - start;
			} else if (i - start == codeTicks) {
				++protectedBoundaries[codeStart + 1];
				--protectedBoundaries[i];
				codeStart = -1;
			}
		}
	}
	if (codeStart >= 0) {
		++protectedBoundaries[codeStart + 1];
		--protectedBoundaries[length];
	}
	auto result = TextWithEntities();
	result.text.reserve(length);
	auto before = std::vector<int>(length + 1);
	auto after = std::vector<int>(length + 1);
	auto previous = char32_t(0);
	auto protectedDepth = 0;
	for (auto i = 0; i != length;) {
		protectedDepth += protectedBoundaries[i];
		const auto first = text.text.at(i);
		const auto surrogate = first.isHighSurrogate()
			&& i + 1 < length
			&& text.text.at(i + 1).isLowSurrogate();
		const auto ch = surrogate
			? QChar::surrogateToUcs4(first, text.text.at(i + 1))
			: char32_t(first.unicode());
		before[i] = result.text.size();
		if (!protectedDepth
			&& ((IsHan(previous) && IsLatinOrDigit(ch))
				|| (IsLatinOrDigit(previous) && IsHan(ch)))) {
			result.text += ' ';
		}
		after[i] = result.text.size();
		result.text += first;
		if (surrogate) {
			++i;
			protectedDepth += protectedBoundaries[i];
			before[i] = after[i] = result.text.size();
			result.text += text.text.at(i);
		}
		++i;
		previous = ch;
	}
	if (result.text == text.text) {
		return text;
	}
	before[length] = after[length] = result.text.size();
	for (const auto &entity : text.entities) {
		if (entity.validForText(length)) {
			const auto start = after[entity.offset()];
			const auto end = before[entity.offset() + entity.length()];
			result.entities.push_back(EntityInText(
				entity.type(),
				start,
				end - start,
				entity.data()));
		}
	}
	return result;
}

TextWithEntities PrepareText(
		Core::Settings &settings,
		const TextWithEntities &text,
		bool editing) {
	auto result = Get(settings, editing
		? Option::PanguOnEditing
		: Option::PanguOnSending)
		? AddTextSpacing(text)
		: text;
	const auto tools = TextTools(settings);
	const auto language = tools
		? tools->value(u"codeLanguage"_q).toString()
		: QString();
	if (!language.isEmpty()) {
		for (auto &entity : result.entities) {
			if (entity.type() == EntityType::Pre && entity.data().isEmpty()) {
				entity = EntityInText(
					entity.type(),
					entity.offset(),
					entity.length(),
					language);
			}
		}
	}
	return result;
}

} // namespace Nagram
