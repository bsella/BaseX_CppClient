#pragma once

#include <string>
#include <exception>

class BaseXSession{
public:
	BaseXSession() = default;
	BaseXSession(const std::string& host, const std::string& port, const std::string& user, const std::string& pass);

	~BaseXSession();

	void open(const std::string& host, const std::string& port, const std::string& user, const std::string& pass);

	std::string execute(const std::string& command);

	void close();

	inline bool is_open()const{return m_socket;}

private:
	bool m_db_open = false;

	void* m_socket = nullptr;
};

class BaseXNetworkError : public std::exception{
public:
	BaseXNetworkError(const std::string& message);
	const char* what()const noexcept override;
private:
	const std::string m_message;
};

class BaseXCommandError : public std::exception{
public:
	BaseXCommandError(const std::string& message);
	const char* what()const noexcept override;
private:
	const std::string m_message;
};