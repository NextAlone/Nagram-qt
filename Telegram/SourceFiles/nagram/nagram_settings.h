#pragma once

#include <array>
#include <string_view>

namespace Core {
class Settings;
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
};

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
void Set(Core::Settings &settings, Option option, bool value);
void Reset(Core::Settings &settings, Option option);

} // namespace Nagram
