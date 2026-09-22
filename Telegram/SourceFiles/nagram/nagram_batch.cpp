#include "nagram/nagram_batch.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_changes.h"
#include "data/data_drafts.h"
#include "data/data_groups.h"
#include "data/data_peer.h"
#include "data/data_peer_values.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mainwidget.h"
#include "nagram/nagram_filters.h"
#include "nagram/nagram_menu.h"
#include "nagram/nagram_text.h"
#include "storage/storage_account.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_peer_menu.h"
#include "window/window_session_controller.h"

#include <QtCore/QJsonArray>
#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Nagram {
namespace {

constexpr auto kMaximumMessages = 100;
constexpr auto kMaximumText = 32768;

bool Available(not_null<HistoryItem*> item) {
	return item->allowsForward() && !item->isTtlCoveredMedia()
		&& !item->isEphemeral() && !item->isMediaSensitive();
}

QString ValidateItems(
		not_null<Main::Session*> session,
		const MessageIdsList &ids) {
	if (ids.empty() || ids.size() > kMaximumMessages) {
		return tr::lng_nagram_batch_limit(tr::now);
	}
	for (const auto id : ids) {
		const auto item = session->data().message(id);
		if (!item || !Available(item)) {
			return tr::lng_nagram_batch_unavailable(tr::now);
		}
		if (const auto filter = FilterMessage(item); filter && !item->nagramOriginalShown()) {
			if (!filter->ready) {
				return tr::lng_nagram_filter_pending(tr::now);
			} else if (filter->result.hidden || !filter->result.error.isEmpty()
				|| filter->result.projection.text != filter->original) {
				return tr::lng_nagram_batch_filtered(tr::now);
			}
		}
	}
	return {};
}

void MessageBatchBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		MessageIdsList ids) {
	box->setTitle(tr::lng_nagram_batch_title());

	const auto session = &controller->session();
	const auto label = box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_nagram_batch_about(), st::boxLabel));
	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box, st::defaultInputField, Ui::InputField::Mode::MultiLine));
	field->setMaxLength(kMaximumText);
	const auto reverse = box->addRow(object_ptr<Ui::Checkbox>(
		box, tr::lng_nagram_batch_reverse(tr::now), false));
	const auto headers = box->addRow(object_ptr<Ui::Checkbox>(
		box, tr::lng_nagram_batch_headers(tr::now), false));
	const auto omit = box->addRow(object_ptr<Ui::Checkbox>(
		box, tr::lng_nagram_batch_omit_names(tr::now), false));
	const auto reply = box->addRow(object_ptr<Ui::Checkbox>(
		box, tr::lng_nagram_batch_reply(tr::now), false));
	const auto current = box->lifetime().make_state<std::optional<MessageBatch>>();
	const auto rebuild = [=] {
		const auto prepared = PrepareMessageBatch(
			session, ids, reverse->checked(), headers->checked());
		if (const auto error = std::get_if<QString>(&prepared)) {
			*current = std::nullopt;
			label->setText(*error);
			field->setTextWithTags({});
			return;
		}
		*current = std::get<MessageBatch>(prepared);
		label->setText(tr::lng_nagram_batch_about(tr::now));
		field->setTextWithTags({ (*current)->text.text,
			TextUtilities::ConvertEntitiesToTextTags((*current)->text.entities) });
	};
	reverse->checkedChanges() | rpl::on_next(rebuild, box->lifetime());
	headers->checkedChanges() | rpl::on_next(rebuild, box->lifetime());
	const auto validate = [=] {
		const auto prepared = PrepareMessageBatch(
			session, ids, reverse->checked(), headers->checked());
		if (const auto error = std::get_if<QString>(&prepared)) {
			box->showToast(*error);
			return false;
		} else if (!*current || std::get<MessageBatch>(prepared).text != (*current)->text) {
			box->showToast(tr::lng_nagram_batch_changed(tr::now));
			return false;
		}
		return true;
	};
	const auto choose = [=](bool forward, bool saved) {
		if (!validate()) {
			return;
		}
		auto batch = **current;
		batch.text = { field->getTextWithTags().text,
			TextUtilities::ConvertTextTagsToEntities(field->getTextWithTags().tags) };
		const auto weak = base::make_weak(box);
		const auto weakController = base::make_weak(controller);
		const auto omitNames = omit->checked();
		const auto withReply = reply->checked();
		const auto apply = [=](not_null<Data::Thread*> target) {
			if (!weak || !weakController || !validate()) {
				return false;
			}
			const auto error = ApplyMessageBatchDraft(
				controller, target, batch, forward, omitNames, withReply);
			if (!error.isEmpty()) {
				controller->showToast(error);
				return false;
			}
			if (weak) {
				weak->closeBox();
			}
			return true;
		};
		if (saved) {
			apply(session->data().history(session->user()));
		} else {
			controller->show(Window::PrepareChooseRecipientBox(
				session, apply, tr::lng_nagram_batch_choose()), Ui::LayerOption::KeepOther);
		}
	};
	const auto quick = TextTools(Core::App().settings());
	if (quick) {
		for (const auto &text : quick->value(u"quickReplies"_q).toArray()) {
			if (text.toString().isEmpty()) {
				continue;
			}
			const auto button = box->addRow(object_ptr<Ui::SettingsButton>(
				box, rpl::single(text.toString()), st::settingsButtonNoIcon));
			button->setClickedCallback([=] {
				field->setTextWithTags({ text.toString() });
				reply->setChecked(true);
			});
		}
	}
	for (const auto &[title, callback] : std::array{
		std::pair(tr::lng_nagram_batch_text(tr::now), Fn<void()>([=] { choose(false, false); })),
		std::pair(tr::lng_nagram_batch_forward(tr::now), Fn<void()>([=] { choose(true, false); })),
		std::pair(tr::lng_nagram_batch_saved(tr::now), Fn<void()>([=] { choose(true, true); })),
	}) {
		const auto button = box->addRow(object_ptr<Ui::SettingsButton>(
			box, rpl::single(title), st::settingsButtonNoIcon));
		button->setClickedCallback(callback);
	}
	box->addButton(tr::lng_nagram_batch_copy(), [=] {
		if (validate()) {
			QApplication::clipboard()->setText(field->getLastText());
		}
	});
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	rebuild();
}

} // namespace

