#pragma once

#include <string>

struct SmtpEmailConfig
{
	std::string url;
	std::string from;
	std::string username;
	std::string password;
	bool useTls;
};

class SmtpEmailSender
{
	public:
		explicit SmtpEmailSender(SmtpEmailConfig config);

		void sendTextEmail(
			const std::string &recipient,
			const std::string &subject,
			const std::string &body
		) const;

	private:
		SmtpEmailConfig _config;
};
