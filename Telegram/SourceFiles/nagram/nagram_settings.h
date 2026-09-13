#pragma once

#include <rpl/producer.h>
#include <crl/crl_time.h>

#include <array>
#include <string_view>

class PeerData;

namespace Core {
class Settings;
struct WindowTitleContent;
} // namespace Core

namespace Nagram {

enum class Category {
	Appearance,
	Chats,
	Media,
	Privacy,
	Translation,
	Advanced,
};

enum class Option {
	CompactChatList,
	HideStories,
	HideReactions,
	WideChannelPosts,
	SecondsInMessages,
	ShowForwardedMessageDate,
	DisableScrollToNextChannel,
	DisableScrollToNextTopic,
	ConfirmCalls,
	ShowProfileId,
	ShowDc,
	HideRecordingButton,
	HideStickerTimestamp,
	DisableVideoAutoplay,
	HidePhoneNumber,
	HidePrivateChatActivities,
	TranslateBeforeSend,
	AutoTranslate,
	PanguOnSending,
	PanguOnEditing,
	RegexFilters,
	ShowMessageId,
	HideQuickShare,
	HideBotMenu,
	DisableEmojiHover,
	DisableAttachHover,
	BotCommandsToDraft,
	HideEditedBadge,
	ExactMessageCounters,
	HideMessageViews,
	HideChannelSignature,
	HideFolderUnreadCounters,
	HideGiftButton,
	HideBotCommandButton,
	HideAutoDeleteButton,
	DisablePremiumStickerEffects,
	DisableEmojiInteractions,
	DisableMessageEffects,
	HideMenuPin,
	HideMenuReport,
	HideMenuBlock,
	HideMenuStatistics,
	HideMenuCopyLink,
	HideMenuForward,
	HideMenuTranslate,
	HideMenuSelect,
	HideMenuEmojiPacks,
	ConfirmStickers,
	ConfirmGifs,
	PreviewVoiceMessages,
	PreviewVideoMessages,
	HideSponsoredMessages,
	HideRecommendedChannels,
	HidePremiumBadges,
	HideChannelBottomButton,
	HideSendAsButton,
	HideAttachButton,
	HideEmojiButton,
	HideAiComposeButton,
	HideGreetingSticker,
	ForwardBeforeComment,
	HideAllChatsFolder,
	ShowArchiveInFolders,
	HideSavedAndArchivedPreviews,
	HidePremiumPromotions,
	HideBirthdaySuggestions,
	HideFeaturedStickers,
	HideFeaturedEmoji,
	HideGifShortcuts,
	HideSavedTags,
	HidePhoneSharePrompt,
	HideQuickStars,
	HideProfileGifts,
	HideProxySponsor,
	ShowMediaDetails,
	ShowServiceTime,
	HideReadTime,
	RevealSpoilers,
	HideReactionMenu,
	HideReactionMenuWhenSelecting,
	HideCreateTodo,
	HidePrivateReactions,
	HideGroupReactions,
	HideChannelReactions,
	HideBubbleTail,
	SimpleQuotesAndReplies,
	HideReplyThumbnails,
	DisableMarkdown,
	DisableLinkPreview,
	HideGroupStickers,
	PanguOnReading,
	PresentationMode,
	IgnorePeerThemes,
	PreferSystemAi,
	NarrowInterfaceSymbols,
	GifPlaybackControls,
	Mp4FilePreview,
	HideApplicationBadge,
	UniformAvatarShapes,
	RawProfileId,
	Count,
};

struct OptionDefinition {
	Option option;
	std::string_view key;
	Category category;
	bool defaultValue = false;
};

inline constexpr auto kOptions = std::array{
	OptionDefinition{ Option::CompactChatList, "nagram.chatListCompact", Category::Appearance },
	OptionDefinition{ Option::HideStories, "nagram.hideStories", Category::Appearance },
	OptionDefinition{ Option::HideReactions, "nagram.hideReactions", Category::Appearance },
	OptionDefinition{ Option::WideChannelPosts, "nagram.wideChannelPosts", Category::Appearance },
	OptionDefinition{ Option::SecondsInMessages, "nagram.secondsInMessages", Category::Chats },
	OptionDefinition{ Option::ShowForwardedMessageDate, "nagram.showForwardedMessageDate", Category::Chats },
	OptionDefinition{ Option::DisableScrollToNextChannel, "nagram.disableScrollToNextChannel", Category::Chats },
	OptionDefinition{ Option::DisableScrollToNextTopic, "nagram.disableScrollToNextTopic", Category::Chats },
	OptionDefinition{ Option::ConfirmCalls, "nagram.confirmCalls", Category::Chats },
	OptionDefinition{ Option::ShowProfileId, "nagram.showProfileId", Category::Chats },
	OptionDefinition{ Option::ShowDc, "nagram.showDC", Category::Chats },
	OptionDefinition{ Option::HideRecordingButton, "nagram.hideRecordingButton", Category::Media },
	OptionDefinition{ Option::HideStickerTimestamp, "nagram.hideStickerTimestamp", Category::Media },
	OptionDefinition{ Option::DisableVideoAutoplay, "nagram.disableVideoAutoplay", Category::Media },
	OptionDefinition{ Option::HidePhoneNumber, "nagram.hidePhoneNumber", Category::Privacy },
	OptionDefinition{ Option::HidePrivateChatActivities, "nagram.hidePrivateChatActivities", Category::Privacy },
	OptionDefinition{ Option::TranslateBeforeSend, "nagram.translateBeforeSend", Category::Translation },
	OptionDefinition{ Option::AutoTranslate, "nagram.autoTranslate", Category::Translation },
	OptionDefinition{ Option::PanguOnSending, "nagram.enablePanguOnSending", Category::Advanced },
	OptionDefinition{ Option::PanguOnEditing, "nagram.enablePanguOnEditing", Category::Advanced },
	OptionDefinition{ Option::RegexFilters, "nagram.regexFiltersEnabled", Category::Advanced },
	OptionDefinition{ Option::ShowMessageId, "nagram.showMessageId", Category::Chats },
	OptionDefinition{ Option::HideQuickShare, "nagram.hideQuickShare", Category::Appearance },
	OptionDefinition{ Option::HideBotMenu, "nagram.hideBotMenu", Category::Chats },
	OptionDefinition{ Option::DisableEmojiHover, "nagram.disableEmojiHover", Category::Chats },
	OptionDefinition{ Option::DisableAttachHover, "nagram.disableAttachHover", Category::Chats },
	OptionDefinition{ Option::BotCommandsToDraft, "nagram.botCommandsToDraft", Category::Chats },
	OptionDefinition{ Option::HideEditedBadge, "nagram.hideEditedBadge", Category::Appearance },
	OptionDefinition{ Option::ExactMessageCounters, "nagram.exactMessageCounters", Category::Appearance },
	OptionDefinition{ Option::HideMessageViews, "nagram.hideMessageViews", Category::Appearance },
	OptionDefinition{ Option::HideChannelSignature, "nagram.hideChannelSignature", Category::Appearance },
	OptionDefinition{ Option::HideFolderUnreadCounters, "nagram.hideFolderUnreadCounters", Category::Appearance },
	OptionDefinition{ Option::HideGiftButton, "nagram.hideGiftButton", Category::Chats },
	OptionDefinition{ Option::HideBotCommandButton, "nagram.hideBotCommandButton", Category::Chats },
	OptionDefinition{ Option::HideAutoDeleteButton, "nagram.hideAutoDeleteButton", Category::Chats },
	OptionDefinition{ Option::DisablePremiumStickerEffects, "nagram.disablePremiumStickerEffects", Category::Media },
	OptionDefinition{ Option::DisableEmojiInteractions, "nagram.disableEmojiInteractions", Category::Media },
	OptionDefinition{ Option::DisableMessageEffects, "nagram.disableMessageEffects", Category::Media },
	OptionDefinition{ Option::HideMenuPin, "nagram.hideMenuPin", Category::Chats },
	OptionDefinition{ Option::HideMenuReport, "nagram.hideMenuReport", Category::Chats },
	OptionDefinition{ Option::HideMenuBlock, "nagram.hideMenuBlock", Category::Chats },
	OptionDefinition{ Option::HideMenuStatistics, "nagram.hideMenuStatistics", Category::Chats },
	OptionDefinition{ Option::HideMenuCopyLink, "nagram.hideMenuCopyLink", Category::Chats },
	OptionDefinition{ Option::HideMenuForward, "nagram.hideMenuForward", Category::Chats },
	OptionDefinition{ Option::HideMenuTranslate, "nagram.hideMenuTranslate", Category::Chats },
	OptionDefinition{ Option::HideMenuSelect, "nagram.hideMenuSelect", Category::Chats },
	OptionDefinition{ Option::HideMenuEmojiPacks, "nagram.hideMenuEmojiPacks", Category::Chats },
	OptionDefinition{ Option::ConfirmStickers, "nagram.confirmStickers", Category::Media },
	OptionDefinition{ Option::ConfirmGifs, "nagram.confirmGifs", Category::Media },
	OptionDefinition{ Option::PreviewVoiceMessages, "nagram.previewVoiceMessages", Category::Media },
	OptionDefinition{ Option::PreviewVideoMessages, "nagram.previewVideoMessages", Category::Media },
	OptionDefinition{ Option::HideSponsoredMessages, "nagram.hideSponsoredMessages", Category::Appearance },
	OptionDefinition{ Option::HideRecommendedChannels, "nagram.hideRecommendedChannels", Category::Appearance },
	OptionDefinition{ Option::HidePremiumBadges, "nagram.hidePremiumBadges", Category::Appearance },
	OptionDefinition{ Option::HideChannelBottomButton, "nagram.hideChannelBottomButton", Category::Chats },
	OptionDefinition{ Option::HideSendAsButton, "nagram.hideSendAsButton", Category::Chats },
	OptionDefinition{ Option::HideAttachButton, "nagram.hideAttachButton", Category::Chats },
	OptionDefinition{ Option::HideEmojiButton, "nagram.hideEmojiButton", Category::Chats },
	OptionDefinition{ Option::HideAiComposeButton, "nagram.hideAiComposeButton", Category::Chats },
	OptionDefinition{ Option::HideGreetingSticker, "nagram.hideGreetingSticker", Category::Media },
	OptionDefinition{ Option::ForwardBeforeComment, "nagram.forwardBeforeComment", Category::Chats },
	OptionDefinition{ Option::HideAllChatsFolder, "nagram.hideAllChatsFolder", Category::Appearance },
	OptionDefinition{ Option::ShowArchiveInFolders, "nagram.showArchiveInFolders", Category::Appearance },
	OptionDefinition{ Option::HideSavedAndArchivedPreviews, "nagram.hideSavedAndArchivedPreviews", Category::Privacy },
	OptionDefinition{ Option::HidePremiumPromotions, "nagram.hidePremiumPromotions", Category::Appearance },
	OptionDefinition{ Option::HideBirthdaySuggestions, "nagram.hideBirthdaySuggestions", Category::Appearance },
	OptionDefinition{ Option::HideFeaturedStickers, "nagram.hideFeaturedStickers", Category::Media },
	OptionDefinition{ Option::HideFeaturedEmoji, "nagram.hideFeaturedEmoji", Category::Media },
	OptionDefinition{ Option::HideGifShortcuts, "nagram.hideGifShortcuts", Category::Media },
	OptionDefinition{ Option::HideSavedTags, "nagram.hideSavedTags", Category::Appearance },
	OptionDefinition{ Option::HidePhoneSharePrompt, "nagram.hidePhoneSharePrompt", Category::Privacy },
	OptionDefinition{ Option::HideQuickStars, "nagram.hideQuickStars", Category::Chats },
	OptionDefinition{ Option::HideProfileGifts, "nagram.hideProfileGifts", Category::Appearance },
	OptionDefinition{ Option::HideProxySponsor, "nagram.hideProxySponsor", Category::Appearance },
	OptionDefinition{ Option::ShowMediaDetails, "nagram.showMediaDetails", Category::Media },
	OptionDefinition{ Option::ShowServiceTime, "nagram.showServiceTime", Category::Chats },
	OptionDefinition{ Option::HideReadTime, "nagram.hideReadTime", Category::Privacy },
	OptionDefinition{ Option::RevealSpoilers, "nagram.revealSpoilers", Category::Chats },
	OptionDefinition{ Option::HideReactionMenu, "nagram.hideReactionMenu", Category::Chats },
	OptionDefinition{ Option::HideReactionMenuWhenSelecting, "nagram.hideReactionMenuWhenSelecting", Category::Chats },
	OptionDefinition{ Option::HideCreateTodo, "nagram.hideCreateTodo", Category::Chats },
	OptionDefinition{ Option::HidePrivateReactions, "nagram.hidePrivateReactions", Category::Appearance },
	OptionDefinition{ Option::HideGroupReactions, "nagram.hideGroupReactions", Category::Appearance },
	OptionDefinition{ Option::HideChannelReactions, "nagram.hideChannelReactions", Category::Appearance },
	OptionDefinition{ Option::HideBubbleTail, "nagram.hideBubbleTail", Category::Appearance },
	OptionDefinition{ Option::SimpleQuotesAndReplies, "nagram.simpleQuotesAndReplies", Category::Appearance },
	OptionDefinition{ Option::HideReplyThumbnails, "nagram.hideReplyThumbnails", Category::Appearance },
	OptionDefinition{ Option::DisableMarkdown, "nagram.disableMarkdown", Category::Chats },
	OptionDefinition{ Option::DisableLinkPreview, "nagram.disableLinkPreview", Category::Chats },
	OptionDefinition{ Option::HideGroupStickers, "nagram.hideGroupStickers", Category::Media },
	OptionDefinition{ Option::PanguOnReading, "nagram.panguOnReading", Category::Appearance },
	OptionDefinition{ Option::PresentationMode, "nagram.presentationMode", Category::Privacy },
	OptionDefinition{ Option::IgnorePeerThemes, "nagram.ignorePeerThemes", Category::Appearance },
	OptionDefinition{ Option::PreferSystemAi, "nagram.preferSystemAi", Category::Advanced },
	OptionDefinition{ Option::NarrowInterfaceSymbols, "nagram.narrowInterfaceSymbols", Category::Appearance },
	OptionDefinition{ Option::GifPlaybackControls, "nagram.gifPlaybackControls", Category::Media },
	OptionDefinition{ Option::Mp4FilePreview, "nagram.mp4FilePreview", Category::Media },
	OptionDefinition{ Option::HideApplicationBadge, "nagram.hideApplicationBadge", Category::Appearance },
	OptionDefinition{ Option::UniformAvatarShapes, "nagram.uniformAvatarShapes", Category::Appearance },
	OptionDefinition{ Option::RawProfileId, "nagram.rawProfileId", Category::Chats },
};

[[nodiscard]] Core::WindowTitleContent WindowTitleOptions(
	Core::Settings &settings);

static_assert(kOptions.size() == static_cast<std::size_t>(Option::Count));
static_assert([] {
	for (auto i = std::size_t(0); i != kOptions.size(); ++i) {
		if (static_cast<std::size_t>(kOptions[i].option) != i
			|| !kOptions[i].key.starts_with("nagram.")) {
			return false;
		}
		for (auto j = std::size_t(0); j != i; ++j) {
			if (kOptions[i].key == kOptions[j].key) {
				return false;
			}
		}
	}
	return true;
}());

[[nodiscard]] bool Get(Core::Settings &settings, Option option);
[[nodiscard]] rpl::producer<bool> Value(
	Core::Settings &settings,
	Option option);
void Set(Core::Settings &settings, Option option, bool value);

[[nodiscard]] bool ReactionsHidden(
	Core::Settings &settings,
	not_null<const PeerData*> peer);

inline constexpr auto kMessageWidthKey = std::string_view("nagram.messageWidth");
[[nodiscard]] int MessageWidth(Core::Settings &settings);
[[nodiscard]] rpl::producer<int> MessageWidthValue(Core::Settings &settings);
void SetMessageWidth(Core::Settings &settings, int value);

enum class Roundness {
	Bubbles,
	Avatars,
};
inline constexpr auto kBubbleRoundnessKey = std::string_view("nagram.bubbleRoundness");
inline constexpr auto kMonospaceFontKey = std::string_view("nagram.monospaceFont");
[[nodiscard]] bool ValidMonospaceFont(const QString &family);
[[nodiscard]] bool MonospaceFontAvailable(const QString &family);
[[nodiscard]] QString MonospaceFont(Core::Settings &settings);
void SetMonospaceFont(Core::Settings &settings, const QString &family);
inline constexpr auto kAvatarRoundnessKey = std::string_view("nagram.avatarRoundness");
[[nodiscard]] int RoundnessValue(Core::Settings &settings, Roundness target);
[[nodiscard]] rpl::producer<int> RoundnessChanges(
	Core::Settings &settings,
	Roundness target);
void SetRoundness(Core::Settings &settings, Roundness target, int value);

enum class NotificationTiming {
	Default,
	OtherDevice,
};
inline constexpr auto kNotificationDelayKey = std::string_view("nagram.notificationDelay");
inline constexpr auto kCloudNotificationDelayKey = std::string_view("nagram.cloudNotificationDelay");
inline constexpr auto kMaxNotificationDelay = 60000;
[[nodiscard]] int NotificationDelay(
	Core::Settings &settings,
	NotificationTiming timing);
[[nodiscard]] rpl::producer<int> NotificationDelayValue(
	Core::Settings &settings,
	NotificationTiming timing);
void SetNotificationDelay(
	Core::Settings &settings,
	NotificationTiming timing,
	int value);
[[nodiscard]] crl::time ApplyNotificationDelay(
	Core::Settings &settings,
	NotificationTiming timing,
	crl::time nativeDelay,
	crl::time minimum);

inline constexpr auto kStickerScaleKey = std::string_view("nagram.stickerScale");
inline constexpr auto kStickerScales = std::array{ 50, 75, 100, 125, 150, 175, 200 };
[[nodiscard]] int StickerScale(Core::Settings &settings);
[[nodiscard]] rpl::producer<int> StickerScaleValue(Core::Settings &settings);
void SetStickerScale(Core::Settings &settings, int value);

inline constexpr auto kChatPreviewLinesKey = std::string_view("nagram.chatPreviewLines");
[[nodiscard]] int ChatPreviewLines(Core::Settings &settings);
[[nodiscard]] rpl::producer<int> ChatPreviewLinesValue(Core::Settings &settings);
void SetChatPreviewLines(Core::Settings &settings, int value);

inline constexpr auto kRecentStickerLimitKey = std::string_view("nagram.recentStickerLimit");
[[nodiscard]] int RecentStickerLimit(Core::Settings &settings);
[[nodiscard]] rpl::producer<int> RecentStickerLimitValue(Core::Settings &settings);
void SetRecentStickerLimit(Core::Settings &settings, int value);

inline constexpr auto kEditedMarkKey = std::string_view("nagram.editedMark");
[[nodiscard]] QString EditedMark(Core::Settings &settings);
[[nodiscard]] rpl::producer<QString> EditedMarkValue(Core::Settings &settings);
void SetEditedMark(Core::Settings &settings, const QString &value);

} // namespace Nagram
