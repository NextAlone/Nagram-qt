#include "nagram/nagram_settings.h"

#include "core/core_settings.h"
#include "data/data_peer.h"

#include <QtGui/QFontDatabase>

namespace Nagram {
namespace {

const OptionDefinition &Definition(Option option) {
	const auto index = static_cast<std::size_t>(option);
	Expects(index < kOptions.size());
	return kOptions[index];
}

} // namespace

bool Get(Core::Settings &settings, Option option) {
	const auto &definition = Definition(option);
	return settings.readPref<bool>(definition.key, definition.defaultValue);
}

rpl::producer<bool> Value(Core::Settings &settings, Option option) {
	return rpl::single(rpl::empty) | rpl::then(
		settings.saveDelayedRequests()
	) | rpl::map([&settings, option] {
		return Get(settings, option);
	}) | rpl::distinct_until_changed();
}

void Set(Core::Settings &settings, Option option, bool value) {
	const auto &definition = Definition(option);
	if (value == definition.defaultValue) {
		settings.clearPref(definition.key);
	} else {
		settings.writePref<bool>(definition.key, value);
	}
}

Core::WindowTitleContent WindowTitleOptions(Core::Settings &settings) {
	return Get(settings, Option::PresentationMode)
		? Core::WindowTitleContent{
			.hideChatName = true,
			.hideAccountName = true,
			.hideTotalUnread = true,
		}
		: settings.windowTitleContent();
}

bool ValidMonospaceFont(const QString &family) {
	return family.size() <= 128
		&& family == family.trimmed()
		&& family == QString::fromUtf8(family.toUtf8())
		&& ranges::none_of(family, [](QChar ch) {
			return ch.category() == QChar::Other_Control
				|| ch.category() == QChar::Separator_Line
				|| ch.category() == QChar::Separator_Paragraph;
		});
}

bool MonospaceFontAvailable(const QString &family) {
	return ValidMonospaceFont(family) && (family.isEmpty()
		|| (QFontDatabase::families().contains(family)
			&& QFontDatabase::isFixedPitch(family)));
}

QString MonospaceFont(Core::Settings &settings) {
	return QString::fromUtf8(settings.readPref<QByteArray>(kMonospaceFontKey));
}

void SetMonospaceFont(Core::Settings &settings, const QString &family) {
	Expects(MonospaceFontAvailable(family));
	if (family.isEmpty()) {
		settings.clearPref(kMonospaceFontKey);
	} else {
		settings.writePref<QByteArray>(kMonospaceFontKey, family.toUtf8());
	}
}

bool ReactionsHidden(
		Core::Settings &settings,
		not_null<const PeerData*> peer) {
	return Get(settings, Option::HideReactions)
		|| Get(settings, peer->isUser()
			? Option::HidePrivateReactions
			: peer->isBroadcast()
			? Option::HideChannelReactions
			: Option::HideGroupReactions);
}

int RoundnessValue(Core::Settings &settings, Roundness target) {
	const auto key = (target == Roundness::Bubbles)
		? kBubbleRoundnessKey
		: kAvatarRoundnessKey;
	const auto bytes = settings.readPref<QByteArray>(key);
	if (bytes.isEmpty()) {
		return 0;
	}
	auto ok = false;
	const auto value = bytes.toInt(&ok);
	if (ok && (!value || (value >= 10 && value <= 100))) {
		return value;
	}
	LOG(("Nagram Error: Invalid roundness preference; using inherited value."));
	return 0;
}

rpl::producer<int> RoundnessChanges(
		Core::Settings &settings,
		Roundness target) {
	return rpl::single(rpl::empty) | rpl::then(
		settings.saveDelayedRequests()
	) | rpl::map([&settings, target] {
		return RoundnessValue(settings, target);
	}) | rpl::distinct_until_changed();
}

void SetRoundness(Core::Settings &settings, Roundness target, int value) {
	Expects(!value || (value >= 10 && value <= 100));
	const auto key = (target == Roundness::Bubbles)
		? kBubbleRoundnessKey
		: kAvatarRoundnessKey;
	if (!value) {
		settings.clearPref(key);
	} else {
		settings.writePref<QByteArray>(key, QByteArray::number(value));
	}
}

int NotificationDelay(Core::Settings &settings, NotificationTiming timing) {
	const auto key = (timing == NotificationTiming::OtherDevice)
		? kCloudNotificationDelayKey
		: kNotificationDelayKey;
	const auto bytes = settings.readPref<QByteArray>(key);
	if (bytes.isEmpty()) {
		return 0;
	}
	auto ok = false;
	const auto value = bytes.toInt(&ok);
	if (ok && value >= 0 && value <= kMaxNotificationDelay) {
		return value;
	}
	LOG(("Nagram Error: Invalid %1 preference; using inherited notification timing."
		).arg(QString::fromUtf8(key.data(), key.size())));
	return 0;
}

rpl::producer<int> NotificationDelayValue(
		Core::Settings &settings,
		NotificationTiming timing) {
	return rpl::single(rpl::empty) | rpl::then(
		settings.saveDelayedRequests()
	) | rpl::map([&settings, timing] {
		return NotificationDelay(settings, timing);
	}) | rpl::distinct_until_changed();
}

void SetNotificationDelay(
		Core::Settings &settings,
		NotificationTiming timing,
		int value) {
	Expects(value >= 0 && value <= kMaxNotificationDelay);
	const auto key = (timing == NotificationTiming::OtherDevice)
		? kCloudNotificationDelayKey
		: kNotificationDelayKey;
	if (!value) {
		settings.clearPref(key);
	} else {
		settings.writePref<QByteArray>(key, QByteArray::number(value));
	}
}

crl::time ApplyNotificationDelay(
		Core::Settings &settings,
		NotificationTiming timing,
		crl::time nativeDelay,
		crl::time minimum) {
	const auto delay = NotificationDelay(settings, timing);
	return delay ? std::max<crl::time>(delay, minimum) : nativeDelay;
}

int MessageWidth(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kMessageWidthKey);
	if (bytes.isEmpty()) {
		return 0;
	}
	auto ok = false;
	const auto value = bytes.toInt(&ok);
	if (ok && (!value || (value >= 50 && value <= 400))) {
		return value;
	}
	LOG(("Nagram Error: Invalid messageWidth preference; using inherited value."));
	return 0;
}

