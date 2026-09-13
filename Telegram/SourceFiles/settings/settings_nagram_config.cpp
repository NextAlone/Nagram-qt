#include "settings/settings_nagram_config.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "core/file_utilities.h"
#include "core/version.h"
#include "lang/lang_keys.h"
#include "nagram/nagram_config.h"
#include "nagram/nagram_chat_sort.h"
#include "nagram/nagram_main_menu.h"
#include "nagram/nagram_text.h"
#include "nagram/nagram_menu.h"
#include "nagram/nagram_services.h"
#include "nagram/nagram_snapshot.h"
#include "nagram/nagram_links.h"
#include "settings/settings_builder.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtGui/QClipboard>

#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

using namespace Nagram;

QString ErrorText(ConfigError error) {
	switch (error) {
	case ConfigError::TooLarge: return tr::lng_nagram_config_too_large(tr::now);
	case ConfigError::InvalidJson: return tr::lng_nagram_config_json_error(tr::now);
	case ConfigError::InvalidFormat: return tr::lng_nagram_config_format_error(tr::now);
	case ConfigError::UnsupportedVersion: return tr::lng_nagram_config_version_error(tr::now);
	case ConfigError::InvalidValue: return tr::lng_nagram_config_value_error(tr::now);
	case ConfigError::ReadFailed: return tr::lng_nagram_config_read_error(tr::now);
	case ConfigError::WriteFailed: return tr::lng_nagram_config_write_error(tr::now);
	case ConfigError::Changed: return tr::lng_nagram_config_changed_error(tr::now);
	case ConfigError::None: return QString();
	}
	Unexpected("Invalid config error.");
}

QString Quoted(QString text) {
	constexpr auto kPreviewCharacters = 160;
	if (text.size() > kPreviewCharacters) {
		text.truncate(kPreviewCharacters);
		if (text.back().isHighSurrogate()) {
			text.chop(1);
		}
		text += QChar(0x2026);
	}
	const auto json = QJsonDocument(QJsonArray{ text }).toJson(
		QJsonDocument::Compact);
	return QString::fromUtf8(json.mid(1, json.size() - 2));
}

