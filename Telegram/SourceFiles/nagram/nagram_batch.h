#pragma once

#include "ui/text/text_entity.h"

namespace Main { class Session; }
namespace Data { class Thread; }
namespace Ui { class PopupMenu; }
namespace Window { class SessionController; }

namespace Nagram {

struct MessageBatch {
	MessageIdsList ids;
	TextWithEntities text;
};

[[nodiscard]] std::variant<MessageBatch, QString> PrepareMessageBatch(
	not_null<Main::Session*> session,
	MessageIdsList ids,
	bool reversed,
	bool headers);
[[nodiscard]] QString ApplyMessageBatchDraft(
	not_null<Window::SessionController*> controller,
	not_null<Data::Thread*> target,
	const MessageBatch &batch,
	bool forward,
	bool omitNames,
	bool reply);
void AddMessageBatchAction(
	not_null<Ui::PopupMenu*> menu,
	not_null<Window::SessionController*> controller,
	MessageIdsList ids);

} // namespace Nagram
