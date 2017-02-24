#ifndef SERVERMANAGER_H
#define SERVERMANAGER_H

#include <vtkSMProxySelectionModel.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>

#include "Connection.h"
#include <string>

Connection *ActiveConnection =NULL;
bool newData; // used by update

Connection* connect(std::string host, std::string port);
vtkSMProxySelectionModel* selectionModel(std::string name);
vtkSMViewProxy* activeView();
vtkSMSourceProxy* activeSource();
void update();
#endif // SERVERMANAGER_H

