/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/platform/win/base_windows_rpcndr_h.h"
#include "windows_toastactivator_h.h"

#include "base/platform/win/wrl/wrl_implements_h.h"

// {1E0F4420-99AB-5C2B-B22F-4D575D4522DB}
class DECLSPEC_UUID("1E0F4420-99AB-5C2B-B22F-4D575D4522DB") ToastActivator
	: public ::Microsoft::WRL::RuntimeClass<
		::Microsoft::WRL::RuntimeClassFlags<::Microsoft::WRL::ClassicCom>,
		INotificationActivationCallback,
		::Microsoft::WRL::FtmBase> {
public:
	ToastActivator() = default;
	~ToastActivator() = default;

	HRESULT STDMETHODCALLTYPE Activate(
		_In_ LPCWSTR appUserModelId,
		_In_ LPCWSTR invokedArgs,
		_In_reads_(dataCount) const NOTIFICATION_USER_INPUT_DATA *data,
		ULONG dataCount) override;

	HRESULT STDMETHODCALLTYPE QueryInterface(
		REFIID riid,
		void **ppObj);
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();

private:
	long _ref = 1;

};

struct ToastActivation {
	struct UserInput {
		QString key;
		QString value;
	};
	QString args;
	std::vector<UserInput> input;

	[[nodiscard]] static QString String(LPCWSTR value);
};
[[nodiscard]] rpl::producer<ToastActivation> ToastActivations();
