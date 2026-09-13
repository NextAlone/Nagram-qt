/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "api/api_transcribes.h"

#include "apiwrap.h"
#include "api/api_text_entities.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_channel.h"
#include "data/data_document.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "lang/lang_keys.h"
#include "main/main_app_config.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "nagram/nagram_services.h"
#include "spellcheck/spellcheck_types.h"

namespace Api {
namespace {

bool ExternalTranscriptionSelected() {
	const auto config = Nagram::Services(Core::App().settings());
	if (!config) {
		return true;
	}
	const auto id = config->value(u"transcription"_q).toString();
	return !id.isEmpty() && id != u"telegram"_q;
}

} // namespace

Transcribes::Transcribes(not_null<ApiWrap*> api)
: _session(&api->session())
, _api(&api->instance()) {
	_externalConfig = Core::App().settings().readPref<QByteArray>(
		Nagram::kServicesKey);
	_externalSelected = ExternalTranscriptionSelected();
	Core::App().settings().saveDelayedRequests(
	) | rpl::on_next([=] {
		const auto config = Core::App().settings().readPref<QByteArray>(
			Nagram::kServicesKey);
		if (_externalConfig != config) {
			_externalConfig = config;
			_externalSelected = ExternalTranscriptionSelected();
			clearExternal();
		}
	}, _externalLifetime);
}

bool Transcribes::isRated(not_null<HistoryItem*> item) const {
	if (external(item)) {
		return true;
	}
	const auto fullId = item->fullId();
	for (const auto &[transcribeId, id] : _ids) {
		if (id == fullId) {
			return _session->settings().isTranscriptionRated(transcribeId);
		}
	}
	return false;
}

void Transcribes::rate(not_null<HistoryItem*> item, bool isGood) {
	if (external(item)) {
		return;
	}
	const auto fullId = item->fullId();
	for (const auto &[transcribeId, id] : _ids) {
		if (id == fullId) {
			_api.request(MTPmessages_RateTranscribedAudio(
				item->history()->peer->input(),
				MTP_int(item->id),
				MTP_long(transcribeId),
				MTP_bool(isGood))).send();
			_session->settings().markTranscriptionAsRated(transcribeId);
			_session->saveSettings();
			return;
		}
	}
}

bool Transcribes::freeFor(not_null<HistoryItem*> item) const {
	if (const auto channel = item->history()->peer->asMegagroup()) {
		const auto owner = &channel->owner();
		return channel->levelHint() >= owner->groupFreeTranscribeLevel();
	}
	return false;
}

bool Transcribes::trialsSupport() {
	if (!_trialsSupport) {
		const auto count = _session->appConfig().get<int>(
			u"transcribe_audio_trial_weekly_number"_q,
			0);
		const auto until = _session->appConfig().get<int>(
			u"transcribe_audio_trial_cooldown_until"_q,
			0);
		_trialsSupport = (count > 0) || (until > 0);
	}
	return *_trialsSupport;
}

TimeId Transcribes::trialsRefreshAt() {
	if (_trialsRefreshAt < 0) {
		_trialsRefreshAt = _session->appConfig().get<int>(
			u"transcribe_audio_trial_cooldown_until"_q,
			0);
	}
	return _trialsRefreshAt;
}

int Transcribes::trialsCount() {
	if (_trialsCount < 0) {
		_trialsCount = _session->appConfig().get<int>(
			u"transcribe_audio_trial_weekly_number"_q,
			-1);
		return std::max(_trialsCount, 0);
	}
	return _trialsCount;
}

crl::time Transcribes::trialsMaxLengthMs() const {
	return 1000 * _session->appConfig().get<int>(
		u"transcribe_audio_trial_duration_max"_q,
		300);
}

void Transcribes::toggle(not_null<HistoryItem*> item) {
	const auto id = item->fullId();
	auto i = _map.find(id);
	if (i == _map.end()) {
		load(item);
		_session->data().requestItemResize(item);
	} else if (!i->second.requestId) {
		i->second.shown = !i->second.shown;
		if (i->second.roundview) {
			_session->data().requestItemViewRefresh(item);
		}
		_session->data().requestItemResize(item);
	}
}

void Transcribes::toggleSummary(not_null<HistoryItem*> item) {
	const auto id = item->fullId();
	auto i = _summaries.find(id);
	if (i == _summaries.end()) {
		summarize(item);
	} else if (!i->second.loading) {
		auto &entry = i->second;
		if (entry.result.empty()) {
			summarize(item);
		} else {
			entry.shown = entry.premiumRequired ? false : !entry.shown;
			_session->data().requestItemResize(item);
			if (entry.shown) {
				_session->data().requestItemShowHighlight(item);
			}
		}
	}
}

const Transcribes::Entry &Transcribes::entry(
		not_null<HistoryItem*> item) const {
	if (const auto cached = external(item)) {
		return cached->value;
	}
	static auto empty = Entry();
	if (_externalSelected) {
		return empty;
	}
	const auto i = _map.find(item->fullId());
	return (i != _map.end()) ? i->second : empty;
}

auto Transcribes::external(not_null<HistoryItem*> item) const
-> const ExternalEntry* {
	const auto i = _external.find(item->fullId());
	const auto media = item->media();
	const auto document = media ? media->document() : nullptr;
	return (i != _external.end()
		&& _externalSelected
		&& document
		&& !media->ttlSeconds()
		&& document->id == i->second.documentId)
		? &i->second
		: nullptr;
}

void Transcribes::refreshExternal(FullMsgId id) {
	if (const auto item = _session->data().message(id)) {
		_session->data().requestItemViewRefresh(item);
		_session->data().requestItemResize(item);
	}
}

bool Transcribes::toggleExternal(not_null<HistoryItem*> item) {
	if (!external(item)) {
		return false;
	}
	auto &cached = _external[item->fullId()];
	cached.value.shown = !cached.value.shown;
	cached.accessed = ++_externalAccessed;
	refreshExternal(item->fullId());
	return true;
}

bool Transcribes::setExternal(
		not_null<HistoryItem*> item,
		DocumentId documentId,
		const QByteArray &serviceConfig,
		uint64 generation,
		QString result) {
	const auto media = item->media();
	const auto document = media ? media->document() : nullptr;
	constexpr auto kMaximumText = 16384;
	if (&item->history()->session() != _session
		|| !_externalSelected
		|| !document
		|| document->id != documentId
		|| (!document->isVoiceMessage() && !document->isVideoMessage())
		|| media->ttlSeconds()
		|| serviceConfig != _externalConfig
		|| generation != externalGeneration(document->isVideoMessage())
		|| result.isEmpty()
		|| result.size() > kMaximumText
		|| result.contains(QChar(0))
		|| QString::fromUtf8(result.toUtf8()) != result) {
		return false;
	}
	_external[item->fullId()] = {
		.documentId = documentId,
		.value = {
			.result = std::move(result),
			.shown = true,
			.roundview = document->isVideoMessage(),
		},
		.accessed = ++_externalAccessed,
	};
	constexpr auto kMaximumEntries = 128;
	if (_external.size() > kMaximumEntries) {
		const auto oldest = ranges::min_element(
			_external,
			ranges::less(),
			[](const auto &entry) { return entry.second.accessed; });
		const auto id = oldest->first;
		_external.erase(oldest);
		refreshExternal(id);
	}
	refreshExternal(item->fullId());
	return true;
}

void Transcribes::clearExternal(std::optional<bool> round) {
	for (auto i = 0; i != _externalGeneration.size(); ++i) {
		if (!round || *round == bool(i)) {
			++_externalGeneration[i];
		}
	}
	auto removed = std::vector<FullMsgId>();
	for (auto i = _external.begin(); i != _external.end();) {
		if (!round || i->second.value.roundview == *round) {
			removed.push_back(i->first);
			i = _external.erase(i);
		} else {
			++i;
		}
	}
	for (const auto id : removed) {
		refreshExternal(id);
	}
}

void Transcribes::removeExternal(FullMsgId id) {
	_external.remove(id);
}

const SummaryEntry &Transcribes::summary(
		not_null<const HistoryItem*> item) const {
	static const auto empty = SummaryEntry();
	const auto i = _summaries.find(item->fullId());
	return (i != _summaries.end()) ? i->second : empty;
}

void Transcribes::apply(const MTPDupdateTranscribedAudio &update) {
	const auto id = update.vtranscription_id().v;
	const auto i = _ids.find(id);
	if (i == _ids.end()) {
		return;
	}
	const auto j = _map.find(i->second);
	if (j == _map.end()) {
		return;
	}
	const auto text = qs(update.vtext());
	j->second.result = text;
	j->second.pending = update.is_pending();
	if (const auto item = _session->data().message(i->second)) {
		if (j->second.roundview) {
			_session->data().requestItemViewRefresh(item);
		}
		_session->data().requestItemResize(item);
	}
}

void Transcribes::load(not_null<HistoryItem*> item) {
	if (!item->isHistoryEntry() || item->isLocal()) {
		return;
	}
	const auto toggleRound = [](not_null<HistoryItem*> item, Entry &entry) {
		if (const auto media = item->media()) {
			if (const auto document = media->document()) {
				if (document->isVideoMessage()) {
					entry.roundview = true;
					document->owner().requestItemViewRefresh(item);
				}
			}
		}
	};
	const auto id = item->fullId();
	const auto requestId = _api.request(MTPmessages_TranscribeAudio(
		item->history()->peer->input(),
		MTP_int(item->id)
	)).done([=](const MTPmessages_TranscribedAudio &result) {
		const auto &data = result.data();

		{
			const auto trialsCountChanged = data.vtrial_remains_num()
				&& (_trialsCount != data.vtrial_remains_num()->v);
			if (trialsCountChanged) {
				_trialsCount = data.vtrial_remains_num()->v;
			}
			const auto refreshAtChanged = data.vtrial_remains_until_date()
				&& (_trialsRefreshAt != data.vtrial_remains_until_date()->v);
			if (refreshAtChanged) {
				_trialsRefreshAt = data.vtrial_remains_until_date()->v;
			}
			if (trialsCountChanged) {
				ShowTrialTranscribesToast(_trialsCount, _trialsRefreshAt);
			}
		}

		auto &entry = _map[id];
		entry.requestId = 0;
		entry.pending = data.is_pending();
		entry.result = qs(data.vtext());
		_ids.emplace(data.vtranscription_id().v, id);
		if (const auto item = _session->data().message(id)) {
			toggleRound(item, entry);
			_session->data().requestItemResize(item);
		}
	}).fail([=](const MTP::Error &error) {
		auto &entry = _map[id];
		entry.requestId = 0;
		entry.pending = false;
		entry.failed = true;
		if (error.type() == u"MSG_VOICE_TOO_LONG"_q) {
			entry.toolong = true;
		}
		if (const auto item = _session->data().message(id)) {
			toggleRound(item, entry);
			_session->data().requestItemResize(item);
		}
	}).send();
	auto &entry = _map.emplace(id).first->second;
	entry.requestId = requestId;
	entry.shown = true;
	entry.failed = false;
	entry.pending = false;
}

void Transcribes::summarize(not_null<HistoryItem*> item) {
	if (!item->isHistoryEntry() || item->isLocal()) {
		return;
	}

	const auto id = item->fullId();
	const auto translatedTo = item->history()->translatedTo();
	const auto langCode = translatedTo
		? translatedTo.twoLetterCode()
		: QString();
	const auto requestId = _api.request(MTPmessages_SummarizeText(
		langCode.isEmpty()
			? MTP_flags(0)
			: MTP_flags(MTPmessages_summarizeText::Flag::f_to_lang),
		item->history()->peer->input(),
		MTP_int(item->id),
		langCode.isEmpty() ? MTPstring() : MTP_string(langCode),
		MTPstring() // tone
	)).done([=](const MTPTextWithEntities &result) {
		const auto &data = result.data();
		auto &entry = _summaries[id];
		entry.requestId = 0;
		entry.loading = false;
		entry.premiumRequired = false;
		entry.languageId = translatedTo;
		entry.result = TextWithEntities(
			qs(data.vtext()),
			Api::EntitiesFromMTP(_session, data.ventities().v));
		if (const auto item = _session->data().message(id)) {
			_session->data().requestItemTextRefresh(item);
			_session->data().requestItemShowHighlight(item);
		}
	}).fail([=](const MTP::Error &error) {
		auto &entry = _summaries[id];
		if (error.type() == u"SUMMARY_FLOOD_PREMIUM"_q) {
			entry.premiumRequired = true;
		}
		entry.requestId = 0;
		entry.shown = false;
		entry.loading = false;
		if (const auto item = _session->data().message(id)) {
			_session->data().requestItemTextRefresh(item);
		}
	}).send();

	auto &entry = _summaries.emplace(id).first->second;
	entry.requestId = requestId;
	entry.shown = true;
	entry.loading = true;

	item->setHasSummaryEntry();
	_session->data().requestItemResize(item);
}

void Transcribes::checkSummaryToTranslate(FullMsgId id) {
	const auto i = _summaries.find(id);
	if (i == _summaries.end() || i->second.result.empty()) {
		return;
	}
	const auto item = _session->data().message(id);
	if (!item) {
		return;
	}
	const auto translatedTo = item->history()->translatedTo();
	if (i->second.languageId != translatedTo) {
		i->second.result = tr::lng_contacts_loading(tr::now, tr::italic);
		summarize(item);
	}
}

} // namespace Api
