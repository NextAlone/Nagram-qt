#include "nagram/nagram_profile.h"

#include "data/data_peer.h"
#include "data/data_channel.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "main/session/session_show.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "styles/style_layers.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace Nagram {
namespace {

constexpr auto kMaximumAliases = 1000;
constexpr auto kMaximumAliasLength = 96;

} // namespace

QString FormatPeerId(PeerId id, bool raw) {
	const auto bare = qint64(peerToBareMTPInt(id).v);
	return QString::number(raw || peerIsUser(id)
		? bare : peerIsChat(id) ? -bare : -(1000000000000LL + bare));
}

bool ValidPeerAlias(const QString &value) {
	return value.size() <= kMaximumAliasLength
		&& ranges::none_of(value, [](QChar ch) {
			return ch.category() == QChar::Other_Control
				|| ch.category() == QChar::Separator_Line
				|| ch.category() == QChar::Separator_Paragraph;
		})
		&& value == value.trimmed()
		&& QString::fromUtf8(value.toUtf8()) == value;
}

std::optional<PeerAliases> ParsePeerAliases(const QByteArray &serialized) {
	if (serialized.isEmpty()) {
		return PeerAliases();
	} else if (serialized.size() > 512 * 1024) {
		return std::nullopt;
	}
	const auto document = QJsonDocument::fromJson(serialized);
	const auto object = document.object();
	const auto names = object.value(u"names"_q);
	if (!document.isObject()
		|| object.size() != 2
		|| object.value(u"version"_q) != QJsonValue(1)
		|| !names.isObject()
		|| names.toObject().size() > kMaximumAliases) {
		return std::nullopt;
	}
	auto result = PeerAliases();
	const auto entries = names.toObject();
	for (auto i = entries.begin(); i != entries.end(); ++i) {
		auto ok = false;
		const auto serializedId = i.key().toULongLong(&ok);
		const auto id = DeserializePeerId(serializedId);
		const auto value = i.value().toString();
		if (!ok || !id
			|| (!peerIsUser(id) && !peerIsChat(id) && !peerIsChannel(id))
			|| peerToBareMTPInt(id).v <= 0
			|| QString::number(SerializePeerId(id)) != i.key()
			|| !i.value().isString() || value.isEmpty()
			|| !ValidPeerAlias(value)) {
			return std::nullopt;
		}
		result.emplace(id, value);
	}
	return result;
}

QByteArray SerializePeerAliases(const PeerAliases &aliases) {
	if (aliases.empty()) {
		return {};
	}
	auto names = QJsonObject();
	for (const auto &[id, value] : aliases) {
		names.insert(QString::number(SerializePeerId(id)), value);
	}
	return QJsonDocument(QJsonObject{
		{ u"version"_q, 1 },
		{ u"names"_q, names },
	}).toJson(QJsonDocument::Compact);
}

const QString &PeerAlias(not_null<const PeerData*> peer) {
	const auto &aliases = peer->session().settings().localAliases();
	const auto i = aliases.find(peer->id);
	static const auto empty = QString();
	return (i != aliases.end()) ? i->second : empty;
}

const QString &PeerDisplayName(not_null<const PeerData*> peer) {
	if (const auto to = peer->migrateTo()) {
		return PeerDisplayName(to);
	} else if (const auto broadcast = peer->monoforumBroadcast()) {
		return PeerDisplayName(broadcast);
	}
	const auto &alias = PeerAlias(peer);
	return alias.isEmpty() ? peer->name() : alias;
}

QString SetPeerAlias(
		not_null<PeerData*> peer,
		const QString &value,
		const QString &expected) {
	auto &settings = peer->session().settings();
	if (!settings.localAliasesValid() || !ValidPeerAlias(value)) {
		return tr::lng_nagram_alias_invalid(tr::now);
	} else if (PeerAlias(peer) != expected) {
		return tr::lng_nagram_alias_changed(tr::now);
	}
	auto aliases = settings.localAliases();
	if (value.isEmpty()) {
		aliases.remove(peer->id);
	} else {
		aliases[peer->id] = value;
	}
	if (aliases.size() > kMaximumAliases
		|| !settings.setLocalAliases(SerializePeerAliases(aliases))) {
		return tr::lng_nagram_alias_invalid(tr::now);
	}
	peer->localNameChanged();
	peer->session().saveSettings();
	return {};
}

void ShowPeerAlias(
		std::shared_ptr<Main::SessionShow> show,
		not_null<PeerData*> peer) {
	show->showBox(Box([=](not_null<Ui::GenericBox*> box) {

		box->setTitle(tr::lng_nagram_peer_alias());
		const auto original = PeerAlias(peer);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(peer->name()),
			st::boxLabel));
		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			Ui::InputField::Mode::SingleLine,
			tr::lng_nagram_peer_alias(),
			original));
		field->setMaxLength(kMaximumAliasLength);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_nagram_alias_about(),
			st::boxLabel));
		box->addButton(tr::lng_settings_save(), [=] {
			const auto error = SetPeerAlias(peer, field->getLastText(), original);
			if (!error.isEmpty()) {
				box->showToast(error);
				field->showError();
				return;
			}
			box->closeBox();
		});
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

} // namespace Nagram
