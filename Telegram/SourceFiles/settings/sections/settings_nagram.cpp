#include "settings/sections/settings_nagram.h"

#include "core/application.h"
#include "platform/platform_specific.h"
#include "core/core_settings.h"
#include "core/file_utilities.h"
#include "data/data_chat_filters.h"
#include "data/data_session.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "nagram/nagram_settings.h"
#include "nagram/nagram_chat_sort.h"
#include "nagram/nagram_stickers.h"
#include "nagram/nagram_main_menu.h"
#include "nagram/nagram_text.h"
#include "nagram/nagram_reading.h"
#include "nagram/nagram_translation.h"
#include "ui/boxes/confirm_box.h"

#include <QtCore/QJsonArray>
#include <QtGui/QFontDatabase>
#include "settings/sections/settings_main.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "settings/settings_nagram_config.h"
#include "settings/settings_nagram_menu.h"
#include "settings/settings_nagram_services.h"
#include "settings/settings_nagram_filters.h"
#include "settings/settings_nagram_links.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/painter.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/fields/number_input.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {
namespace {

using namespace Builder;

class NagramSection final : public Section<NagramSection> {
public:
	NagramSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

};

void AddToggle(
		SectionBuilder &builder,
		QString id,
		rpl::producer<QString> title,
		std::optional<rpl::producer<QString>> description,
		rpl::producer<bool> value,
		Fn<void(bool)> change) {
	const auto button = builder.addButton({
		.id = std::move(id),
		.title = std::move(title),
		.st = &st::settingsButtonNoIcon,
		.toggled = std::move(value),
		.keywords = { u"Nagram"_q },
	});
	if (button) {
		button->toggledChanges() | rpl::on_next(
			std::move(change),
			button->lifetime());
	}
	if (description) {
		builder.addDividerText(std::move(*description));
	}
}

void AddOption(
		SectionBuilder &builder,
		::Nagram::Option option,
		rpl::producer<QString> title,
		std::optional<rpl::producer<QString>> description = std::nullopt) {
	const auto &definition = ::Nagram::kOptions[std::size_t(option)];
	AddToggle(
		builder,
		QString::fromUtf8(definition.key.data(), definition.key.size()),
		std::move(title),
		std::move(description),
		::Nagram::Value(Core::App().settings(), option),
		[=](bool value) {
			::Nagram::Set(Core::App().settings(), option, value);
		});
}

void AddNumberOption(
		SectionBuilder &builder,
		QString id,
		rpl::producer<QString> title,
		rpl::producer<QString> description,
		rpl::producer<int> value,
		Fn<int()> current,
		Fn<void(int)> change,
		int maximum,
		int minimum = 0,
		bool showDescription = true) {
	const auto controller = builder.controller();
	builder.addButton({
		.id = std::move(id),
		.title = rpl::duplicate(title),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::combine(std::move(value), tr::lng_nagram_inherit())
			| rpl::map([](int value, const QString &inherit) {
				return value ? QString::number(value) : inherit;
			}),
		.onClick = [=] {
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(rpl::duplicate(title));
				box->addRow(object_ptr<Ui::FlatLabel>(
					box, rpl::duplicate(description), st::boxLabel));
				const auto wrap = box->addRow(object_ptr<Ui::FixedHeightWidget>(
					box, st::defaultInputField.heightMin));
				const auto field = Ui::CreateChild<Ui::NumberInput>(
					wrap,
					st::defaultInputField,
					tr::lng_nagram_inherit_zero(),
					QString::number(current()),
					maximum);
				wrap->widthValue() | rpl::on_next([=](int width) {
					field->resize(width, field->height());
					wrap->resize(width, field->height());
				}, wrap->lifetime());
				const auto save = [=] {
					auto ok = false;
					const auto number = field->getLastText().toInt(&ok);
					if (!ok
						|| number < 0
						|| number > maximum
						|| (number && number < minimum)) {
						field->showError();
						return;
					}
					change(number);
					box->closeBox();
				};
				QObject::connect(field, &Ui::NumberInput::submitted, box, save);
				box->addButton(tr::lng_settings_save(), save);
				box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
				field->selectAll();
				box->setFocusCallback([=] { field->setFocusFast(); });
			}));
		},
		.keywords = { u"Nagram"_q },
	});
	if (showDescription) {
		builder.addDividerText(std::move(description));
	}
}

