#include "nagram/nagram_snapshot.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "core/file_utilities.h"
#include "data/data_groups.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/view/history_view_element.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "nagram/nagram_filters.h"
#include "nagram/nagram_menu.h"
#include "ui/chat/chat_style.h"
#include "ui/chat/chat_theme.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "window/section_widget.h"
#include "window/window_session_controller.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QSaveFile>
#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>

#include "styles/style_chat.h"
#include "styles/style_layers.h"
#include "styles/style_nagram_snapshot.h"
#include "styles/style_settings.h"

namespace Nagram {
namespace {

constexpr auto kMaximumMessages = 20;
constexpr auto kMaximumImageBytes = 64 * 1024 * 1024;

class SnapshotDelegate final : public HistoryView::SimpleElementDelegate {
public:
	SnapshotDelegate(
		not_null<Window::SessionController*> controller,
		bool reactions,
		bool spoilers)
	: SimpleElementDelegate(controller, [] {})
	, _reactions(reactions)
	, _spoilers(spoilers) {
	}

	HistoryView::Context elementContext() override {
		return HistoryView::Context::History;
	}
	bool elementAnimationsPaused() override { return true; }
	bool elementHideReactions() override { return !_reactions; }
	bool elementHideSenderNames() override { return true; }
	std::optional<bool> elementSpoilersRevealed() override { return _spoilers; }

private:
	bool _reactions = false;
	bool _spoilers = false;

};

bool CanCapture(not_null<HistoryItem*> item) {
	return item->allowsForward() && !item->isTtlCoveredMedia()
		&& !item->isEphemeral() && !item->isMediaSensitive();
}

bool AllAvailable(
		not_null<Window::SessionController*> controller,
		const MessageIdsList &ids) {
	return !ids.empty() && ranges::all_of(ids, [&](FullMsgId id) {
		const auto item = controller->session().data().message(id);
		return item && CanCapture(item);
	});
}

void SnapshotBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller,
		MessageIdsList ids) {
	box->setTitle(tr::lng_nagram_snapshot());

	const auto bytes = Core::App().settings().readPref<QByteArray>(kSnapshotKey);
	const auto options = box->lifetime().make_state<QJsonObject>(bytes.isEmpty()
		? SnapshotDefaults() : QJsonDocument::fromJson(bytes).object());
	if (!ValidSnapshot(*options)) {
		box->addRow(object_ptr<Ui::FlatLabel>(box,
			tr::lng_nagram_config_value_error(), st::boxLabel));
		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
		return;
	}
	const auto reveal = box->lifetime().make_state<bool>(false);
	const auto image = box->lifetime().make_state<QImage>();
	const auto label = box->addRow(object_ptr<Ui::FlatLabel>(
		box, tr::lng_nagram_snapshot_about(), st::boxLabel));
	const auto preview = box->addRow(object_ptr<Ui::RpWidget>(box));
	const auto resizePreview = [=] {
		const auto height = image->isNull() ? st::nagramSnapshotPreviewMinimum
			: std::clamp(preview->width() * image->height() / image->width(),
				int(st::nagramSnapshotPreviewMinimum), int(st::nagramSnapshotPreviewHeight));
		preview->resize(preview->width(), height);
	};
	preview->widthValue() | rpl::on_next(resizePreview, preview->lifetime());
	preview->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(preview);
		p.fillRect(preview->rect(), st::windowBg->c);
		if (!image->isNull()) {
			const auto size = image->size().scaled(preview->size(), Qt::KeepAspectRatio);
			p.setRenderHint(QPainter::SmoothPixmapTransform);
			p.drawImage(QRect(QPoint((preview->width() - size.width()) / 2, 0), size), *image);
		}
	}, preview->lifetime());
	const auto render = [=] {
		const auto rendered = RenderSnapshot(controller, ids, *options, *reveal);
		if (const auto error = std::get_if<QString>(&rendered)) {
			*image = QImage();
			label->setText(*error);
		} else {
			*image = std::get<QImage>(rendered);
			label->setText(tr::lng_nagram_snapshot_about(tr::now));
		}
		resizePreview();
		preview->update();
	};
	for (const auto &[key, title] : std::array{
		std::pair(u"background"_q, tr::lng_nagram_snapshot_background(tr::now)),
		std::pair(u"date"_q, tr::lng_nagram_snapshot_date(tr::now)),
		std::pair(u"headers"_q, tr::lng_nagram_snapshot_headers(tr::now)),
		std::pair(u"reactions"_q, tr::lng_nagram_snapshot_reactions(tr::now)),
		std::pair(u"simpleReplies"_q, tr::lng_nagram_snapshot_simple_replies(tr::now)),
		std::pair(u"builtinTheme"_q, tr::lng_nagram_snapshot_builtin(tr::now)),
	}) {
		const auto toggle = box->addRow(object_ptr<Ui::Checkbox>(
			box, title, options->value(key).toBool()));
		toggle->checkedChanges() | rpl::on_next([=](bool value) {
			options->insert(key, value);
			auto &settings = Core::App().settings();
			if (*options == SnapshotDefaults()) {
				settings.clearPref(kSnapshotKey);
			} else {
				settings.writePref<QByteArray>(kSnapshotKey,
					QJsonDocument(*options).toJson(QJsonDocument::Compact));
			}
			render();
		}, toggle->lifetime());
	}
	const auto spoiler = box->addRow(object_ptr<Ui::Checkbox>(
		box, tr::lng_nagram_snapshot_spoilers(tr::now), false));
	spoiler->checkedChanges() | rpl::on_next([=](bool value) {
		*reveal = value;
		render();
	}, spoiler->lifetime());
	const auto refresh = box->addRow(object_ptr<Ui::SettingsButton>(
		box, tr::lng_nagram_snapshot_refresh(), st::settingsButtonNoIcon));
	refresh->setClickedCallback(render);
	const auto matchesPreview = [=](const QImage &expected) {
		if (expected.isNull() || !AllAvailable(controller, ids)) {
			return false;
		}
		const auto current = RenderSnapshot(controller, ids, *options, *reveal);
		const auto value = std::get_if<QImage>(&current);
		return value && *value == expected;
	};
	box->addButton(tr::lng_nagram_snapshot_copy(), [=] {
		if (matchesPreview(*image)) {
			QApplication::clipboard()->setImage(*image);
			box->showToast(tr::lng_nagram_snapshot_copied(tr::now));
		} else {
			render();
			box->showToast(tr::lng_nagram_snapshot_changed(tr::now));
		}
	});
	box->addButton(tr::lng_nagram_snapshot_save(), [=] {
		if (!matchesPreview(*image)) {
			render();
			box->showToast(tr::lng_nagram_snapshot_changed(tr::now));
			return;
		}
		const auto snapshot = *image;
		FileDialog::GetWritePath(Core::App().getFileDialogParent(),
			tr::lng_nagram_snapshot_save(tr::now), u"PNG (*.png)"_q,
			u"nagram-message.png"_q,
			crl::guard(box, [=](QString &&path) {
				if (path.isEmpty()) {
					return;
				}
				if (!matchesPreview(snapshot)) {
					render();
					box->showToast(tr::lng_nagram_snapshot_changed(tr::now));
					return;
				}
				auto file = QSaveFile(path);
				if (!file.open(QIODevice::WriteOnly)
					|| !snapshot.save(&file, "PNG") || !file.commit()) {
					box->showToast(tr::lng_nagram_snapshot_write_error(tr::now));
				} else {
					box->showToast(tr::lng_nagram_snapshot_saved(tr::now));
				}
			}));
	});
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	render();
}

} // namespace

