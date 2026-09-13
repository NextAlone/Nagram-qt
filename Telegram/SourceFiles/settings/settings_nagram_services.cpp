#include "settings/settings_nagram_services.h"

#include "core/application.h"
#include "core/file_utilities.h"
#include "nagram/nagram_translation.h"
#include "lang/lang_keys.h"
#include "nagram/nagram_credentials.h"
#include "nagram/nagram_service_request.h"
#include "platform/platform_translate_provider.h"
#include "settings/settings_builder.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QUuid>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

using namespace Nagram;

bool Current(not_null<Ui::GenericBox*> box, const QJsonObject &expected) {
	if (Services(Core::App().settings()) == expected) {
		return true;
	}
	box->showToast(tr::lng_nagram_config_changed_error(tr::now));
	return false;
}

QString SelectionName(const QJsonObject &config, const QString &key) {
	const auto id = config.value(key).toString();
	if (id.isEmpty()) {
		return tr::lng_nagram_inherit(tr::now);
	} else if (id == u"telegram"_q) {
		return u"Telegram"_q;
	} else if (id == u"system"_q) {
		return tr::lng_nagram_service_system(tr::now);
	}
	const auto service = FindService(config, id);
	return service ? service->name : tr::lng_nagram_config_invalid(tr::now);
}

bool CredentialUsed(const QJsonObject &config, const QString &account) {
	for (const auto &value : config.value(u"instances"_q).toArray()) {
		const auto service = ParseService(value.toObject());
		if (service && CredentialAccount(*service) == account) {
			return true;
		}
	}
	return false;
}

CredentialError RemoveServiceCredential(const ServiceDefinition &service) {
	const auto account = CredentialAccount(service);
	if (!service.useKey && ReadCredential(account).error == CredentialError::Unavailable) {
		return CredentialError::None;
	}
	return DeleteCredential(account);
}