void BuildNagram(SectionBuilder &builder) {
	builder.addSkip();
	builder.add([](const WidgetContext &ctx) {
		auto logo = object_ptr<Ui::RpWidget>(ctx.container);
		const auto widget = logo.data();
		const auto size = st::settingsCloudPasswordIconSize;
		widget->resize(size, size);
		widget->setNaturalWidth(size);
		widget->paintRequest() | rpl::on_next([=] {
			static const auto image = QImage(u":/gui/art/logo_256.png"_q);
			auto painter = QPainter(widget);
			painter.setRenderHint(QPainter::SmoothPixmapTransform);
			painter.drawImage(widget->rect(), image);
		}, widget->lifetime());
		return SectionBuilder::WidgetToAdd{
			.widget = std::move(logo),
			.align = style::al_top,
		};
	});
	builder.addSkip();
	builder.addDividerText(tr::lng_nagram_settings_about());
	builder.addDividerText(tr::lng_nagram_device_options_about());
	builder.addSkip();

	const auto addCategory = [&](
			rpl::producer<QString> title,
			rpl::producer<QString> description) {
		builder.addSubsectionTitle(std::move(title));
		builder.addDividerText(std::move(description));
		builder.addSkip();
	};
	builder.addSubsectionTitle(tr::lng_nagram_appearance());
	AddOption(builder, ::Nagram::Option::HideApplicationBadge,
		tr::lng_nagram_hide_application_badge());
	AddOption(builder, ::Nagram::Option::NarrowInterfaceSymbols,
		tr::lng_nagram_narrow_symbols(), tr::lng_nagram_narrow_symbols_about());
	AddOption(
		builder,
		::Nagram::Option::IgnorePeerThemes,
		tr::lng_nagram_ignore_peer_themes(),
		tr::lng_nagram_ignore_peer_themes_about());

	AddOption(
		builder,
		::Nagram::Option::HidePremiumPromotions,
		tr::lng_nagram_hide_premium_promotions(),
		tr::lng_nagram_hide_premium_promotions_about());
	AddOption(
		builder,
		::Nagram::Option::HideBirthdaySuggestions,
		tr::lng_nagram_hide_birthday_suggestions());
	AddOption(
		builder,
		::Nagram::Option::HideSavedTags,
		tr::lng_nagram_hide_saved_tags(),
		tr::lng_nagram_hide_saved_tags_about());
	AddOption(
		builder,
		::Nagram::Option::HideProfileGifts,
		tr::lng_nagram_hide_profile_gifts(),
		tr::lng_nagram_hide_profile_gifts_about());
	AddOption(
		builder,
		::Nagram::Option::HideProxySponsor,
		tr::lng_nagram_hide_proxy_sponsor(),
		tr::lng_nagram_hide_proxy_sponsor_about());
	AddNumberOption(
		builder,
		u"nagram.chatPreviewLines"_q,
		tr::lng_nagram_preview_lines(),
		tr::lng_nagram_preview_lines_about(),
		::Nagram::ChatPreviewLinesValue(Core::App().settings()),
		[] { return ::Nagram::ChatPreviewLines(Core::App().settings()); },
		[](int value) { ::Nagram::SetChatPreviewLines(Core::App().settings(), value); },
		3);
	AddOption(
		builder,
		::Nagram::Option::HideSponsoredMessages,
		tr::lng_nagram_hide_sponsored(),
		tr::lng_nagram_hide_sponsored_about());
	AddOption(
		builder,
		::Nagram::Option::HideRecommendedChannels,
		tr::lng_nagram_hide_recommended(),
		tr::lng_nagram_hide_recommended_about());
	AddOption(
		builder,
		::Nagram::Option::HidePremiumBadges,
		tr::lng_nagram_hide_premium_badges(),
		tr::lng_nagram_hide_premium_badges_about());
	AddOption(
		builder,
		::Nagram::Option::HideAllChatsFolder,
		tr::lng_nagram_hide_all_chats(),
		tr::lng_nagram_hide_all_chats_about());
	AddOption(
		builder,
		::Nagram::Option::ShowArchiveInFolders,
		tr::lng_nagram_archive_in_folders());
	AddOption(
		builder,
		::Nagram::Option::CompactChatList,
		tr::lng_nagram_compact_chats(),
		tr::lng_nagram_compact_chats_about());
	AddOption(
		builder,
		::Nagram::Option::HideEditedBadge,
		tr::lng_nagram_hide_edited_badge());
	AddOption(
		builder,
		::Nagram::Option::ExactMessageCounters,
		tr::lng_nagram_exact_message_counters());
	AddOption(
		builder,
		::Nagram::Option::HideMessageViews,
		tr::lng_nagram_hide_message_views());
	AddOption(
		builder,
		::Nagram::Option::HideChannelSignature,
		tr::lng_nagram_hide_channel_signature());
	AddOption(
		builder,
		::Nagram::Option::HideFolderUnreadCounters,
		tr::lng_nagram_hide_folder_unread());
	AddOption(
		builder,
		::Nagram::Option::HideStories,
		tr::lng_nagram_hide_stories());
	AddOption(
		builder,
		::Nagram::Option::HideReactions,
		tr::lng_nagram_hide_reactions(),
		tr::lng_nagram_hide_reactions_about());
	AddOption(
		builder,
		::Nagram::Option::HidePrivateReactions,
		tr::lng_nagram_hide_private_reactions());
	AddOption(
		builder,
		::Nagram::Option::HideGroupReactions,
		tr::lng_nagram_hide_group_reactions());
	AddOption(
		builder,
		::Nagram::Option::HideChannelReactions,
		tr::lng_nagram_hide_channel_reactions());
	AddOption(
		builder,
		::Nagram::Option::HideQuickShare,
		tr::lng_nagram_hide_quick_share());
	AddOption(
		builder,
		::Nagram::Option::WideChannelPosts,
		tr::lng_nagram_wide_channel_posts(),
		tr::lng_nagram_wide_channel_posts_about());
	builder.addSkip();
	AddOption(
		builder,
		::Nagram::Option::HideBubbleTail,
		tr::lng_nagram_hide_bubble_tail());
	AddOption(
		builder,
		::Nagram::Option::SimpleQuotesAndReplies,
		tr::lng_nagram_simple_quotes(),
		tr::lng_nagram_simple_quotes_about());
	AddOption(
		builder,
		::Nagram::Option::HideReplyThumbnails,
		tr::lng_nagram_hide_reply_thumbnails());
	AddNumberOption(
		builder,
		u"nagram.messageWidth"_q,
		tr::lng_nagram_message_width(),
		tr::lng_nagram_message_width_about(),
		::Nagram::MessageWidthValue(Core::App().settings()),
		[] { return ::Nagram::MessageWidth(Core::App().settings()); },
		[](int value) { ::Nagram::SetMessageWidth(Core::App().settings(), value); },
		400,
		50);
	builder.addButton({
		.id = u"nagram.monospaceFont"_q,
		.title = tr::lng_nagram_monospace_font(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(rpl::empty) | rpl::then(
			Core::App().settings().saveDelayedRequests()
		) | rpl::map([] {
			const auto family = ::Nagram::MonospaceFont(Core::App().settings());
			return family.isEmpty() ? tr::lng_nagram_inherit(tr::now) : family;
		}),
		.onClick = [controller = builder.controller()] {
			controller->show(Box([](not_null<Ui::GenericBox*> box) {
				auto families = std::vector<QString>{ QString() };
				auto options = std::vector<QString>{ tr::lng_nagram_inherit(tr::now) };
				for (const auto &family : QFontDatabase::families()) {
					if (QFontDatabase::isFixedPitch(family)
						&& ::Nagram::ValidMonospaceFont(family)) {
						families.push_back(family);
						options.push_back(family);
					}
				}
				const auto current = ::Nagram::MonospaceFont(Core::App().settings());
				const auto i = ranges::find(families, current);
				SingleChoiceBox(box, {
					.title = tr::lng_nagram_monospace_font(),
					.options = options,
					.initialSelection = i == families.end()
						? 0
						: int(i - families.begin()),
					.callback = [=](int index) {
						::Nagram::SetMonospaceFont(
							Core::App().settings(), families[index]);
					},
				});
			}));
		},
		.keywords = { u"Nagram"_q },
	});
	builder.addDividerText(tr::lng_nagram_monospace_about());
	AddOption(builder, ::Nagram::Option::UniformAvatarShapes,
		tr::lng_nagram_uniform_avatars(), tr::lng_nagram_uniform_avatars_about());
	for (const auto target : {
		::Nagram::Roundness::Bubbles,
		::Nagram::Roundness::Avatars,
	}) {
		const auto bubbles = (target == ::Nagram::Roundness::Bubbles);
		AddNumberOption(
			builder,
			bubbles ? u"nagram.bubbleRoundness"_q : u"nagram.avatarRoundness"_q,
			bubbles ? tr::lng_nagram_bubble_roundness() : tr::lng_nagram_avatar_roundness(),
			bubbles ? tr::lng_nagram_bubble_roundness_about() : tr::lng_nagram_avatar_roundness_about(),
			::Nagram::RoundnessChanges(Core::App().settings(), target),
			[=] { return ::Nagram::RoundnessValue(Core::App().settings(), target); },
			[=](int value) { ::Nagram::SetRoundness(Core::App().settings(), target, value); },
			100,
			10);
	}
	builder.addSkip();
	builder.addSubsectionTitle(tr::lng_nagram_chats());
	const auto editController = builder.controller();
	builder.addButton({
		.id = u"nagram.linkRules"_q,
		.title = tr::lng_nagram_link_rules(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { editController->show(Box(NagramLinksBox)); },
		.keywords = { u"Nagram"_q },
	});
	const auto navigationSession = builder.session();
	const auto placeholderTitle = [](const QString &mode) {
		return mode == u"chat"_q ? tr::lng_nagram_input_placeholder_chat(tr::now)
			: mode == u"sender"_q ? tr::lng_nagram_input_placeholder_sender(tr::now)
			: tr::lng_nagram_inherit(tr::now);
	};
	builder.addButton({
		.id = u"nagram.inputPlaceholder"_q,
		.title = tr::lng_nagram_input_placeholder(),
		.st = &st::settingsButtonNoIcon,
		.label = ::Nagram::InputPlaceholderModeValue(Core::App().settings())
			| rpl::map(placeholderTitle),
		.onClick = [=] {
			editController->show(Box([=](not_null<Ui::GenericBox*> box) {
				const auto modes = std::vector<QString>{ {}, u"chat"_q, u"sender"_q };
				const auto mode = ::Nagram::InputPlaceholderMode(Core::App().settings());
				SingleChoiceBox(box, {
					.title = tr::lng_nagram_input_placeholder(),
					.options = { placeholderTitle({}), placeholderTitle(u"chat"_q), placeholderTitle(u"sender"_q) },
					.initialSelection = int(ranges::find(modes, mode) - modes.begin()),
					.callback = [=](int index) {
						::Nagram::SetInputPlaceholderMode(Core::App().settings(), modes[index]);
					},
				});
			}));
		},
		.keywords = { u"Nagram"_q },
	});
	builder.addDividerText(tr::lng_nagram_input_placeholder_about());

	const auto folderTitle = [=](FilterId id) {
		if (id == Main::SessionSettings::kStartupFilterDefault) {
			return tr::lng_nagram_inherit(tr::now);
		} else if (id == Main::SessionSettings::kStartupFilterLast) {
			return tr::lng_nagram_startup_folder_last(tr::now);
		} else if (!id) {
			return tr::lng_filters_all(tr::now);
		}
		const auto &filters = navigationSession->data().chatsFilters().list();
		const auto i = ranges::find(filters, id, &Data::ChatFilter::id);
		return (i != end(filters))
			? i->title().text.text
			: tr::lng_nagram_startup_folder_unavailable(tr::now);
	};
	builder.addButton({
		.id = u"nagram.mainMenu"_q,
		.title = tr::lng_nagram_main_menu(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [controller = builder.controller()] {
			controller->show(Box(::Nagram::MainMenuBox));
		},
		.keywords = { u"Nagram"_q },
	});
	builder.addButton({
		.id = u"nagram.chatSort"_q,
		.title = tr::lng_nagram_chat_sort(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [controller = builder.controller()] {
			controller->show(Box(::Nagram::ChatSortBox));
		},
		.keywords = { u"Nagram"_q },
	});
	builder.addButton({
		.id = u"nagram/startup-folder"_q,
		.title = tr::lng_nagram_startup_folder(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::combine(
			navigationSession->settings().startupChatsFilterValue(),
			(rpl::single(rpl::empty_value())
				| rpl::then(navigationSession->data().chatsFilters().changed()))
		) | rpl::map([=](FilterId id, rpl::empty_value) { return folderTitle(id); }),
		.onClick = [=] {
			editController->show(Box([=](not_null<Ui::GenericBox*> box) {

				auto ids = std::vector<FilterId>{
					Main::SessionSettings::kStartupFilterDefault,
					Main::SessionSettings::kStartupFilterLast,
				};
				for (const auto &filter : navigationSession->data().chatsFilters().list()) {
					ids.push_back(filter.id());
				}
				const auto current = navigationSession->settings().startupChatsFilter();
				if (!ranges::contains(ids, current)) {
					ids.push_back(current);
				}
				auto titles = std::vector<QString>();
				for (const auto id : ids) {
					titles.push_back(folderTitle(id));
				}
				SingleChoiceBox(box, {
					.title = tr::lng_nagram_startup_folder(),
					.options = std::move(titles),
					.initialSelection = int(ranges::find(ids, current) - begin(ids)),
					.callback = [=](int index) {
						navigationSession->settings().setStartupChatsFilter(ids[index]);
						navigationSession->saveSettingsDelayed();
					},
				});
			}));
		},
		.keywords = { u"Nagram"_q },
	});
	builder.addDividerText(tr::lng_nagram_startup_folder_about());
	builder.addButton({
		.id = u"nagram.editedMark"_q,
		.title = tr::lng_nagram_edited_mark(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::combine(::Nagram::EditedMarkValue(Core::App().settings()),
			tr::lng_nagram_inherit()) | rpl::map([](const QString &text, const QString &inherit) {
				return text.isEmpty() ? inherit : text;
			}),
		.onClick = [=] {
			editController->show(Box([](not_null<Ui::GenericBox*> box) {
				box->setTitle(tr::lng_nagram_edited_mark());
				const auto field = box->addRow(object_ptr<Ui::InputField>(
					box, st::defaultInputField, Ui::InputField::Mode::SingleLine,
					tr::lng_nagram_inherit(), ::Nagram::EditedMark(Core::App().settings())));
				box->addRow(object_ptr<Ui::FlatLabel>(
					box, tr::lng_nagram_edited_mark_about(), st::boxLabel));
				const auto save = [=] {
					::Nagram::SetEditedMark(Core::App().settings(), field->getLastText().trimmed());
					box->closeBox();
				};
				field->submits() | rpl::on_next(save, field->lifetime());
				box->addButton(tr::lng_settings_save(), save);
				box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
				field->selectAll();
				box->setFocusCallback([=] { field->setFocusFast(); });
			}));
		},
		.keywords = { u"Nagram"_q },
	});
	builder.addDividerText(tr::lng_nagram_edited_mark_about());
	AddOption(
		builder,
		::Nagram::Option::ShowServiceTime,
		tr::lng_nagram_show_service_time(),
		tr::lng_nagram_show_service_time_about());
	AddOption(
		builder,
		::Nagram::Option::RevealSpoilers,
		tr::lng_nagram_reveal_spoilers(),
		tr::lng_nagram_reveal_spoilers_about());
	AddOption(
		builder,
		::Nagram::Option::HideReactionMenu,
		tr::lng_nagram_hide_reaction_menu());
	AddOption(
		builder,
		::Nagram::Option::HideReactionMenuWhenSelecting,
		tr::lng_nagram_hide_reaction_menu_selecting());
	AddOption(
		builder,
		::Nagram::Option::HideCreateTodo,
		tr::lng_nagram_hide_create_todo());
	AddOption(
		builder,
		::Nagram::Option::HideQuickStars,
		tr::lng_nagram_hide_quick_stars(),
		tr::lng_nagram_hide_quick_stars_about());
	AddOption(
		builder,
		::Nagram::Option::HideChannelBottomButton,
		tr::lng_nagram_hide_channel_bottom(),
		tr::lng_nagram_hide_channel_bottom_about());
	AddOption(
		builder,
		::Nagram::Option::HideSendAsButton,
		tr::lng_nagram_hide_send_as(),
		tr::lng_nagram_hide_send_as_about());
	AddOption(
		builder,
		::Nagram::Option::HideAttachButton,
		tr::lng_nagram_hide_attach());
	AddOption(
		builder,
		::Nagram::Option::HideEmojiButton,
		tr::lng_nagram_hide_emoji());
	AddOption(
		builder,
		::Nagram::Option::HideAiComposeButton,
		tr::lng_nagram_hide_ai_compose());
	AddOption(
		builder,
		::Nagram::Option::ForwardBeforeComment,
		tr::lng_nagram_forward_before_comment(),
		tr::lng_nagram_forward_before_comment_about());
	AddOption(
		builder,
		::Nagram::Option::HideGiftButton,
		tr::lng_nagram_hide_gift_button());
	AddOption(
		builder,
		::Nagram::Option::HideBotCommandButton,
		tr::lng_nagram_hide_bot_command_button());
	AddOption(
		builder,
		::Nagram::Option::HideAutoDeleteButton,
		tr::lng_nagram_hide_auto_delete_button(),
		tr::lng_nagram_hide_auto_delete_button_about());
	// Message-menu visibility (HideMenu* options) now lives in the unified
	// configurable menu list (nagram.messageMenu / BuildNagramMenu).
	AddOption(
		builder,
		::Nagram::Option::ShowMessageId,
		tr::lng_nagram_show_message_id(),
		tr::lng_nagram_show_message_id_about());
	AddOption(
		builder,
		::Nagram::Option::HideBotMenu,
		tr::lng_nagram_hide_bot_menu());
	AddOption(
		builder,
		::Nagram::Option::DisableEmojiHover,
		tr::lng_nagram_disable_emoji_hover());
	AddOption(
		builder,
		::Nagram::Option::DisableAttachHover,
		tr::lng_nagram_disable_attach_hover());
	AddOption(
		builder,
		::Nagram::Option::BotCommandsToDraft,
		tr::lng_nagram_bot_commands_to_draft(),
		tr::lng_nagram_bot_commands_to_draft_about());
	AddOption(
		builder,
		::Nagram::Option::SecondsInMessages,
		tr::lng_nagram_seconds_in_messages());
	AddOption(
		builder,
		::Nagram::Option::ShowForwardedMessageDate,
		tr::lng_nagram_forwarded_date(),
		tr::lng_nagram_forwarded_date_about());
	AddOption(
		builder,
		::Nagram::Option::DisableScrollToNextChannel,
		tr::lng_nagram_disable_next_channel());
	AddOption(
		builder,
		::Nagram::Option::DisableScrollToNextTopic,
		tr::lng_nagram_disable_next_topic());
	builder.addButton({
		.id = u"nagram.showProfileId"_q,
		.title = tr::lng_nagram_show_profile_id(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::combine(
			::Nagram::Value(Core::App().settings(), ::Nagram::Option::ShowProfileId),
			::Nagram::Value(Core::App().settings(), ::Nagram::Option::RawProfileId),
			tr::lng_nagram_config_off(),
			tr::lng_nagram_profile_id_raw(),
			tr::lng_nagram_profile_id_bot()
		) | rpl::map([](bool shown, bool raw, const QString &off,
				const QString &rawLabel, const QString &botLabel) {
			return !shown ? off : raw ? rawLabel : botLabel;
		}),
		.onClick = [controller = builder.controller()] {
			controller->show(Box([](not_null<Ui::GenericBox*> box) {
				using namespace ::Nagram;
				auto &settings = Core::App().settings();
				SingleChoiceBox(box, {
					.title = tr::lng_nagram_show_profile_id(),
					.options = { tr::lng_nagram_config_off(tr::now),
						tr::lng_nagram_profile_id_bot(tr::now),
						tr::lng_nagram_profile_id_raw(tr::now) },
					.initialSelection = !Get(settings, Option::ShowProfileId)
						? 0 : Get(settings, Option::RawProfileId) ? 2 : 1,
					.callback = [](int index) {
						auto &settings = Core::App().settings();
						Set(settings, Option::RawProfileId, index == 2);
						Set(settings, Option::ShowProfileId, index != 0);
					},
				});
			}));
		},
		.keywords = { u"Nagram"_q, u"ID"_q },
	});
	AddOption(
		builder,
		::Nagram::Option::ShowDc,
		tr::lng_nagram_show_photo_dc(),
		tr::lng_nagram_show_photo_dc_about());
	AddOption(
		builder,
		::Nagram::Option::ConfirmCalls,
		tr::lng_nagram_confirm_calls(),
		tr::lng_nagram_confirm_calls_about());
	builder.addSkip();
	AddOption(
		builder,
		::Nagram::Option::DisableMarkdown,
		tr::lng_nagram_disable_markdown(),
		tr::lng_nagram_disable_markdown_about());
	AddOption(
		builder,
		::Nagram::Option::DisableLinkPreview,
		tr::lng_nagram_disable_link_preview(),
		tr::lng_nagram_disable_link_preview_about());
	builder.addSkip();
	builder.addSubsectionTitle(tr::lng_nagram_media());
	AddOption(
		builder,
		::Nagram::Option::HideFeaturedStickers,
		tr::lng_nagram_hide_featured_stickers(),
		tr::lng_nagram_hide_featured_stickers_about());
	AddOption(
		builder,
		::Nagram::Option::HideFeaturedEmoji,
		tr::lng_nagram_hide_featured_emoji(),
		tr::lng_nagram_hide_featured_emoji_about());
	AddOption(
		builder,
		::Nagram::Option::HideGifShortcuts,
		tr::lng_nagram_hide_gif_shortcuts(),
		tr::lng_nagram_hide_gif_shortcuts_about());
	AddOption(
		builder,
		::Nagram::Option::ShowMediaDetails,
		tr::lng_nagram_show_media_details(),
		tr::lng_nagram_show_media_details_about());
	AddNumberOption(
		builder,
		u"nagram.recentStickerLimit"_q,
		tr::lng_nagram_recent_sticker_limit(),
		tr::lng_nagram_recent_sticker_limit_about(),
		::Nagram::RecentStickerLimitValue(Core::App().settings()),
		[] { return ::Nagram::RecentStickerLimit(Core::App().settings()); },
		[](int value) { ::Nagram::SetRecentStickerLimit(Core::App().settings(), value); },
		200);
	AddOption(
		builder,
		::Nagram::Option::HideGreetingSticker,
		tr::lng_nagram_hide_greeting(),
		tr::lng_nagram_hide_greeting_about());
	const auto controller = builder.controller();
	builder.addButton({
		.id = u"nagram/sticker-catalog"_q,
		.title = tr::lng_nagram_catalog_title(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] { ::Nagram::ShowStickerCatalog(controller); },
		.keywords = { u"Nagram"_q },
	});
	builder.addButton({
		.id = u"nagram.stickerScale"_q,
		.title = tr::lng_nagram_sticker_scale(),
		.st = &st::settingsButtonNoIcon,
		.label = ::Nagram::StickerScaleValue(Core::App().settings())
			| rpl::map([](int value) { return QString::number(value) + '%'; }),
		.onClick = [=] {
			controller->show(Box([](not_null<Ui::GenericBox*> box) {
				auto options = std::vector<QString>();
				for (const auto scale : ::Nagram::kStickerScales) {
					options.push_back(QString::number(scale) + '%');
				}
				SingleChoiceBox(box, {
					.title = tr::lng_nagram_sticker_scale(),
					.options = options,
					.initialSelection = (::Nagram::StickerScale(
						Core::App().settings()) - 50) / 25,
					.callback = [](int index) {
						::Nagram::SetStickerScale(
							Core::App().settings(),
							::Nagram::kStickerScales[index]);
					},
				});
			}));
		},
		.keywords = { u"Nagram"_q },
	});
	builder.addDividerText(tr::lng_nagram_sticker_scale_about());
	AddOption(
		builder,
		::Nagram::Option::ConfirmStickers,
		tr::lng_nagram_confirm_stickers(),
		tr::lng_nagram_confirm_stickers_about());
	AddOption(
		builder,
		::Nagram::Option::ConfirmGifs,
		tr::lng_nagram_confirm_gifs(),
		tr::lng_nagram_confirm_gifs_about());
	AddOption(
		builder,
		::Nagram::Option::PreviewVoiceMessages,
		tr::lng_nagram_preview_voice());
	AddOption(
		builder,
		::Nagram::Option::PreviewVideoMessages,
		tr::lng_nagram_preview_video());
	AddOption(
		builder,
		::Nagram::Option::DisablePremiumStickerEffects,
		tr::lng_nagram_disable_premium_effects());
	AddOption(
		builder,
		::Nagram::Option::DisableEmojiInteractions,
		tr::lng_nagram_disable_emoji_interactions());
	AddOption(
		builder,
		::Nagram::Option::DisableMessageEffects,
		tr::lng_nagram_disable_message_effects());
	AddOption(
		builder,
		::Nagram::Option::HideRecordingButton,
		tr::lng_nagram_hide_recording(),
		tr::lng_nagram_hide_recording_about());
	AddOption(
		builder,
		::Nagram::Option::HideStickerTimestamp,
		tr::lng_nagram_hide_sticker_time(),
		tr::lng_nagram_hide_sticker_time_about());
	AddOption(
		builder,
		::Nagram::Option::DisableVideoAutoplay,
		tr::lng_nagram_disable_video_autoplay(),
		tr::lng_nagram_disable_video_autoplay_about());
	builder.addSkip();
	AddOption(builder, ::Nagram::Option::GifPlaybackControls,
		tr::lng_nagram_gif_controls(), tr::lng_nagram_gif_controls_about());
	AddOption(builder, ::Nagram::Option::Mp4FilePreview,
		tr::lng_nagram_mp4_preview(), tr::lng_nagram_mp4_preview_about());
	AddOption(
		builder,
		::Nagram::Option::HideGroupStickers,
		tr::lng_nagram_hide_group_stickers());
	builder.addSkip();
	builder.addSubsectionTitle(tr::lng_nagram_privacy());
	AddOption(
		builder,
		::Nagram::Option::PresentationMode,
		tr::lng_nagram_presentation_mode(),
		Platform::ScreenshotProtectionSupported()
			? tr::lng_nagram_presentation_about()
			: tr::lng_nagram_presentation_unsupported());
	AddOption(
		builder,
		::Nagram::Option::HideReadTime,
		tr::lng_nagram_hide_read_time(),
		tr::lng_nagram_hide_read_time_about());
	AddOption(
		builder,
		::Nagram::Option::HidePhoneSharePrompt,
		tr::lng_nagram_hide_phone_share_prompt(),
		tr::lng_nagram_hide_phone_share_prompt_about());
	AddOption(
		builder,
		::Nagram::Option::HideSavedAndArchivedPreviews,
		tr::lng_nagram_hide_saved_previews(),
		tr::lng_nagram_hide_saved_previews_about());
	AddOption(
		builder,
		::Nagram::Option::HidePrivateChatActivities,
		tr::lng_nagram_hide_private_activities(),
		tr::lng_nagram_hide_private_activities_about());
	const auto session = builder.session();
	AddToggle(
		builder,
		u"nagram/phone-spoiler"_q,
		tr::lng_nagram_phone_spoiler(),
		tr::lng_nagram_phone_spoiler_about(),
		session->settings().phoneNumberHiddenValue(),
		[=](bool value) {
			auto &settings = session->settings();
			if (settings.phoneNumberHidden() != value) {
				settings.setPhoneNumberHidden(value);
				session->saveSettingsDelayed();
			}
		});
	builder.addSkip();
	addCategory(tr::lng_nagram_translation(), tr::lng_nagram_translation_about());
	builder.addSubsectionTitle(tr::lng_nagram_advanced());
	AddOption(builder, ::Nagram::Option::PreferSystemAi,
		tr::lng_nagram_system_ai(), tr::lng_nagram_system_ai_about());
	AddOption(builder, ::Nagram::Option::PanguOnSending,
		tr::lng_nagram_spacing_send(), tr::lng_nagram_spacing_about());
	AddOption(builder, ::Nagram::Option::PanguOnEditing,
		tr::lng_nagram_spacing_edit());
	AddOption(builder, ::Nagram::Option::PanguOnReading,
		tr::lng_nagram_spacing_reading(), tr::lng_nagram_reading_about());
	if (::Nagram::ChineseConversionAvailable()) {
		builder.addButton({
			.id = u"nagram.readingChinese"_q,
			.title = tr::lng_nagram_reading_chinese(),
			.st = &st::settingsButtonNoIcon,
			.onClick = [=] {
				editController->show(Box([=](not_null<Ui::GenericBox*> box) {
					box->setTitle(tr::lng_nagram_reading_chinese());
					const auto current = ::Nagram::ReadingChinese(Core::App().settings());
					const auto group = std::make_shared<Ui::RadiobuttonGroup>(
						current == u"simplified"_q ? 1 : current == u"traditional"_q ? 2 : 0);
					const auto labels = std::array{
						tr::lng_nagram_inherit(tr::now),
						tr::lng_nagram_chinese_simplified(tr::now),
						tr::lng_nagram_chinese_traditional(tr::now),
					};
					for (auto i = 0; i != labels.size(); ++i) {
						box->addRow(object_ptr<Ui::Radiobutton>(box, group, i, labels[i]));
					}
					box->addRow(object_ptr<Ui::FlatLabel>(box,
						tr::lng_nagram_reading_about(), st::boxLabel));
					group->setChangedCallback([=](int index) {
						::Nagram::SetReadingChinese(Core::App().settings(),
							index == 1 ? u"simplified"_q : index == 2 ? u"traditional"_q : QString());
						box->closeBox();
					});
					box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
				}));
			},
			.keywords = { u"Nagram"_q },
		});
	}
	builder.addButton({
		.id = u"nagram.textTools"_q,
		.title = tr::lng_nagram_text_tools(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			const auto current = ::Nagram::TextTools(Core::App().settings());
			if (!current) {
				editController->show(Ui::MakeInformBox(
					tr::lng_nagram_config_value_error(tr::now)
					+ u"\n\nnagram.textTools"_q));
				return;
			}
			editController->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(tr::lng_nagram_text_tools());

				box->addRow(object_ptr<Ui::FlatLabel>(
					box, tr::lng_nagram_code_language_default(), st::boxLabel));
				const auto language = box->addRow(object_ptr<Ui::InputField>(
					box, st::defaultInputField, Ui::InputField::Mode::SingleLine,
					tr::lng_nagram_inherit(), current->value(u"codeLanguage"_q).toString()));
				language->setMaxLength(32);
				const auto replies = current->value(u"quickReplies"_q).toArray();
				auto fields = std::array<Ui::InputField*, 2>();
				for (auto i = 0; i != fields.size(); ++i) {
					box->addRow(object_ptr<Ui::FlatLabel>(box,
						tr::lng_nagram_quick_reply_label(lt_index, rpl::single(QString::number(i + 1))),
						st::boxLabel));
					fields[i] = box->addRow(object_ptr<Ui::InputField>(
						box, st::defaultInputField, Ui::InputField::Mode::MultiLine,
						tr::lng_nagram_quick_reply_empty(), replies[i].toString()));
				}
				box->addRow(object_ptr<Ui::FlatLabel>(
					box, tr::lng_nagram_text_tools_about(), st::boxLabel));
				const auto save = [=] {
					auto updated = *current;
					updated.insert(u"codeLanguage"_q, language->getLastText().trimmed());
					updated.insert(u"quickReplies"_q, QJsonArray{
						fields[0]->getLastText(), fields[1]->getLastText(),
					});
					if (!::Nagram::ValidTextTools(updated)) {
						language->showError();
						return;
					}
					::Nagram::SetTextTools(Core::App().settings(), updated);
					box->closeBox();
				};
				box->addButton(tr::lng_settings_save(), save);
				box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
				box->setFocusCallback([=] { language->setFocusFast(); });
			}));
		},
		.keywords = { u"Nagram"_q },
	});
	for (const auto timing : {
			::Nagram::NotificationTiming::Default,
			::Nagram::NotificationTiming::OtherDevice }) {
		const auto cloud = (timing == ::Nagram::NotificationTiming::OtherDevice);
		const auto key = cloud
			? ::Nagram::kCloudNotificationDelayKey
			: ::Nagram::kNotificationDelayKey;
		AddNumberOption(
			builder,
			QString::fromUtf8(key.data(), key.size()),
			cloud ? tr::lng_nagram_notification_cloud_delay() : tr::lng_nagram_notification_delay(),
			tr::lng_nagram_notification_delay_about(),
			::Nagram::NotificationDelayValue(Core::App().settings(), timing),
			[=] { return ::Nagram::NotificationDelay(Core::App().settings(), timing); },
			[=](int value) { ::Nagram::SetNotificationDelay(Core::App().settings(), timing, value); },
			::Nagram::kMaxNotificationDelay,
			0,
			false);
	}
	builder.addDividerText(tr::lng_nagram_notification_delay_about());
	BuildNagramMenu(builder);
	BuildNagramServices(builder);
	BuildNagramFilters(builder);
	BuildNagramConfig(builder);

	builder.addDivider();
	builder.addSkip();
	builder.addButton({
		.id = u"nagram/source"_q,
		.title = tr::lng_nagram_source(),
		.icon = { &st::menuIconInfo },
		.onClick = [] {
			File::OpenUrl(u"https://github.com/NextAlone/Nagram-qt"_q);
		},
		.keywords = { u"Nagram"_q, u"source"_q, u"GitHub"_q },
	});
	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = NagramSection::Id(),
	.parentId = MainId(),
	.title = &tr::lng_nagram_settings,
	.icon = &st::menuIconSettings,
}, BuildNagram);

NagramSection::NagramSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

rpl::producer<QString> NagramSection::title() {
	return tr::lng_nagram_settings();
}

} // namespace

Type NagramId() {
	return NagramSection::Id();
}

} // namespace Settings