QJsonObject SnapshotDefaults() {
	return {
		{ u"version"_q, 1 },
		{ u"background"_q, true },
		{ u"date"_q, true },
		{ u"headers"_q, true },
		{ u"reactions"_q, true },
		{ u"simpleReplies"_q, false },
		{ u"builtinTheme"_q, false },
	};
}

bool ValidSnapshot(const QJsonObject &value) {
	if (value.keys() != SnapshotDefaults().keys() || value.value(u"version"_q) != 1) {
		return false;
	}
	for (auto i = value.begin(); i != value.end(); ++i) {
		if (i.key() != u"version"_q && !i.value().isBool()) {
			return false;
		}
	}
	return true;
}

std::variant<QImage, QString> RenderSnapshot(
		not_null<Window::SessionController*> controller,
		const MessageIdsList &ids,
		const QJsonObject &options,
		bool revealSpoilers) {
	if (!ValidSnapshot(options) || !AllAvailable(controller, ids)) {
		return tr::lng_nagram_snapshot_unavailable(tr::now);
	} else if (ids.size() > kMaximumMessages) {
		return tr::lng_nagram_snapshot_limit(tr::now);
	}
	auto delegate = SnapshotDelegate(controller,
		options.value(u"reactions"_q).toBool(), revealSpoilers);
	auto palette = style::palette();
	palette.finalize();
	auto snapshotStyle = Ui::ChatStyle(controller->session().colorIndicesValue());
	auto builtinTheme = Ui::ChatTheme();
	builtinTheme.setBackground({ .colorForFill = palette.windowBg()->c });
	const auto builtin = options.value(u"builtinTheme"_q).toBool();
	snapshotStyle.applyCustomPalette(builtin ? &palette : controller->chatStyle().get());
	const auto chatStyle = &snapshotStyle;
	const auto theme = builtin ? &builtinTheme : controller->currentChatTheme().get();
	chatStyle->setSimpleQuotes(options.value(u"simpleReplies"_q).toBool());
	const auto padding = st::nagramSnapshotPadding;
	const auto width = st::nagramSnapshotWidth;
	auto height = padding;
	auto views = std::vector<std::unique_ptr<HistoryView::Element>>();
	auto seen = base::flat_set<FullMsgId>();
	const auto date = options.value(u"date"_q).toBool();
	const auto headers = options.value(u"headers"_q).toBool();
	for (const auto id : ids) {
		if (seen.contains(id)) {
			continue;
		}
		const auto item = controller->session().data().message(id);
		if (const auto filtered = FilterMessage(item); filtered && !filtered->ready) {
			return tr::lng_nagram_filter_pending(tr::now);
		}
		if (const auto group = controller->session().data().groups().find(item)) {
			for (const auto part : group->items) {
				if (const auto filtered = FilterMessage(part); filtered && !filtered->ready) {
					return tr::lng_nagram_filter_pending(tr::now);
				}
				if (!ranges::contains(ids, part->fullId())) {
					return tr::lng_nagram_snapshot_album(tr::now);
				}
				seen.insert(part->fullId());
			}
			views.push_back(group->items.front()->createView(&delegate));
		} else {
			seen.insert(id);
			views.push_back(item->createView(&delegate));
		}
		const auto &view = views.back();
		height += view->resizeGetHeight(width) + padding;
		height += (headers || date) ? st::nagramSnapshotHeaderHeight : 0;
		view->hideSpoilers();
	}
	const auto ratio = style::DevicePixelRatio();
	if (int64(width) * height * ratio * ratio * 4 > kMaximumImageBytes) {
		return tr::lng_nagram_snapshot_limit(tr::now);
	}
	auto result = QImage(width * ratio, height * ratio, QImage::Format_ARGB32_Premultiplied);
	if (result.isNull()) {
		return tr::lng_nagram_snapshot_limit(tr::now);
	}
	result.setDevicePixelRatio(ratio);
	result.fill(Qt::transparent);
	auto p = Painter(&result);
	if (options.value(u"background"_q).toBool()) {
		Window::SectionWidget::PaintBackground(p, theme,
			QSize(width, height), QRect(0, 0, width, height), true);
	}
	auto top = padding;
	for (const auto &view : views) {
		const auto item = view->data();
		if (headers || date) {
			p.setFont(st::msgNameFont);
			p.setPen(chatStyle->windowFg());
			auto left = padding;
			if (headers) {
				auto userpic = Ui::PeerUserpicView();
				item->from()->paintUserpic(p, userpic, {
					.position = { left, top },
					.size = st::nagramSnapshotAvatarSize,
				});
				left += st::nagramSnapshotAvatarSize + padding;
			}
			const auto name = headers ? item->from()->name() : QString();
			const auto text = name
				+ ((!name.isEmpty() && date) ? u" · "_q : QString())
				+ (date ? view->dateTime().toString(Qt::ISODate) : QString());
			p.drawText(QRect(left, top, width - left - padding, st::nagramSnapshotHeaderHeight),
				Qt::AlignVCenter | Qt::AlignLeft,
				st::msgNameFont->elided(text, width - left - padding));
			top += st::nagramSnapshotHeaderHeight;
		}
		auto context = theme->preparePaintContext(chatStyle,
			QRect(0, -top, width, height), QRect(0, -top, width, height),
			QRect(0, 0, width, view->height()), true);
		context.outbg = view->hasOutLayout();
		context.simpleQuotes = options.value(u"simpleReplies"_q).toBool();
		p.save();
		p.translate(0, top);
		view->draw(p, context);
		p.restore();
		top += view->height() + padding;
	}
	p.end();
	return result;
}

void AddSnapshotAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<Window::SessionController*> controller,
		MessageIdsList ids) {
	if (ids.size() == 1) {
		if (const auto item = controller->session().data().message(ids.front())) {
			if (const auto group = controller->session().data().groups().find(item)) {
				ids.clear();
				for (const auto part : group->items) {
					ids.push_back(part->fullId());
				}
			}
		}
	}
	if (!AllAvailable(controller, ids)) {
		return;
	}
	if (MenuHidden(Core::App().settings(), MenuAction::Snapshot)) {
		return;
	}
	AddOrderedMenuAction(
		menu,
		MenuAction::Snapshot,
		tr::lng_nagram_snapshot(tr::now),
		crl::guard(controller, [=] {
			controller->show(Box(SnapshotBox, controller, ids));
		}));
}

} // namespace Nagram
