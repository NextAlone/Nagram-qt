#include "nagram/nagram_repeat.h"

#include "api/api_common.h"
#include "api/api_sending.h"
#include "apiwrap.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_channel.h"
#include "data/data_chat_participant_status.h"
#include "data/data_document.h"
#include "data/data_histories.h"
#include "data/data_peer.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/view/history_view_context_menu.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mainwidget.h"
#include "nagram/nagram_menu.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"

namespace Nagram {
namespace {

[[nodiscard]] bool CanRepeat(not_null<HistoryItem*> item) {
	const auto peer = item->history()->peer;
	if (item->isService()
		|| item->isLocal()
		|| !item->isHistoryEntry()
		|| item->id <= 0) {
		return false;
	}
	if (const auto channel = peer->asChannel()) {
		if (!channel->amIn() || channel->isForum()) {
			return false;
		}
	}
	return Data::CanSendAnything(peer);
}

void RepeatWithQuote(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	const auto history = item->history();
	const auto owner = &item->history()->owner();

	// Re-resolve item to prevent use-after-free
	const auto itemId = item->fullId();
	const auto resolved = owner->message(itemId);
	if (!resolved) {
		return;
	}

	// Directly forward to the same chat with author info preserved
	auto draft = Data::ForwardDraft{
		.ids = owner->itemOrItsGroup(resolved),
		.options = Data::ForwardOptions::PreserveInfo,
	};

	auto resolvedDraft = history->resolveForwardDraft(draft);
	if (!resolvedDraft.items.empty()) {
		auto action = Api::SendAction(history);
		action.clearDraft = false;
		action.generateLocal = false;
		history->session().api().forwardMessages(
			std::move(resolvedDraft),
			action,
			nullptr);
	}
}

void RepeatWithoutQuote(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	const auto history = item->history();
	const auto owner = &item->history()->owner();
	// Re-resolve item to prevent use-after-free
	const auto itemId = item->fullId();
	const auto resolved = owner->message(itemId);
	if (!resolved) {
		return;
	}

	const auto media = resolved->media();
	auto action = Api::SendAction(history);
	action.clearDraft = false;
	action.replyTo = {};
	auto message = Api::MessageToSend(std::move(action));
	const auto &original = resolved->originalText();
	message.textWithTags = {
		original.text,
		TextUtilities::ConvertEntitiesToTextTags(original.entities),
	};

	if (const auto photo = media ? media->photo() : nullptr) {
		Api::SendExistingPhoto(
			std::move(message),
			photo);
	} else if (const auto document = media ? media->document() : nullptr) {
		Api::SendExistingDocument(
			std::move(message),
			document);
	} else if (!original.text.isEmpty()) {
		history->session().api().sendMessage(std::move(message));
	}
}

void ForwardWithoutQuote(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	const auto history = item->history();
	const auto owner = &item->history()->owner();

	// Re-resolve item to prevent use-after-free
	const auto itemId = item->fullId();
	const auto resolved = owner->message(itemId);
	if (!resolved) {
		return;
	}

	// Directly forward to the same chat without sender names
	auto draft = Data::ForwardDraft{
		.ids = owner->itemOrItsGroup(resolved),
		.options = Data::ForwardOptions::NoSenderNames,
	};

	auto resolvedDraft = history->resolveForwardDraft(draft);
	if (!resolvedDraft.items.empty()) {
		auto action = Api::SendAction(history);
		action.clearDraft = false;
		action.generateLocal = false;
		history->session().api().forwardMessages(
			std::move(resolvedDraft),
			action,
			nullptr);
	}
}

} // namespace

bool MessageForwardable(not_null<HistoryItem*> item) {
	return item->allowsForward()
		&& item->isHistoryEntry()
		&& !item->isService()
		&& !item->isLocal()
		&& item->id > 0;
}

void AddRepeatActions(
		not_null<Ui::PopupMenu*> menu,
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	auto &settings = Core::App().settings();
	const auto forwardable = MessageForwardable(item);

	// Repeat with quote (forward to same chat with author info)
	// If not forwardable, fall back to ForwardWithoutQuote
	if (!MenuHidden(settings, MenuAction::Repeat) && CanRepeat(item)) {
		AddOrderedMenuAction(
			menu,
			MenuAction::Repeat,
			tr::lng_nagram_action_repeat(tr::now),
			[=] {
				if (forwardable) {
					RepeatWithQuote(controller, item);
				} else {
					ForwardWithoutQuote(controller, item);
				}
			},
			&st::menuIconRepeat);
	}

	// Repeat without quote (resend content as own message)
	// If not forwardable, this is the same as RepeatNoQuote
	if (!MenuHidden(settings, MenuAction::RepeatNoQuote) && CanRepeat(item)) {
		AddOrderedMenuAction(
			menu,
			MenuAction::RepeatNoQuote,
			tr::lng_nagram_action_repeat_no_quote(tr::now),
			[=] { RepeatWithoutQuote(controller, item); },
			&st::menuIconRepeat);
	}

	// Forward without quote (forward with no sender names)
	if (!MenuHidden(settings, MenuAction::ForwardNoQuote) && forwardable) {
		AddOrderedMenuAction(
			menu,
			MenuAction::ForwardNoQuote,
			tr::lng_nagram_action_forward_no_quote(tr::now),
			[=] { ForwardWithoutQuote(controller, item); },
			&st::menuIconForward);
	}
}

} // namespace Nagram
