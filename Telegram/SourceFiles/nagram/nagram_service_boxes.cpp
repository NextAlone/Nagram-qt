#include "nagram/nagram_service_boxes.h"

#include "apiwrap.h"
#include "api/api_transcribes.h"
#include "boxes/translate_box.h"
#include "boxes/translate_box_content.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/file_location.h"
#include "core/ui_integration.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "data/data_document_media.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/session/session_show.h"
#include "main/main_session.h"
#include "nagram/nagram_service_request.h"
#include "nagram/nagram_translation.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"

#include <QtCore/QFile>
#include <QtCore/QJsonDocument>

#include "styles/style_layers.h"

namespace Nagram {

void ShowDraftTranslation(
		std::shared_ptr<Main::SessionShow> show,
		not_null<Ui::InputField*> field) {
	const auto original = field->getTextWithTags();
	show->showBox(Box([=](not_null<Ui::GenericBox*> box) {

		struct State {
			std::unique_ptr<Ui::TranslateProvider> provider;
			std::optional<TextWithEntities> result;
			rpl::variable<LanguageId> to;
		};
		const auto state = box->lifetime().make_state<State>();
		state->to = Ui::ChooseTranslateTo(LanguageId());
		const auto text = TextWithEntities{
			original.text,
			TextUtilities::ConvertTextTagsToEntities(original.tags),
		};
		Ui::TranslateBoxContent(box, {
			.text = text,
			.textContext = Core::TextContext({ .session = &show->session() }),
			.to = state->to.value(),
			.chooseTo = [=] {
				box->uiShow()->showBox(Ui::ChooseTranslateToBox(
					state->to.current(),
					crl::guard(box, [=](LanguageId to) { state->to = to; })));
			},
			.request = [=](LanguageId to, Fn<void(Ui::TranslateBoxContentResult)> done) {
				state->result.reset();
				state->provider = CreateInteractiveTranslateProvider(
					&show->session(),
					crl::guard(box, [=](QString error) { box->showToast(error); }));
				state->provider->request({ .text = text }, to,
					[=](Ui::TranslateProviderResult result) {
						state->result = result.text;
						done({
							.text = std::move(result.text),
							.error = result.error == Ui::TranslateProviderError::None
								? Ui::TranslateBoxContentError::None
								: result.error == Ui::TranslateProviderError::LocalLanguagePackMissing
								? Ui::TranslateBoxContentError::LocalLanguagePackMissing
								: Ui::TranslateBoxContentError::Unknown,
						});
					});
			},
		});
		box->addButton(tr::lng_nagram_translate_apply(), crl::guard(field, [=] {
			if (!state->result) {
				return;
			}
			if (field->getTextWithTags() != original) {
				box->showToast(tr::lng_nagram_draft_changed(tr::now));
				return;
			}
			field->setTextWithTags({
				state->result->text,
				TextUtilities::ConvertEntitiesToTextTags(state->result->entities),
			});
			box->closeBox();
		}));
	}));
}

bool CustomTranscriptionSelected() {
	const auto config = Services(Core::App().settings());
	if (!config) {
		return true;
	}
	const auto id = config->value(u"transcription"_q).toString();
	return !id.isEmpty() && id != u"telegram"_q;
}

void ShowCustomTranscription(
		std::shared_ptr<Main::SessionShow> show,
		not_null<HistoryItem*> item,
		bool manage) {
	if (!manage && show->session().api().transcribes().toggleExternal(item)) {
		return;
	}
	const auto config = Services(Core::App().settings());
	const auto service = config
		? FindService(*config, config->value(u"transcription"_q).toString())
		: std::nullopt;
	const auto document = item->media() ? item->media()->document() : nullptr;
	if (!service || !document) {
		show->showToast(tr::lng_nagram_service_invalid(tr::now));
		return;
	}
	const auto id = item->fullId();
	const auto documentId = document->id;
	const auto serviceConfig = Core::App().settings().readPref<QByteArray>(
		kServicesKey);
	show->showBox(Box([=](not_null<Ui::GenericBox*> box) {

		struct State {
			ServiceRequest request;
			std::shared_ptr<Data::DocumentMedia> media;
			QString result;
			bool loading = false;
		};
		box->setTitle(tr::lng_nagram_service_transcription());
		const auto state = box->lifetime().make_state<State>();
		state->media = document->createMediaView();
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_nagram_transcribe_upload_about(
				lt_name, rpl::single(service->name),
				lt_url, rpl::single(ServiceEndpoint(*service).toDisplayString())),
			st::boxLabel));
		const auto label = box->addRow(object_ptr<Ui::FlatLabel>(box, st::boxLabel));
		label->setSelectable(item->allowsForward());
		state->result = show->session().api().transcribes().entry(item).result;
		label->setText(state->result);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			tr::lng_nagram_transcribe_cache_about(),
			st::boxLabel));
		box->addButton(tr::lng_nagram_transcribe_start(), [=] {
			if (state->loading) {
				return;
			}
			if (Core::App().settings().readPref<QByteArray>(kServicesKey)
				!= serviceConfig) {
				label->setText(tr::lng_nagram_service_invalid(tr::now));
				return;
			}
			const auto item = show->session().data().message(id);
			const auto media = item ? item->media() : nullptr;
			if (!media || !media->document() || media->document()->id != documentId) {
				box->showToast(tr::lng_nagram_transcribe_missing(tr::now));
				return;
			}
			if (media->ttlSeconds()) {
				box->showToast(tr::lng_nagram_transcribe_missing(tr::now));
				return;
			}
			auto bytes = state->media->bytes();
			constexpr auto limit = 24 * 1024 * 1024;
			if (document->size > limit) {
				label->setText(ServiceErrorText(ServiceError::TooLarge));
				return;
			}
			if (bytes.isEmpty()) {
				const auto location = document->location(true);
				if (!location.isEmpty() && location.accessEnable()) {
					auto file = QFile(location.name());
					if (file.open(QIODevice::ReadOnly)) {
						bytes = file.read(limit + 1);
					}
					location.accessDisable();
				}
			}
			if (bytes.isEmpty()) {
				document->save(item->fullId(), QString());
				label->setText(tr::lng_nagram_transcribe_download(tr::now));
				return;
			}
			const auto generation = show->session().api().transcribes().externalGeneration(
				document->isVideoMessage());
			state->loading = true;
			label->setText(tr::lng_contacts_loading(tr::now));
			state->request.audio(*service, std::move(bytes),
				document->isVideoMessage() ? u"audio.mp4"_q : u"audio.ogg"_q,
				[=](ServiceResult response) {
					state->loading = false;
					if (response.error != ServiceError::None) {
						label->setText(ServiceErrorText(response.error, response.status));
						return;
					}
					const auto value = QJsonDocument::fromJson(
						response.body).object().value(u"text"_q);
					if (!value.isString() || value.toString().isEmpty()
						|| value.toString().size() > 16384
						|| value.toString().contains(QChar(0))) {
						label->setText(ServiceErrorText(ServiceError::Response));
						return;
					}
					const auto item = show->session().data().message(id);
					if (!item || !show->session().api().transcribes().setExternal(
							item, documentId, serviceConfig, generation, value.toString())) {
						label->setText(tr::lng_nagram_transcribe_missing(tr::now));
						return;
					}
					state->result = value.toString();
					label->setText(state->result);
				});
		});
		box->addButton(tr::lng_context_copy_text(), [=] {
			const auto item = show->session().data().message(id);
			const auto media = item ? item->media() : nullptr;
			const auto document = media ? media->document() : nullptr;
			if (item && item->allowsForward() && document
				&& document->id == documentId && !media->ttlSeconds()
				&& !state->result.isEmpty()
				&& show->session().api().transcribes().entry(item).result
					== state->result) {
				TextUtilities::SetClipboardText(TextForMimeData::Simple(state->result));
			}
		});
		box->addButton(tr::lng_cancel(), [=] {
			state->request.cancel();
			box->closeBox();
		});
	}));
}

} // namespace Nagram
