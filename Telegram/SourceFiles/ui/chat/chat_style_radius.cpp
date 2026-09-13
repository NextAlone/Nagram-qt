/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/chat/chat_style_radius.h"
#include "ui/chat/chat_style.h"
#include "base/options.h"

#include "ui/chat/chat_theme.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "styles/style_chat.h"

namespace Ui {
namespace {

auto BubbleRoundness = 0;

int AdjustRoundness(int radius) {
	return BubbleRoundness
		? std::max(1, radius * BubbleRoundness / 100)
		: radius;
}

base::options::toggle UseSmallMsgBubbleRadius({
	.id = kOptionUseSmallMsgBubbleRadius,
	.name = "Use small message bubble radius",
	.description = "Makes most message bubbles square-ish.",
	.restartRequired = true,
});

} // namespace

const char kOptionUseSmallMsgBubbleRadius[] = "use-small-msg-bubble-radius";

void SetBubbleRoundness(int percent) {
	Expects(!percent || (percent >= 10 && percent <= 100));
	BubbleRoundness = percent;
}

int BubbleRadiusSmall() {
	return AdjustRoundness(st::bubbleRadiusSmall);
}

int BubbleRadiusLarge() {
	return !BubbleRoundness && UseSmallMsgBubbleRadius.value()
		? int(st::bubbleRadiusSmall)
		: AdjustRoundness(st::bubbleRadiusLarge);
}

int MsgFileThumbRadiusSmall() {
	return AdjustRoundness(st::msgFileThumbRadiusSmall);
}

int MsgFileThumbRadiusLarge() {
	return !BubbleRoundness && UseSmallMsgBubbleRadius.value()
		? int(st::msgFileThumbRadiusSmall)
		: AdjustRoundness(st::msgFileThumbRadiusLarge);
}

}