std::variant<MessageBatch, QString> PrepareMessageBatch(
		not_null<Main::Session*> session,
		MessageIdsList ids,
		bool reversed,
		bool headers) {
	if (const auto error = ValidateItems(session, ids); !error.isEmpty()) {
		return error;
	}
	if (base::flat_set<FullMsgId>(ids.begin(), ids.end()).size() != ids.size()) {
		return tr::lng_nagram_batch_unavailable(tr::now);
	}
	if (reversed) {
		ranges::reverse(ids);
	}
	auto result = MessageBatch{ .ids = std::move(ids) };
	for (const auto id : result.ids) {
		const auto item = session->data().message(id);
		if (!result.text.empty()) {
			result.text.append(u"\n\n"_q);
		}
		if (headers) {
			result.text.append(item->from()->name() + u" · "_q
				+ QDateTime::fromSecsSinceEpoch(item->date()).toString(Qt::ISODate) + u"\n"_q);
		}
		result.text.append(item->clipboardText().rich);
		if (result.text.text.size() > kMaximumText) {
			return tr::lng_nagram_batch_limit(tr::now);
		}
	}
	return result;
}

QString ApplyMessageBatchDraft(
		not_null<Window::SessionController*> controller,
		not_null<Data::Thread*> target,
		const MessageBatch &batch,
		bool forward,
		bool omitNames,
		bool reply) {
	if (&target->session() != &controller->session()) {
		return tr::lng_nagram_batch_unavailable(tr::now);
	}
	if (const auto error = ValidateItems(&controller->session(), batch.ids); !error.isEmpty()) {
		return error;
	}
	const auto history = target->owningHistory();
	const auto topic = target->topicRootId();
	const auto monoforum = target->monoforumPeerId();
	auto &local = controller->session().local();
	local.readDraftsWithCursors(history);
	if (!Data::DraftIsNull(history->localDraft(topic, monoforum))
		|| !Data::DraftIsNull(history->cloudDraft(topic, monoforum))
		|| local.draftSourceHasContent(history, Data::DraftKey::Local(topic, monoforum))
		|| local.draftSourceHasContent(history, Data::DraftKey::LocalEdit(topic, monoforum))
		|| history->localEditDraft(topic, monoforum)
		|| !history->forwardDraft(topic, monoforum).ids.empty()) {
		return tr::lng_nagram_batch_draft_exists(tr::now);
	} else if (forward) {
		return controller->content()->setForwardDraft(target, {
			.ids = batch.ids,
			.options = omitNames ? Data::ForwardOptions::NoSenderNames : Data::ForwardOptions::PreserveInfo,
		}) ? QString() : tr::lng_nagram_batch_unavailable(tr::now);
	} else if (!Data::CanSendTexts(target) || batch.text.empty()
		|| batch.text.text.size() > kMaximumText) {
		return tr::lng_nagram_batch_unavailable(tr::now);
	}
	const auto source = controller->session().data().message(batch.ids.front());
	if (reply && (source->history() != history
		|| source->topicRootId() != topic
		|| (monoforum && source->sublistPeerId() != monoforum))) {
		return tr::lng_nagram_batch_reply_scope(tr::now);
	}
	const auto text = TextWithTags{ batch.text.text,
		TextUtilities::ConvertEntitiesToTextTags(batch.text.entities) };
	history->setLocalDraft(std::make_unique<Data::Draft>(text, FullReplyTo{
		.messageId = reply ? batch.ids.front() : FullMsgId(),
		.topicRootId = topic,
		.monoforumPeerId = monoforum,
	}, SuggestOptions(), MessageCursor{
		int(text.text.size()), int(text.text.size()), Ui::kQFixedMax,
	}, Data::WebPageDraft()));
	controller->session().changes().entryUpdated(target, Data::EntryUpdate::Flag::LocalDraftSet);
	auto params = Window::SectionShow();
	params.reapplyLocalDraft = true;
	controller->showThread(target, ShowAtTheEndMsgId, params);
	return {};
}

void AddMessageBatchAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<Window::SessionController*> controller,
		MessageIdsList ids) {
	if (ids.empty()) {
		return;
	}
	if (ids.size() == 1) {
		if (const auto item = controller->session().data().message(ids.front())) {
			ids = controller->session().data().itemOrItsGroup(item);
		}
	}
	if (ranges::any_of(ids, [&](FullMsgId id) {
		const auto item = controller->session().data().message(id);
		return !item || !Available(item);
	})) {
		return;
	}
	if (MenuHidden(Core::App().settings(), MenuAction::Batch)) {
		return;
	}
	AddOrderedMenuAction(
		menu,
		MenuAction::Batch,
		tr::lng_nagram_batch_title(tr::now),
		crl::guard(controller, [=] {
			controller->show(Box(MessageBatchBox, controller, ids));
		}));
}

} // namespace Nagram