rpl::producer<int> MessageWidthValue(Core::Settings &settings) {
	return rpl::single(rpl::empty) | rpl::then(
		settings.saveDelayedRequests()
	) | rpl::map([&settings] {
		return MessageWidth(settings);
	}) | rpl::distinct_until_changed();
}

void SetMessageWidth(Core::Settings &settings, int value) {
	Expects(!value || (value >= 50 && value <= 400));
	if (!value) {
		settings.clearPref(kMessageWidthKey);
	} else {
		settings.writePref<QByteArray>(kMessageWidthKey, QByteArray::number(value));
	}
}

int StickerScale(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kStickerScaleKey);
	if (bytes.isEmpty()) {
		return 100;
	}
	const auto value = bytes.toInt();
	if (ranges::contains(kStickerScales, value)) {
		return value;
	}
	LOG(("Nagram Error: Invalid stickerScale preference; using 100 percent."));
	return 100;
}

rpl::producer<int> StickerScaleValue(Core::Settings &settings) {
	return rpl::single(rpl::empty) | rpl::then(
		settings.saveDelayedRequests()
	) | rpl::map([&settings] {
		return StickerScale(settings);
	}) | rpl::distinct_until_changed();
}

void SetStickerScale(Core::Settings &settings, int value) {
	Expects(ranges::contains(kStickerScales, value));
	if (value == 100) {
		settings.clearPref(kStickerScaleKey);
	} else {
		settings.writePref<QByteArray>(kStickerScaleKey, QByteArray::number(value));
	}
}

int ChatPreviewLines(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kChatPreviewLinesKey);
	if (bytes.isEmpty()) {
		return 0;
	}
	auto ok = false;
	const auto value = bytes.toInt(&ok);
	if (ok && value >= 0 && value <= 3) {
		return value;
	}
	LOG(("Nagram Error: Invalid chatPreviewLines preference; using inherited value."));
	return 0;
}

rpl::producer<int> ChatPreviewLinesValue(Core::Settings &settings) {
	return rpl::single(rpl::empty) | rpl::then(
		settings.saveDelayedRequests()
	) | rpl::map([&settings] {
		return ChatPreviewLines(settings);
	}) | rpl::distinct_until_changed();
}

void SetChatPreviewLines(Core::Settings &settings, int value) {
	Expects(value >= 0 && value <= 3);
	if (!value) {
		settings.clearPref(kChatPreviewLinesKey);
	} else {
		settings.writePref<QByteArray>(kChatPreviewLinesKey, QByteArray::number(value));
	}
}

int RecentStickerLimit(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kRecentStickerLimitKey);
	if (bytes.isEmpty()) {
		return 0;
	}
	auto ok = false;
	const auto value = bytes.toInt(&ok);
	if (ok && value >= 0 && value <= 200) {
		return value;
	}
	LOG(("Nagram Error: Invalid recentStickerLimit preference; using inherited value."));
	return 0;
}

rpl::producer<int> RecentStickerLimitValue(Core::Settings &settings) {
	return rpl::single(rpl::empty) | rpl::then(
		settings.saveDelayedRequests()
	) | rpl::map([&settings] {
		return RecentStickerLimit(settings);
	}) | rpl::distinct_until_changed();
}

void SetRecentStickerLimit(Core::Settings &settings, int value) {
	Expects(value >= 0 && value <= 200);
	if (!value) {
		settings.clearPref(kRecentStickerLimitKey);
	} else {
		settings.writePref<QByteArray>(kRecentStickerLimitKey, QByteArray::number(value));
	}
}

QString EditedMark(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kEditedMarkKey);
	const auto text = QString::fromUtf8(bytes);
	if (text.toUtf8() != bytes || text.contains('\n') || text.contains('\r')) {
		LOG(("Nagram Error: Invalid editedMark preference; using inherited value."));
		return {};
	}
	return text;
}

rpl::producer<QString> EditedMarkValue(Core::Settings &settings) {
	return rpl::single(rpl::empty) | rpl::then(
		settings.saveDelayedRequests()
	) | rpl::map([&settings] {
		return EditedMark(settings);
	}) | rpl::distinct_until_changed();
}

void SetEditedMark(Core::Settings &settings, const QString &value) {
	Expects(!value.contains('\n') && !value.contains('\r'));
	if (value.isEmpty()) {
		settings.clearPref(kEditedMarkKey);
	} else {
		settings.writePref<QByteArray>(kEditedMarkKey, value.toUtf8());
	}
}

} // namespace Nagram
