#include "BaseXSession.h"

#include "basexdbc.h"

BaseXSession::BaseXSession(const std::string& host, const std::string& port, const std::string& user, const std::string& pass)
{
	open(host, port, user, pass);
}

BaseXSession::~BaseXSession()
{
	if(is_open())
		close();
}

void BaseXSession::open(const std::string& host, const std::string& port, const std::string& user, const std::string& pass)
{
	m_socket = basex_connect(host.c_str(), port.c_str());
	if(!m_socket)
	{
		throw BaseXNetworkError("Cannot connect to BaseX server at "+ host + " with port " + port);
	}
	if (basex_authenticate(m_socket, user.c_str(), pass.c_str()) == -1) {
		throw BaseXNetworkError("Access denied, please verify username and password");
	}
}

std::string BaseXSession::execute(const std::string& command)
{
	char* result;
	char* info;
	
	int res = basex_execute(m_socket, command.c_str(), &result, &info);

	if(res == -1)
	{
		throw BaseXNetworkError("Connection to BaseX server lost");
	}

	if(res > 0)
	{
		BaseXCommandError error(info);
		free(info);
		throw error;
	}

	std::string ret = result;
	free(result);
	free(info);
	return ret;
}

void BaseXSession::close()
{
	basex_close(m_socket);
	m_socket = nullptr;
}


BaseXNetworkError::BaseXNetworkError(const std::string& message) : m_message(message)
{
}
const char* BaseXNetworkError::what()const noexcept
{
	return m_message.c_str();
}

BaseXCommandError::BaseXCommandError(const std::string& message) : m_message(message)
{
}
const char* BaseXCommandError::what()const noexcept
{
	return m_message.c_str();
}
