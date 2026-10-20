#include "SmtpEmailSender.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace
{
	struct UploadState
	{
		std::string message;
		std::size_t offset = 0;
	};

	std::size_t readMessage(
		char *buffer,
		std::size_t size,
		std::size_t count,
		void *userData
	)
	{
		auto &state = *static_cast<UploadState *>(userData);
		const std::size_t capacity = size * count;
		const std::size_t remaining =
			state.message.size() - state.offset;
		const std::size_t bytes = std::min(capacity, remaining);
		std::copy_n(
			state.message.data() + state.offset,
			bytes,
			buffer
		);
		state.offset += bytes;
		return bytes;
	}

	void requireSafeHeader(const std::string &value, const char *name)
	{
		if (value.empty() ||
			value.find_first_of("\r\n") != std::string::npos)
			throw std::invalid_argument(
				std::string("Invalid email header: ") + name
			);
	}
}

SmtpEmailSender::SmtpEmailSender(SmtpEmailConfig config):
	_config(std::move(config))
{
	static std::once_flag curlInitialization;
	static CURLcode initializationResult = CURLE_FAILED_INIT;
	std::call_once(curlInitialization, []
	{
		initializationResult = curl_global_init(CURL_GLOBAL_DEFAULT);
	});
	if (initializationResult != CURLE_OK)
		throw std::runtime_error("Unable to initialize libcurl");

	if (this->_config.url.empty() ||
		(this->_config.url.rfind("smtp://", 0) != 0 &&
			this->_config.url.rfind("smtps://", 0) != 0))
		throw std::invalid_argument(
			"Email SMTP URL must use smtp:// or smtps://"
		);
	requireSafeHeader(this->_config.from, "From");
	if (this->_config.username.empty() != this->_config.password.empty())
		throw std::invalid_argument(
			"SMTP username and password must be configured together"
		);
	if (!this->_config.useTls &&
		this->_config.url.rfind("smtp://mailpit:", 0) != 0)
		throw std::invalid_argument(
			"TLS is required unless using the local Mailpit service"
		);
}

void SmtpEmailSender::sendTextEmail(
	const std::string &recipient,
	const std::string &subject,
	const std::string &body
) const
{
	requireSafeHeader(recipient, "To");
	requireSafeHeader(subject, "Subject");
	if (body.find('\0') != std::string::npos)
		throw std::invalid_argument("Email body contains a null byte");

	CURL *handle = curl_easy_init();
	if (handle == nullptr)
		throw std::runtime_error("Unable to create SMTP request");

	curl_slist *recipients = nullptr;
	recipients = curl_slist_append(recipients, recipient.c_str());
	if (recipients == nullptr)
	{
		curl_easy_cleanup(handle);
		throw std::runtime_error("Unable to configure SMTP recipient");
	}

	UploadState upload{
		"To: " + recipient + "\r\n"
		"From: " + this->_config.from + "\r\n"
		"Subject: " + subject + "\r\n"
		"Content-Type: text/plain; charset=UTF-8\r\n"
		"\r\n" + body + "\r\n"
	};

	curl_easy_setopt(handle, CURLOPT_URL, this->_config.url.c_str());
	curl_easy_setopt(handle, CURLOPT_MAIL_FROM, this->_config.from.c_str());
	curl_easy_setopt(handle, CURLOPT_MAIL_RCPT, recipients);
	curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(handle, CURLOPT_READFUNCTION, readMessage);
	curl_easy_setopt(handle, CURLOPT_READDATA, &upload);
	curl_easy_setopt(
		handle,
		CURLOPT_INFILESIZE_LARGE,
		static_cast<curl_off_t>(upload.message.size())
	);
	curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(handle, CURLOPT_TIMEOUT, 10L);
	if (!this->_config.username.empty())
	{
		curl_easy_setopt(
			handle,
			CURLOPT_USERNAME,
			this->_config.username.c_str()
		);
		curl_easy_setopt(
			handle,
			CURLOPT_PASSWORD,
			this->_config.password.c_str()
		);
	}
	if (this->_config.useTls)
		curl_easy_setopt(handle, CURLOPT_USE_SSL, CURLUSESSL_ALL);

	const CURLcode result = curl_easy_perform(handle);
	const std::string error =
		result == CURLE_OK ? std::string{} : curl_easy_strerror(result);
	curl_slist_free_all(recipients);
	curl_easy_cleanup(handle);
	if (result != CURLE_OK)
		throw std::runtime_error("SMTP delivery failed: " + error);
}
