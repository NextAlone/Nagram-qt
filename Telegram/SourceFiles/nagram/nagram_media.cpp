#include "nagram/nagram_media.h"

#include "core/application.h"
#include "data/data_document.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "nagram/nagram_settings.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

namespace Nagram {

bool ShouldRevealMediaSpoiler(not_null<HistoryItem*> item) {
	return Get(Core::App().settings(), Option::RevealSpoilers)
		&& !item->isMediaSensitive()
		&& !item->isTtlCoveredMedia()
		&& !(item->media() && item->media()->invoice());
}

QString MediaDetailsText(not_null<HistoryItem*> item) {
	const auto media = item->media();
	if (!media) {
		return {};
	}
	auto lines = QStringList();
	const auto add = [&](QString name, QString value) {
		if (!value.isEmpty()) {
			lines.push_back(name + u": "_q + value);
		}
	};
	auto dimensions = QSize();
	auto bytes = int64(0);
	if (const auto document = media->document()) {
		add(tr::lng_nagram_media_filename(tr::now), document->filename());
		add(tr::lng_nagram_media_type(tr::now), document->mimeString());
		dimensions = document->dimensions;
		bytes = document->size;
		if (document->hasDuration()) {
			add(tr::lng_nagram_media_duration(tr::now),
				QString::number(document->duration()) + u" ms"_q);
		}
	} else if (const auto photo = media->photo()) {
		dimensions = photo->size(Data::PhotoSize::Large).value_or(QSize());
		bytes = photo->imageByteSize(Data::PhotoSize::Large);
	} else {
		return {};
	}
	if (!dimensions.isEmpty()) {
		add(tr::lng_nagram_media_dimensions(tr::now),
			QString::number(dimensions.width()) + u" × "_q
				+ QString::number(dimensions.height()) + u" px"_q);
	}
	if (bytes > 0) {
		add(tr::lng_nagram_media_bytes(tr::now), QString::number(bytes));
	}
	return lines.isEmpty()
		? tr::lng_nagram_media_unknown(tr::now)
		: lines.join('\n');
}

void AddMediaDetailsAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<Window::SessionController*> controller,
		HistoryItem *item) {
	if (!item || !item->media()
		|| (!item->media()->document() && !item->media()->photo())
		|| !Get(Core::App().settings(), Option::ShowMediaDetails)) {
		return;
	}
	const auto id = item->fullId();
	menu->addAction(tr::lng_nagram_media_details(tr::now), [=] {
		const auto current = controller->session().data().message(id);
		if (!current) {
			return;
		}
		const auto text = MediaDetailsText(current);
		if (text.isEmpty()) {
			return;
		}
		controller->show(Box([=](not_null<Ui::GenericBox*> box) {
			box->setTitle(tr::lng_nagram_media_details());
			const auto label = box->addRow(object_ptr<Ui::FlatLabel>(
				box, text, st::boxLabel));
			label->setSelectable(true);
			box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		}));
	}, &st::menuIconInfo);
}

} // namespace Nagram