QString ValueText(const QString &key, const QJsonValue &value) {
	if (value.isObject() && key == u"nagram.mainMenu"_q) {
		if (!ValidMainMenu(value.toObject())) {
			return tr::lng_nagram_config_invalid(tr::now);
		}
		const auto config = value.toObject();
		const auto labels = [](const QJsonArray &items) {
			auto result = QStringList();
			for (const auto &id : items) {
				result.push_back(MainMenuActionTitle(id.toString()));
			}
			return result.isEmpty()
				? tr::lng_nagram_inherit(tr::now)
				: result.join(u" → "_q);
		};
		return tr::lng_nagram_main_menu_summary(
			tr::now,
			lt_title, Quoted(config.value(u"title"_q).toString()),
			lt_order, labels(config.value(u"order"_q).toArray()),
			lt_actions, labels(config.value(u"hidden"_q).toArray()),
			lt_value, config.value(u"seasonalDecorations"_q).toBool()
				? tr::lng_nagram_config_on(tr::now)
				: tr::lng_nagram_config_off(tr::now));
	} else if (value.isObject() && key == u"nagram.chatSort"_q) {
		if (!ValidChatSort(value.toObject())) {
			return tr::lng_nagram_config_invalid(tr::now);
		}
		const auto order = value.toObject().value(u"order"_q).toArray();
		auto ids = QStringList();
		for (const auto &id : order) {
			ids.push_back(ChatSortRuleTitle(id.toString()));
		}
		return ids.isEmpty() ? tr::lng_nagram_config_off(tr::now) : ids.join(u" → "_q);
	} else if (value.isObject() && key == u"nagram.linkRules"_q) {
		if (!ValidLinkRules(value.toObject())) {
			return tr::lng_nagram_config_invalid(tr::now);
		}
		const auto config = value.toObject();
		auto lines = QStringList{ tr::lng_nagram_link_confirm_all(tr::now) + u": "_q
			+ (config.value(u"confirmAll"_q).toBool() ? tr::lng_nagram_config_on(tr::now)
				: tr::lng_nagram_config_off(tr::now)) };
		for (const auto &entry : config.value(u"rules"_q).toArray()) {
			const auto rule = entry.toObject();
			auto parameters = QStringList();
			for (const auto &name : rule.value(u"removeParameters"_q).toArray()) {
				parameters.push_back(name.toString());
			}
			lines.push_back(rule.value(u"host"_q).toString() + u" → "_q
				+ (rule.value(u"replacementHost"_q).toString().isEmpty()
					? tr::lng_nagram_inherit(tr::now) : rule.value(u"replacementHost"_q).toString())
				+ u"\n"_q + tr::lng_nagram_link_parameters(tr::now) + u": "_q + parameters.join(u", "_q)
				+ u"\n"_q + (rule.value(u"enabled"_q).toBool()
					? tr::lng_nagram_config_on(tr::now) : tr::lng_nagram_config_off(tr::now)));
		}
		return lines.join('\n');
	} else if (value.isObject() && key == u"nagram.snapshot"_q) {
		if (!ValidSnapshot(value.toObject())) {
			return tr::lng_nagram_config_invalid(tr::now);
		}
		auto lines = QStringList();
		for (const auto &[name, label] : std::array{
			std::pair(u"background"_q, tr::lng_nagram_snapshot_background(tr::now)),
			std::pair(u"date"_q, tr::lng_nagram_snapshot_date(tr::now)),
			std::pair(u"headers"_q, tr::lng_nagram_snapshot_headers(tr::now)),
			std::pair(u"reactions"_q, tr::lng_nagram_snapshot_reactions(tr::now)),
			std::pair(u"simpleReplies"_q, tr::lng_nagram_snapshot_simple_replies(tr::now)),
			std::pair(u"builtinTheme"_q, tr::lng_nagram_snapshot_builtin(tr::now)),
		}) {
			lines.push_back(label + u": "_q + (value.toObject().value(name).toBool()
				? tr::lng_nagram_config_on(tr::now) : tr::lng_nagram_config_off(tr::now)));
		}
		return lines.join('\n');
	} else if (value.isObject() && key == u"nagram.services"_q) {
		if (!ValidServices(value.toObject())) {
			return tr::lng_nagram_config_invalid(tr::now);
		}
		const auto config = value.toObject();
		auto lines = QStringList();
		for (const auto &key : { u"translation"_q, u"transcription"_q }) {
			const auto id = config.value(key).toString();
			const auto selected = FindService(config, id);
			lines.push_back((key == u"translation"_q
				? tr::lng_nagram_service_translation(tr::now)
				: tr::lng_nagram_service_transcription(tr::now)) + u": "_q
				+ (selected ? selected->name + u" ["_q + id + u"]"_q : id == u"system"_q
					? tr::lng_nagram_service_system(tr::now) : id.isEmpty()
					? tr::lng_nagram_inherit(tr::now) : id));
		}
		for (const auto &instance : config.value(u"instances"_q).toArray()) {
			const auto service = ParseService(instance.toObject());
			lines.push_back(service->name + u" ["_q + service->id + u"] ("_q
				+ service->protocol + u")\n"_q
				+ ServiceEndpoint(*service).toString(QUrl::FullyEncoded)
				+ u"\n"_q + tr::lng_nagram_service_model(tr::now) + u": "_q + service->model
				+ u"\n"_q + tr::lng_nagram_service_use_key(tr::now) + u": "_q
				+ (service->useKey ? tr::lng_nagram_config_on(tr::now) : tr::lng_nagram_config_off(tr::now))
				+ u"\n"_q + tr::lng_nagram_service_binding(tr::now) + u": "_q
				+ (service->credentialRef.isEmpty() ? tr::lng_nagram_config_off(tr::now) : service->credentialRef)
				+ u"\n"_q + tr::lng_nagram_service_language(tr::now) + u": "_q
				+ (service->language.isEmpty() ? tr::lng_nagram_inherit(tr::now) : service->language)
				+ u"\n"_q + tr::lng_nagram_service_temperature(tr::now) + u": "_q
				+ (service->temperature ? QString::number(*service->temperature) : tr::lng_nagram_inherit(tr::now))
				+ u"\n"_q + tr::lng_nagram_service_system_prompt(tr::now) + u": "_q + Quoted(service->systemPrompt)
				+ u"\n"_q + tr::lng_nagram_service_prompt(tr::now) + u": "_q + Quoted(service->prompt));
		}
		return lines.join(u"\n"_q);
	} else if (value.isObject() && key == u"nagram.textTools"_q) {
		const auto object = value.toObject();
		if (!ValidTextTools(object)) {
			return tr::lng_nagram_config_invalid(tr::now);
		}
		const auto language = object.value(u"codeLanguage"_q).toString();
		const auto replies = object.value(u"quickReplies"_q).toArray();
		return tr::lng_nagram_text_tools_summary(
			tr::now,
			lt_language, language.isEmpty() ? tr::lng_nagram_inherit(tr::now) : language,
			lt_first, Quoted(replies[0].toString()),
			lt_second, Quoted(replies[1].toString()));
	} else if (value.isObject() && key == u"nagram.messageMenu"_q) {
		const auto object = value.toObject();
		if (!ValidMenuTools(object)) {
			return tr::lng_nagram_config_invalid(tr::now);
		}
		const auto labels = [](const QJsonArray &ids) {
			auto result = QStringList();
			for (const auto &value : ids) {
				for (auto i = 0; i != int(MenuAction::Count); ++i) {
					if (value.toString() == MenuActionId(MenuAction(i))) {
						result.push_back(MenuActionTitle(MenuAction(i)));
						break;
					}
				}
			}
			return result.isEmpty() ? tr::lng_nagram_inherit(tr::now) : result.join(u", "_q);
		};
		return tr::lng_nagram_menu_summary(
			tr::now,
			lt_order, labels(object.value(u"order"_q).toArray()),
			lt_actions, labels(object.value(u"revealWithModifier"_q).toArray()));
	} else if (value.isBool()) {
		return value.toBool()
			? tr::lng_nagram_config_on(tr::now)
			: tr::lng_nagram_config_off(tr::now);
	} else if (value.isNull() || value.isUndefined()) {
		return tr::lng_nagram_config_invalid(tr::now);
	} else if (value.isString()) {
		return value.toString().isEmpty()
			? tr::lng_nagram_inherit(tr::now)
			: Quoted(value.toString());
	} else if (!value.toInt()) {
		return tr::lng_nagram_inherit(tr::now);
	}
	return QString::number(value.toInt())
		+ ((key == u"nagram.messageWidth"_q || key == u"nagram.stickerScale"_q)
			? u"%"_q
			: QString());
}