void ServiceTestBox(
		not_null<Ui::GenericBox*> box,
		ServiceDefinition service,
		Fn<void(QString)> chooseModel) {
	box->setTitle(tr::lng_nagram_service_test());

	box->addRow(object_ptr<Ui::FlatLabel>(box, rpl::single(
		service.name + u"\n"_q + ServiceEndpoint(service).toDisplayString()
		+ u"\n"_q + tr::lng_nagram_service_test_about(tr::now)), st::boxLabel));
	const auto result = box->addRow(object_ptr<Ui::FlatLabel>(
		box, rpl::single(QString()), st::boxLabel));
	result->setSelectable(true);
	struct State {
		ServiceRequest request;
		std::unique_ptr<Ui::TranslateProvider> provider;
		int generation = 0;
	};
	const auto state = box->lifetime().make_state<State>();
	const auto request = &state->request;
	const auto provider = &state->provider;
	const auto generation = &state->generation;
	const auto modelRows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto stop = [=] {
		++*generation;
		request->cancel();
		provider->reset();
		modelRows->clear();
	};
	const auto status = [=](ServiceResult response) {
		result->setText(ServiceErrorText(response.error, response.status));
	};
	if (service.protocol == u"openai"_q) {
		box->addButton(tr::lng_nagram_service_models(), [=] {
			stop();
			result->setText(tr::lng_nagram_service_testing(tr::now));
			request->models(service, crl::guard(box, [=](ServiceResult response) {
				if (response.error != ServiceError::None) {
					status(std::move(response));
					return;
				}
				const auto object = QJsonDocument::fromJson(response.body).object();
				const auto list = object.value(u"data"_q).toArray();
				auto models = QStringList();
				auto valid = object.value(u"data"_q).isArray()
					&& !list.isEmpty() && list.size() <= 2000;
				for (const auto &entry : list) {
					const auto value = entry.toObject().value(u"id"_q);
					const auto id = value.toString();
					valid = valid && value.isString() && !id.isEmpty()
						&& id.size() <= 256 && !id.contains('\n')
						&& !id.contains('\r') && !id.contains(QChar(0))
						&& QString::fromUtf8(id.toUtf8()) == id;
					models.push_back(id);
				}
				if (!valid) {
					status({ .error = ServiceError::Response });
					return;
				}
				models.removeDuplicates();
				models.sort();
				result->setText(tr::lng_nagram_service_models_about(tr::now));
				for (const auto &id : models) {
					const auto row = modelRows->add(object_ptr<Ui::SettingsButton>(
						modelRows, rpl::single(id), st::settingsButtonNoIcon));
					row->setClickedCallback([=] { chooseModel(id); box->closeBox(); });
				}
			}));
		});
	}
	if (service.kind == ServiceKind::Translation) {
		box->addButton(tr::lng_nagram_service_test_translation(), [=] {
			stop();
			result->setText(tr::lng_nagram_service_testing(tr::now));
			*provider = CreateServiceTranslateProvider(service,
				crl::guard(box, [=](QString error) { result->setText(error); }));
			(*provider)->request({ .text = { u"Hello, world!"_q } },
				LanguageId::FromName(u"zh"_q), crl::guard(box, [=](Ui::TranslateProviderResult response) {
					if (response.text) {
						result->setText(response.text->text);
					}
				}));
		});
	} else {
		box->addButton(tr::lng_nagram_service_test_audio(), [=] {
			stop();
			const auto expected = *generation;
			FileDialog::GetOpenPath(Core::App().getFileDialogParent(),
				tr::lng_nagram_service_test_audio(tr::now),
				tr::lng_nagram_service_audio_filter(tr::now),
				crl::guard(box, [=](FileDialog::OpenResult &&selected) {
					if (expected != *generation || selected.paths.isEmpty()) {
						return;
					}
					auto file = QFile(selected.paths.front());
					if (!file.open(QIODevice::ReadOnly)) {
						result->setText(tr::lng_nagram_service_audio_read_error(tr::now));
						return;
					}
					const auto bytes = file.read(24 * 1024 * 1024 + 1);
					const auto name = QFileInfo(file).fileName();
					file.close();
					box->uiShow()->showBox(Ui::MakeConfirmBox({
						.text = tr::lng_nagram_service_audio_upload(tr::now)
							+ u"\n\n"_q + name + u"\n"_q + ServiceEndpoint(service).toDisplayString(),
						.confirmed = crl::guard(box, [=](Fn<void()> close) {
							close();
							if (expected != *generation) {
								return;
							}
							result->setText(tr::lng_nagram_service_testing(tr::now));
							request->audio(service, bytes, name,
								crl::guard(box, [=](ServiceResult response) {
									if (response.error != ServiceError::None) {
										status(std::move(response));
										return;
									}
									const auto text = QJsonDocument::fromJson(response.body).object().value(u"text"_q).toString();
									if (text.isEmpty() || text.size() > 16384
										|| text.contains(QChar(0))
										|| QString::fromUtf8(text.toUtf8()) != text) {
										status({ .error = ServiceError::Response });
									} else {
										result->setText(text);
									}
								}));
						}),
					}));
				}));
		});
	}
	box->addButton(tr::lng_cancel(), [=] {
		stop();
		box->closeBox();
	});
}

