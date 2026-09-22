#include "nagram/nagram_repeat.h"

#include "api/api_common.h"
#include "apiwrap.h"
#include "core/application.h"
#include "core/core_settings.h"
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
#include "nagram/nagram_menu.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"

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
	return Data::CanSendTexts(peer)
		|| Data::CanSendAnyOf(peer, Data::SendRestriction::SendPhotos)
		|| Data::CanSendAnyOf(peer, Data::SendRestriction::SendVideos)
		|| Data::CanSendAnyOf(peer, Data::SendRestriction::SendMusic)
		|| Data::CanSendAnyOf(peer, Data::SendRestriction::SendFiles)
		|| Data::CanSendAnyOf(peer, Data::SendRestriction::SendStickers);
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

	auto draft = Data::ForwardDraft{
		.ids = owner->itemOrItsGroup(resolved),
		.options = Data::ForwardOptions::PreserveInfo,
	};

	history->setForwardDraft(std::move(draft));
	controller->content()->cancelSelection();
}

void RepeatWithoutQuote(
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	const auto history = item->history();
	const auto owner = &item->history()->owner();
	const auto peer = history->peer;

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

	if (const auto photo = media ? media->photo() : nullptr) {
		const auto &session = history->session();
		Api::SendExistingPhoto(
			Api::MessageToSend(std::move(action)),
			photo,
			resolved->originalText());
	} else if (const auto document = media ? media->document() : nullptr) {
		const auto &session = history->session();
		Api::SendExistingDocument(
			Api::MessageToSend(std::move(action)),
			document,
			resolved->originalText());
	} else if (!resolved->originalText().text.isEmpty()) {
		const auto &session = history->session();
		session.api().sendMessage(Api::MessageToSend(std::move(action)
			.withText(resolved->originalText())));
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

	auto draft = Data::ForwardDraft{
		.ids = owner->itemOrItsGroup(resolved),
		.options = Data::ForwardOptions::NoSenderNames,
	};

	history->setForwardDraft(std::move(draft));
	controller->content()->cancelSelection();
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
		not_null<HistoryItem*> item,
		const HistoryView::ContextMenuRequest &request) {
	const auto &settings = Core::App().settings();

	// Repeat with quote (forward to same chat with author info)
	if (!MenuHidden(settings, MenuAction::Repeat) && CanRepeat(item)) {
		AddOrderedMenuAction(
			menu,
			MenuAction::Repeat,
			tr::lng_nagram_action_repeat(tr::now),
			[=] { RepeatWithQuote(controller, item); });
	}

	// Repeat without quote (resend content as own message)
	if (!MenuHidden(settings, MenuAction::RepeatNoQuote) && CanRepeat(item)) {
		AddOrderedMenuAction(
			menu,
			MenuAction::RepeatNoQuote,
			tr::lng_nagram_action_repeat_no_quote(tr::now),
			[=] { RepeatWithoutQuote(controller, item); });
	}

	// Forward without quote (forward with no sender names)
	if (!MenuHidden(settings, MenuAction::ForwardNoQuote)
		&& MessageForwardable(item)) {
		AddOrderedMenuAction(
			menu,
			MenuAction::ForwardNoQuote,
			tr::lng_nagram_action_forward_no_quote(tr::now),
			[=] { ForwardWithoutQuote(controller, item); });
	}
}

} // namespace Nagram

