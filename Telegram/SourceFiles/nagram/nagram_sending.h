#pragma once

#include "base/basic_types.h"

class DocumentData;

namespace Ui {
class Show;
} // namespace Ui

namespace Nagram {

[[nodiscard]] bool ConfirmMediaSend(
	std::shared_ptr<Ui::Show> show,
	DocumentData *document,
	QString recipient,
	Fn<void()> confirmed);

} // namespace Nagram
