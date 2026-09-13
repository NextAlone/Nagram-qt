#include "settings/sections/settings_nagram.h"

#include "core/file_utilities.h"
#include "lang/lang_keys.h"
#include "settings/sections/settings_main.h"
#include "settings/settings_builder.h"
#include "settings/settings_common_session.h"
#include "ui/painter.h"
#include "ui/wrap/vertical_layout.h"

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
	builder.addSkip();

	const auto addCategory = [&](
			rpl::producer<QString> title,
			rpl::producer<QString> description) {
		builder.addSubsectionTitle(std::move(title));
		builder.addDividerText(std::move(description));
		builder.addSkip();
	};
	addCategory(tr::lng_nagram_appearance(), tr::lng_nagram_appearance_about());
	addCategory(tr::lng_nagram_chats(), tr::lng_nagram_chats_about());
	addCategory(tr::lng_nagram_media(), tr::lng_nagram_media_about());
	addCategory(tr::lng_nagram_privacy(), tr::lng_nagram_privacy_about());
	addCategory(tr::lng_nagram_translation(), tr::lng_nagram_translation_about());
	addCategory(tr::lng_nagram_advanced(), tr::lng_nagram_advanced_about());

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
