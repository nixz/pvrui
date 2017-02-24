#include "Connection.h"

#include <vtkProcessModule.h>

Connection::Connection(std::string host, std::string port)
{
      std::stringstream ss;
      ss <<"cs://"<<host<<":"<<port;
      this->_session = vtkSMSessionClient::New();
      this->_session->Connect(ss.str().c_str());
      this->_id= vtkProcessModule::GetProcessModule()->RegisterSession(this->_session);
}

Connection::~Connection()
{
    this->_session->Delete();
}

vtkSMSessionClient* Connection::session()
{
    return this->_session;
}
