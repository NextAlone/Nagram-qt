#include "nagram/nagram_links.h"

#include "core/application.h"
#include "core/click_handler_types.h"
#include "core/core_settings.h"
#include "core/file_utilities.h"
#include "lang/lang_keys.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QUuid>

#include "styles/style_layers.h"

namespace Nagram {
namespace {

bool Host(const QJsonValue &value, bool empty = false) {
	if (!value.isString()) {
		return false;
	}
	const auto host = value.toString();
	if (host.isEmpty()) {
		return empty;
	} else if (host.size() > 253 || host != host.toLower()) {
		return false;
	}
	static const auto label = QRegularExpression(u"\\A[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\\z"_q);
	return ranges::all_of(host.split('.'), [&](const QString &part) {
		return label.match(part).hasMatch();
	});
}

bool Removes(const QString &name, const QJsonArray &patterns) {
	return ranges::any_of(patterns, [&](const QJsonValue &value) {
		const auto pattern = value.toString();
		return pattern.endsWith('*')
			? name.startsWith(pattern.chopped(1)) : name == pattern;
	});
}

} // namespace

QJsonObject LinkRulesDefaults() {
	return { { u"version"_q, 1 }, { u"confirmAll"_q, false },
		{ u"rules"_q, QJsonArray() } };
}

bool ValidLinkRules(const QJsonObject &value) {
	if (value.keys() != LinkRulesDefaults().keys()
		|| value.value(u"version"_q) != 1
		|| !value.value(u"confirmAll"_q).isBool()
		|| !value.value(u"rules"_q).isArray()
		|| value.value(u"rules"_q).toArray().size() > 32) {
		return false;
	}
	auto ids = QSet<QString>();
	for (const auto &entry : value.value(u"rules"_q).toArray()) {
		const auto rule = entry.toObject();
		const auto id = rule.value(u"id"_q).toString();
		const auto uuid = QUuid(id);
		if (!entry.isObject() || rule.size() != 5 || uuid.isNull()
			|| uuid.toString(QUuid::WithoutBraces) != id || ids.contains(id)
			|| !rule.value(u"enabled"_q).isBool()
			|| !Host(rule.value(u"host"_q))
			|| !Host(rule.value(u"replacementHost"_q), true)
			|| !rule.value(u"removeParameters"_q).isArray()) {
			return false;
		}
		const auto parameters = rule.value(u"removeParameters"_q).toArray();
		if (parameters.size() > 32
			|| (parameters.isEmpty() && rule.value(u"replacementHost"_q).toString().isEmpty())) {
			return false;
		}
		auto names = QSet<QString>();
		static const auto pattern = QRegularExpression(u"\\A[a-zA-Z0-9_+.-]{1,64}\\*?\\z"_q);
		for (const auto &parameter : parameters) {
			const auto name = parameter.toString();
			if (!parameter.isString() || !pattern.match(name).hasMatch() || names.contains(name)) {
				return false;
			}
			names.insert(name);
		}
		ids.insert(id);
	}
	return true;
}

std::optional<QJsonObject> LinkRules(Core::Settings &settings) {
	const auto bytes = settings.readPref<QByteArray>(kLinkRulesKey);
	if (bytes.isEmpty()) {
		return LinkRulesDefaults();
	}
	const auto document = QJsonDocument::fromJson(bytes);
	return document.isObject() && ValidLinkRules(document.object())
		? std::make_optional(document.object()) : std::nullopt;
}

void SetLinkRules(Core::Settings &settings, const QJsonObject &value) {
	Expects(ValidLinkRules(value));
	if (value == LinkRulesDefaults()) {
		settings.clearPref(kLinkRulesKey);
	} else {
		settings.writePref<QByteArray>(kLinkRulesKey,
			QJsonDocument(value).toJson(QJsonDocument::Compact));
	}
}

std::variant<QUrl, QString> RewriteLink(
		const QJsonObject &config,
		const QString &original) {
	if (!ValidLinkRules(config) || original.size() > 16384) {
		return tr::lng_nagram_link_invalid(tr::now);
	}
	auto url = QUrl(original, QUrl::StrictMode);
	if (!url.isValid() || url.host().isEmpty()
		|| (url.scheme() != u"https"_q && url.scheme() != u"http"_q)) {
		return tr::lng_nagram_link_invalid(tr::now);
	}
	for (const auto &entry : config.value(u"rules"_q).toArray()) {
		const auto rule = entry.toObject();
		if (!rule.value(u"enabled"_q).toBool()
			|| QString::fromLatin1(QUrl::toAce(url.host())).toLower() != rule.value(u"host"_q).toString()) {
			continue;
		}
		if (!url.userInfo().isEmpty()) {
			return tr::lng_nagram_link_credentials(tr::now);
		}
		const auto host = rule.value(u"replacementHost"_q).toString();
		if (!host.isEmpty() && host != url.host()) {
			url.setScheme(u"https"_q);
			url.setHost(host);
			url.setPort(-1);
		}
		const auto parameters = rule.value(u"removeParameters"_q).toArray();
		if (!parameters.isEmpty() && url.hasQuery()) {
			auto kept = QStringList();
			for (const auto &part : url.query(QUrl::FullyEncoded).split('&')) {
				const auto name = QUrl::fromPercentEncoding(part.section('=', 0, 0).toUtf8());
				if (!Removes(name, parameters)) {
					kept.push_back(part);
				}
			}
			url.setQuery(kept.isEmpty() ? QString() : kept.join('&'), QUrl::StrictMode);
		}
		break;
	}
	return url;
}

bool HandleExternalLink(const QString &url, const ClickHandlerContext &context) {
	if (context.skipNagramLinkRules) {
		return false;
	}
	const auto parsed = QUrl(url, QUrl::StrictMode);
	if (parsed.scheme() != u"http"_q && parsed.scheme() != u"https"_q) {
		return false;
	}
	const auto config = LinkRules(Core::App().settings());
	const auto rewritten = config ? RewriteLink(*config, url)
		: std::variant<QUrl, QString>(tr::lng_nagram_link_invalid(tr::now));
	const auto error = std::get_if<QString>(&rewritten);
	const auto result = error ? QUrl() : std::get<QUrl>(rewritten);
	const auto changed = result != parsed;
	if (!error && !changed && !config->value(u"confirmAll"_q).toBool()) {
		return false;
	}
	const auto active = Core::App().activeWindow();
	const auto window = active ? active : Core::App().activePrimaryWindow();
	if (!context.show && !window) {
		LOG(("Nagram link: confirmation requires an application window; reopen the link from Nagram."));
		return true;
	}
	const auto show = [&](object_ptr<Ui::BoxContent> box) {
		if (context.show) {
			context.show->showBox(std::move(box));
		} else if (window) {
			window->show(std::move(box));
			window->activate();
		}
	};
	if (error) {
		show(Ui::MakeInformBox(*error));
		return true;
	}
	const auto destination = result.toString(QUrl::FullyEncoded);
	show(Box([=](not_null<Ui::GenericBox*> box) {

		Ui::ConfirmBox(box, {
			.text = changed ? tr::lng_nagram_link_changed(tr::now) : tr::lng_open_this_link(tr::now),
			.confirmed = [=](Fn<void()> close) {
				close();
				if (changed) {
					File::OpenUrl(destination);
				} else {
					auto next = context;
					next.skipNagramLinkRules = true;
					UrlClickHandler::Open(destination, QVariant::fromValue(next));
				}
			},
			.confirmText = tr::lng_open_link(),
		});
		const auto label = box->addRow(object_ptr<Ui::FlatLabel>(
			box, rpl::single(changed ? url + u"\n↓\n"_q + destination : destination), st::boxLabel));
		label->setSelectable(true);
	}));
	return true;
}

} // namespace Nagram