void ServiceBox(
		not_null<Ui::GenericBox*> box,
		QJsonObject current,
		ServiceDefinition original,
		Fn<void(QJsonObject)> saved) {
	box->setTitle(tr::lng_nagram_service_edit());

	const auto add = [&](rpl::producer<QString> title,
			const QString &value, bool multiline = false) {
		box->addRow(object_ptr<Ui::FlatLabel>(box, std::move(title), st::boxLabel));
		return box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			multiline ? Ui::InputField::Mode::MultiLine : Ui::InputField::Mode::SingleLine,
			rpl::single(QString()),
			value));
	};
	const auto name = add(tr::lng_nagram_service_name(), original.name);
	const auto url = add(tr::lng_nagram_service_url(), original.baseUrl.toString());
	const auto endpoint = add(tr::lng_nagram_service_endpoint(), original.endpoint);
	const auto openai = original.protocol == u"openai"_q;
	const auto translation = original.kind == ServiceKind::Translation;
	const auto model = openai ? add(tr::lng_nagram_service_model(), original.model) : nullptr;
	const auto system = openai && translation
		? add(tr::lng_nagram_service_system_prompt(), original.systemPrompt, true) : nullptr;
	const auto prompt = openai ? add(tr::lng_nagram_service_prompt(), original.prompt, true) : nullptr;
	const auto language = !translation ? add(tr::lng_nagram_service_language(), original.language) : nullptr;
	const auto temperature = openai ? add(tr::lng_nagram_service_temperature(),
		original.temperature ? QString::number(*original.temperature) : QString()) : nullptr;
	const auto useKey = box->addRow(object_ptr<Ui::SettingsButton>(
		box, tr::lng_nagram_service_use_key(), st::settingsButtonNoIcon));
	useKey->toggleOn(rpl::single(original.useKey));
	const auto enabled = box->lifetime().make_state<bool>(original.useKey);
	useKey->toggledChanges() | rpl::on_next([=](bool value) {
		*enabled = value;
	}, box->lifetime());
	const auto keyRow = box->addRow(object_ptr<Ui::RpWidget>(box));
	keyRow->resize(keyRow->width(), st::defaultInputField.heightMin);
	const auto key = Ui::CreateChild<Ui::PasswordInput>(
		keyRow, st::defaultInputField, tr::lng_nagram_service_key());
	keyRow->widthValue() | rpl::on_next([=](int width) {
		key->resize(width, key->height());
	}, keyRow->lifetime());
	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_nagram_service_edit_about(), st::boxLabel));
	box->addButton(tr::lng_settings_save(), [=] {
		if (!Current(box, current)) {
			return;
		}
		auto service = original;
		service.name = name->getLastText().trimmed();
		service.baseUrl = QUrl(url->getLastText().trimmed(), QUrl::StrictMode);
		service.endpoint = endpoint->getLastText().trimmed();
		service.model = model ? model->getLastText().trimmed() : QString();
		service.systemPrompt = system ? system->getLastText() : QString();
		service.prompt = prompt ? prompt->getLastText() : QString();
		service.language = language ? language->getLastText().trimmed() : QString();
		service.useKey = *enabled;
		service.temperature = std::nullopt;
		if (temperature && !temperature->getLastText().trimmed().isEmpty()) {
			auto ok = false;
			service.temperature = temperature->getLastText().trimmed().toDouble(&ok);
			if (!ok) {
				temperature->showError();
				return;
			}
		}
		const auto value = SerializeService(service);
		if (!ParseService(value)) {
			box->showToast(tr::lng_nagram_service_invalid(tr::now));
			return;
		}
		auto updated = current;
		auto instances = updated.value(u"instances"_q).toArray();
		auto found = false;
		for (auto i = 0; i != instances.size(); ++i) {
			if (instances[i].toObject().value(u"id"_q) == service.id) {
				instances[i] = value;
				found = true;
				break;
			}
		}
		if (!found) {
			instances.push_back(value);
		}
		updated.insert(u"instances"_q, instances);
		auto secret = key->getLastText().toUtf8();
		if (service.useKey) {
			const auto account = CredentialAccount(service);
			const auto error = !secret.isEmpty()
				? WriteCredential(account, secret)
				: ReadCredential(account).error;
			secret.fill('\0');
			if (error != CredentialError::None) {
				key->showError();
				box->showToast(tr::lng_nagram_service_key_error(tr::now));
				return;
			}
		}
		SetServices(Core::App().settings(), updated);
		Core::App().saveSettingsDelayed();
		const auto oldAccount = CredentialAccount(original);
		if (found && oldAccount != CredentialAccount(service)
			&& !CredentialUsed(updated, oldAccount)
			&& RemoveServiceCredential(original) != CredentialError::None) {
			box->uiShow()->showToast(tr::lng_nagram_service_key_cleanup(tr::now));
		}
		saved(updated);
		box->closeBox();
	});
	if (FindService(current, original.id)) {
		box->addButton(tr::lng_nagram_service_test(), [=] {
			if (!Current(box, current)) {
				return;
			}
			box->uiShow()->showBox(Box(ServiceTestBox, original,
				crl::guard(box, [=](QString id) {
					if (model) {
						model->setTextWithTags({ std::move(id) });
					}
				})));
		});
		box->addButton(tr::lng_box_delete(), [=] {
			box->uiShow()->showBox(Ui::MakeConfirmBox({
				.text = tr::lng_nagram_service_delete_confirm(tr::now),
				.confirmed = crl::guard(box, [=](Fn<void()> close) {
					if (!Current(box, current)) {
						return;
					}
					auto updated = current;
					auto instances = QJsonArray();
					for (const auto &value : current.value(u"instances"_q).toArray()) {
						if (value.toObject().value(u"id"_q) != original.id) {
							instances.push_back(value);
						}
					}
					updated.insert(u"instances"_q, instances);
					for (const auto &key : { u"translation"_q, u"transcription"_q }) {
						if (updated.value(key) == original.id) {
							updated.insert(key, QString());
						}
					}
					const auto account = CredentialAccount(original);
					if (!CredentialUsed(updated, account)
						&& RemoveServiceCredential(original) != CredentialError::None) {
						box->showToast(tr::lng_nagram_service_key_cleanup(tr::now));
						return;
					}
					SetServices(Core::App().settings(), updated);
					Core::App().saveSettingsDelayed();
					saved(updated);
					close();
					box->closeBox();
				}),
				.confirmText = tr::lng_box_delete(),
			}));
		});
	}
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