void AddText(not_null<Ui::GenericBox*> box, const QString &text) {
	const auto label = box->addRow(object_ptr<Ui::FlatLabel>(
		box, st::boxLabel));
	label->setMarkedText({ text });
	label->setSelectable(true);
	label->setBreakEverywhere(true);
}

base::flat_map<QString, QString> Titles(
		not_null<Window::SessionController*> controller) {
	auto result = base::flat_map<QString, QString>();
	result.emplace(u"nagram.snapshot"_q, tr::lng_nagram_snapshot(tr::now));
	result.emplace(u"nagram.rawProfileId"_q, tr::lng_nagram_profile_id_raw(tr::now));
	for (const auto &entry : Builder::SearchRegistry::Instance().collectAll(
			&controller->session())) {
		if (entry.id.startsWith(u"nagram."_q)) {
			result.emplace(entry.id, entry.title);
		}
	}
	return result;
}

QStringList Differences(
		not_null<Window::SessionController*> controller,
		const QJsonObject &before,
		const QJsonObject &after) {
	auto result = QStringList();
	const auto titles = Titles(controller);
	for (auto i = after.begin(); i != after.end(); ++i) {
		if (before.value(i.key()) == i.value()) {
			continue;
		}
		const auto title = titles.find(i.key());
		result.push_back((title != titles.end() ? title->second : i.key())
			+ '\n' + ValueText(i.key(), before.value(i.key()))
			+ u" → "_q + ValueText(i.key(), i.value()));
	}
	return result;
}

void ShowError(
		not_null<Window::SessionController*> controller,
		const ConfigData &data) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_nagram_config_title());
		AddText(box, ErrorText(data.error));
		for (const auto &key : data.invalid) {
			AddText(box, key);
		}
		box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });
	}));
}

void ShowImport(
		not_null<Window::SessionController*> controller,
		const ConfigData &data) {
	if (data.error != ConfigError::None) {
		ShowError(controller, data);
		return;
	}
	const auto baseline = ReadConfigRaw(Core::App().settings());
	const auto rows = Differences(
		controller,
		ReadConfig(Core::App().settings()).values,
		data.values);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_nagram_config_preview());

		AddText(box, tr::lng_nagram_config_import_about(tr::now));
		if (data.migratedFrom) {
			AddText(box, tr::lng_nagram_config_migrated(
				tr::now, lt_version, QString::number(data.migratedFrom)));
		}
		AddText(box, tr::lng_nagram_config_changes(
			tr::now, lt_amount, QString::number(rows.size())));
		for (const auto &row : rows) {
			AddText(box, row);
		}
		if (!data.unknown.empty()) {
			constexpr auto kUnknownPreviewCount = 20;
			auto names = QStringList();
			for (const auto &key : data.unknown.mid(0, kUnknownPreviewCount)) {
				names.push_back(Quoted(key));
			}
			AddText(box, tr::lng_nagram_config_unknown(
				tr::now, lt_amount, QString::number(data.unknown.size()))
				+ '\n' + names.join('\n')
				+ ((data.unknown.size() > kUnknownPreviewCount)
					? u"\n…"_q
					: QString()));
		}
		if (!rows.empty()) {
			box->addButton(tr::lng_nagram_config_apply(), crl::guard(controller, [=] {
				const auto error = ApplyConfig(Core::App().settings(), data, baseline);
				box->closeBox();
				if (error != ConfigError::None) {
					ShowError(controller, { .error = error });
				} else {
					controller->showToast(tr::lng_nagram_config_imported(tr::now));
				}
			}));
		}
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	}));
}

