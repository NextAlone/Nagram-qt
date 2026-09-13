/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/sender.h"
#include "spellcheck/spellcheck_types.h"

class ApiWrap;

namespace Main {
class Session;
} // namespace Main

namespace Api {

struct SummaryEntry {
	TextWithEntities result;
	LanguageId languageId;
	bool shown = false;
	bool loading = false;
	bool premiumRequired = false;
	mtpRequestId requestId = 0;
};

class Transcribes final {
public:
	explicit Transcribes(not_null<ApiWrap*> api);

	struct Entry {
		QString result;
		bool shown = false;
		bool failed = false;
		bool toolong = false;
		bool pending = false;
		bool roundview = false;
		mtpRequestId requestId = 0;
	};

	void toggle(not_null<HistoryItem*> item);
	[[nodiscard]] const Entry &entry(not_null<HistoryItem*> item) const;
	[[nodiscard]] bool toggleExternal(not_null<HistoryItem*> item);
	[[nodiscard]] bool setExternal(
		not_null<HistoryItem*> item,
		DocumentId documentId,
		const QByteArray &serviceConfig,
		uint64 generation,
		QString result);
	[[nodiscard]] uint64 externalGeneration(bool round) const {
		return _externalGeneration[round ? 1 : 0];
	}
	void clearExternal(std::optional<bool> round = std::nullopt);
	void removeExternal(FullMsgId id);

	void toggleSummary(not_null<HistoryItem*> item);
	[[nodiscard]] const SummaryEntry &summary(
		not_null<const HistoryItem*> item) const;
	void checkSummaryToTranslate(FullMsgId id);

	void apply(const MTPDupdateTranscribedAudio &update);

	[[nodiscard]] bool freeFor(not_null<HistoryItem*> item) const;
	[[nodiscard]] bool isRated(not_null<HistoryItem*> item) const;
	void rate(not_null<HistoryItem*> item, bool isGood);

	[[nodiscard]] bool trialsSupport();
	[[nodiscard]] TimeId trialsRefreshAt();
	[[nodiscard]] int trialsCount();
	[[nodiscard]] crl::time trialsMaxLengthMs() const;

private:
	struct ExternalEntry {
		DocumentId documentId = 0;
		Entry value;
		uint64 accessed = 0;
	};
	[[nodiscard]] const ExternalEntry *external(
		not_null<HistoryItem*> item) const;
	void refreshExternal(FullMsgId id);
	void load(not_null<HistoryItem*> item);
	void summarize(not_null<HistoryItem*> item);

	const not_null<Main::Session*> _session;
	MTP::Sender _api;

	int _trialsCount = -1;
	std::optional<bool> _trialsSupport;
	TimeId _trialsRefreshAt = -1;

	base::flat_map<FullMsgId, Entry> _map;
	base::flat_map<uint64, FullMsgId> _ids;
	base::flat_map<FullMsgId, ExternalEntry> _external;
	QByteArray _externalConfig;
	uint64 _externalAccessed = 0;
	std::array<uint64, 2> _externalGeneration = {};
	bool _externalSelected = false;
	rpl::lifetime _externalLifetime;

	base::flat_map<FullMsgId, SummaryEntry> _summaries;

};

} // namespace Api
