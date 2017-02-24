#ifndef CONNECTION_H
#define CONNECTION_H

#include <vtkSMSessionClient.h>
#include <iostream>
#include <sstream>


class Connection
{
public:
    Connection(std::string host, std::string port);
    virtual ~Connection();
    vtkSMSessionClient* session();

private:
    vtkSMSessionClient * _session;
    vtkIdType _id;
};

#endif // CONNECTION_H