void ShowModified(not_null<Window::SessionController*> controller) {
	const auto data = ReadConfig(Core::App().settings());
	const auto rows = Differences(controller, ConfigDefaults(), data.values);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_nagram_config_modified());

		AddText(box, tr::lng_nagram_config_modified_about(tr::now));
		if (rows.empty()) {
			AddText(box, tr::lng_nagram_config_no_changes(tr::now));
		}
		for (const auto &row : rows) {
			AddText(box, row);
		}
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

void ShowDiagnostics(not_null<Window::SessionController*> controller) {
	const auto data = ReadConfig(Core::App().settings());
	const auto defaults = ConfigDefaults();
	const auto modified = Differences(controller, defaults, data.values).size();
	const auto report = tr::lng_nagram_config_diagnostic_report(
		tr::now,
		lt_version, QString::fromLatin1(AppVersionStr),
		lt_amount, QString::number(defaults.size()),
		lt_value, QString::number(modified),
		lt_error, QString::number(data.invalid.size()))
		+ (data.invalid.empty() ? QString() : u"\n\n"_q + data.invalid.join('\n'));
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::lng_nagram_config_diagnostics());
		AddText(box, report);
		box->addButton(tr::lng_nagram_config_copy_report(), [=] {
			QGuiApplication::clipboard()->setText(report);
			box->closeBox();
		});
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

} // namespace

void BuildNagramConfig(Builder::SectionBuilder &builder) {
	const auto controller = builder.controller();
	builder.addSubsectionTitle(tr::lng_nagram_config_title());
	builder.addDividerText(tr::lng_nagram_config_scope());
	builder.addButton({
		.id = u"nagram/config-modified"_q,
		.title = tr::lng_nagram_config_modified(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { ShowModified(controller); },
		.keywords = { u"Nagram"_q },
	});
	builder.addButton({
		.id = u"nagram/config-export"_q,
		.title = tr::lng_nagram_config_export(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			const auto data = ReadConfig(Core::App().settings());
			if (data.error != ConfigError::None) {
				ShowError(controller, data);
				return;
			}
			FileDialog::GetWritePath(
				Core::App().getFileDialogParent(),
				tr::lng_nagram_config_export(tr::now),
				tr::lng_nagram_config_file_filter(tr::now),
				u"nagram-settings.json"_q,
				crl::guard(controller, [=](QString &&path) {
					if (path.isEmpty()) {
						return;
					}
					const auto error = SaveConfig(path, data.values);
					if (error != ConfigError::None) {
						ShowError(controller, { .error = error });
					} else {
						controller->showToast(tr::lng_nagram_config_exported(tr::now));
					}
				}));
		},
		.keywords = { u"Nagram"_q },
	});
	builder.addButton({
		.id = u"nagram/config-import"_q,
		.title = tr::lng_nagram_config_import(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			FileDialog::GetOpenPath(
				Core::App().getFileDialogParent(),
				tr::lng_nagram_config_import(tr::now),
				tr::lng_nagram_config_file_filter(tr::now),
				crl::guard(controller, [=](FileDialog::OpenResult &&result) {
					if (!result.paths.isEmpty()) {
						ShowImport(controller, LoadConfig(result.paths.front()));
					} else if (!result.remoteContent.isEmpty()) {
						ShowImport(controller, ParseConfig(result.remoteContent));
					}
				}));
		},
		.keywords = { u"Nagram"_q },
	});
	builder.addButton({
		.id = u"nagram/config-diagnostics"_q,
		.title = tr::lng_nagram_config_diagnostics(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { ShowDiagnostics(controller); },
		.keywords = { u"Nagram"_q },
	});
}

} // namespace Settings
