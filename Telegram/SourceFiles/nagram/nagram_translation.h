#pragma once

#include "ui/text/text_entity.h"
#include "translate_provider.h"

namespace Main {
class Session;
} // namespace Main

namespace Nagram {

struct ServiceDefinition;

[[nodiscard]] bool ChineseConversionAvailable();
[[nodiscard]] std::optional<QString> ConvertChineseText(
	const QString &text,
	bool traditional);
[[nodiscard]] std::optional<TextWithEntities> ConvertChinese(
	TextWithEntities text,
	bool traditional);

struct TranslationPart {
	int start = 0;
	int length = 0;
	int index = -1;
};

struct TranslationPlan {
	TextWithEntities original;
	std::vector<TranslationPart> parts;
	QStringList texts;
};

[[nodiscard]] std::optional<TranslationPlan> PlanTranslation(TextWithEntities text);
[[nodiscard]] std::optional<TextWithEntities> ApplyTranslation(
	const TranslationPlan &plan,
	const QStringList &translated);
[[nodiscard]] std::unique_ptr<Ui::TranslateProvider> CreateServiceTranslateProvider(
	const ServiceDefinition &service,
	Fn<void(QString)> error);
[[nodiscard]] std::unique_ptr<Ui::TranslateProvider> CreateInteractiveTranslateProvider(
	not_null<Main::Session*> session,
	Fn<void(QString)> error);

} // namespace Nagram
