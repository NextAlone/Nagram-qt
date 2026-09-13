#pragma once

class PeerData;
namespace Main { class SessionShow; }

namespace Nagram {

[[nodiscard]] QString FormatPeerId(PeerId id, bool raw);

using PeerAliases = base::flat_map<PeerId, QString>;

[[nodiscard]] bool ValidPeerAlias(const QString &value);
[[nodiscard]] std::optional<PeerAliases> ParsePeerAliases(
	const QByteArray &serialized);
[[nodiscard]] QByteArray SerializePeerAliases(const PeerAliases &aliases);
[[nodiscard]] const QString &PeerAlias(not_null<const PeerData*> peer);
[[nodiscard]] const QString &PeerDisplayName(not_null<const PeerData*> peer);
[[nodiscard]] QString SetPeerAlias(
	not_null<PeerData*> peer,
	const QString &value,
	const QString &expected);
void ShowPeerAlias(
	std::shared_ptr<Main::SessionShow> show,
	not_null<PeerData*> peer);

} // namespace Nagram
