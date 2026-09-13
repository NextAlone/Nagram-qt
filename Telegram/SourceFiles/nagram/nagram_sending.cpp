#include "nagram/nagram_sending.h"

#include "core/application.h"
#include "data/data_document.h"
#include "data/stickers/data_stickers.h"
#include "lang/lang_keys.h"
#include "nagram/nagram_settings.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/show.h"

namespace Nagram {

bool ConfirmMediaSend(
		std::shared_ptr<Ui::Show> show,
		DocumentData *document,
		QString recipient,
		Fn<void()> confirmed) {
	if (!document) {
		return false;
	}
	const auto sticker = document->sticker()
		&& (document->sticker()->setType != Data::StickersType::Emoji);
	const auto gif = document->isGifv();
	if ((!sticker && !gif) || !Get(Core::App().settings(), sticker
			? Option::ConfirmStickers
			: Option::ConfirmGifs)) {
		return false;
	}
	const auto callback = std::make_shared<Fn<void()>>(std::move(confirmed));
	show->showBox(Ui::MakeConfirmBox({
		.text = sticker
			? tr::lng_nagram_confirm_sticker_send(lt_user, rpl::single(recipient))
			: tr::lng_nagram_confirm_gif_send(lt_user, rpl::single(recipient)),
		.confirmed = [=](Fn<void()> close) {
			const auto send = base::take(*callback);
			close();
			if (send) {
				send();
			}
		},
		.confirmText = tr::lng_send_button(),
	}));
	return true;
}

} // namespace Nagram
