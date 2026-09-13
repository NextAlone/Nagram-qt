#include "nagram/nagram_settings.h"

#include "core/core_settings.h"

namespace Nagram {
namespace {

const OptionDefinition &Definition(Option option) {
	const auto index = static_cast<std::size_t>(option);
	Expects(index < kOptions.size());
	return kOptions[index];
}

} // namespace

bool Get(Core::Settings &settings, Option option) {
	const auto &definition = Definition(option);
	return settings.readPref<bool>(definition.key, definition.defaultValue);
}

void Set(Core::Settings &settings, Option option, bool value) {
	const auto &definition = Definition(option);
	if (value == definition.defaultValue) {
		settings.clearPref(definition.key);
	} else {
		settings.writePref<bool>(definition.key, value);
	}
}

void Reset(Core::Settings &settings, Option option) {
	settings.clearPref(Definition(option).key);
}

} // namespace Nagram