void ServicesBox(not_null<Ui::GenericBox*> box, QJsonObject initial) {
	box->setTitle(tr::lng_nagram_services());

	box->addRow(object_ptr<Ui::FlatLabel>(box, tr::lng_nagram_services_about(), st::boxLabel));
	const auto state = box->lifetime().make_state<rpl::variable<QJsonObject>>(initial);
	const auto rows = box->addRow(object_ptr<Ui::VerticalLayout>(box));
	const auto popup = box->lifetime().make_state<base::unique_qptr<Ui::PopupMenu>>();
	const auto changed = crl::guard(box, [=](QJsonObject value) {
		crl::on_main(box, [=] { *state = value; });
	});
	state->value() | rpl::on_next([=](const QJsonObject &current) {
		rows->clear();
		const auto add = [&](QString title, Fn<void()> click) {
			const auto row = rows->add(object_ptr<Ui::SettingsButton>(
				rows, rpl::single(std::move(title)), st::settingsButtonNoIcon));
			row->setClickedCallback(std::move(click));
			return row;
		};
		for (const auto &key : { u"translation"_q, u"transcription"_q }) {
			const auto translation = key == u"translation"_q;
			add((translation ? tr::lng_nagram_service_translation(tr::now)
				: tr::lng_nagram_service_transcription(tr::now)) + u": "_q + SelectionName(current, key), [=] {
				*popup = base::make_unique_q<Ui::PopupMenu>(box);
				const auto option = [&](const QString &id, const QString &title) {
					(*popup)->addAction(title, [=] {
						if (!Current(box, current)) {
							return;
						}
						auto updated = current;
						updated.insert(key, id);
						SetServices(Core::App().settings(), updated);
						Core::App().saveSettingsDelayed();
						changed(updated);
					});
				};
				option(QString(), tr::lng_nagram_inherit(tr::now));
				option(u"telegram"_q, u"Telegram"_q);
				if (translation && Platform::IsTranslateProviderAvailable()) {
					option(u"system"_q, tr::lng_nagram_service_system(tr::now));
				}
				for (const auto &value : current.value(u"instances"_q).toArray()) {
					const auto service = ParseService(value.toObject());
					if (service && (service->kind == ServiceKind::Translation) == translation) {
						option(service->id, service->name);
					}
				}
				(*popup)->popup(QCursor::pos());
			});
		}
		for (const auto &value : current.value(u"instances"_q).toArray()) {
			const auto service = ParseService(value.toObject());
			if (service) {
				add(service->name, [=] {
					box->uiShow()->showBox(Box(ServiceBox, current, *service, changed));
				});
			}
		}
		for (const auto type : { 0, 1, 2 }) {
			const auto deepl = type == 1;
			const auto transcription = type == 2;
			add(transcription ? tr::lng_nagram_service_add_transcription(tr::now)
				: deepl ? tr::lng_nagram_service_add_deepl(tr::now)
				: tr::lng_nagram_service_add_openai(tr::now), [=] {
				auto service = ServiceDefinition{
					.id = QUuid::createUuid().toString(QUuid::WithoutBraces),
					.kind = transcription ? ServiceKind::Transcription : ServiceKind::Translation,
					.protocol = deepl ? u"deepl"_q : u"openai"_q,
					.baseUrl = QUrl(deepl ? u"https://api.deepl.com/v2/"_q : u"https://api.openai.com/v1/"_q),
					.endpoint = deepl ? u"translate"_q : transcription ? u"audio/transcriptions"_q : u"chat/completions"_q,
					.credentialRef = QUuid::createUuid().toString(QUuid::WithoutBraces),
				};
				box->uiShow()->showBox(Box(ServiceBox, current, std::move(service), changed));
			});
		}
	}, box->lifetime());
	box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
}

} // namespace

void BuildNagramServices(Builder::SectionBuilder &builder) {
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"nagram.services"_q,
		.title = tr::lng_nagram_services(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			const auto current = Services(Core::App().settings());
			if (!current) {
				controller->show(Ui::MakeInformBox(tr::lng_nagram_service_invalid(tr::now)));
				return;
			}
			controller->show(Box(ServicesBox, *current));
		},
		.keywords = { u"Nagram"_q, u"LLM"_q, u"API"_q },
	});
}

} // namespace Settings
