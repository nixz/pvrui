#include "servermanager.h"

#include <vtkSMSession.h>
#include <vtkSMSessionClient.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMProxySelectionModel.h>
#include <vtkSMProxyManager.h>
#include <vtkProcessModule.h>
#include <vtkNetworkAccessManager.h>
#include <vtkSMRenderViewProxy.h>

Connection* connect(std::string host, std::string port)
{
  ActiveConnection = new Connection(host,port);
  return ActiveConnection;
}

vtkSMProxySelectionModel* selectionModel(std::string name)
{
  if(!ActiveConnection->session())
    {
    std::cout<<"Active Session not found."<<std::endl;
    exit(1);
    }
  vtkSMSessionClient *session = ActiveConnection->session();
  vtkSMSessionProxyManager* pxm = session->GetSessionProxyManager();
  vtkSMProxySelectionModel* model = pxm->GetSelectionModel(name.c_str());
  if (!model)
    {
    std::cout << name << " not found" << std::endl;
    model = vtkSMProxySelectionModel::New();
    pxm->RegisterSelectionModel(name.c_str(), model);
    model->FastDelete();
    }
  return model;
}

vtkSMViewProxy* activeView()
{
  return vtkSMViewProxy::SafeDownCast(selectionModel("ActiveView")->GetCurrentProxy());
}

vtkSMSourceProxy* activeSource()
{
  return vtkSMSourceProxy::SafeDownCast(selectionModel("ActiveSources")->GetCurrentProxy());
}

void update()
{
  vtkSMSessionClient* session = vtkSMSessionClient::SafeDownCast(vtkSMProxyManager::GetProxyManager()->GetActiveSession());
  if(session->IsMultiClients()&& session->IsNotBusy())
    {
    bool updated = false;
    while(vtkProcessModule::GetProcessModule()->GetNetworkAccessManager()->ProcessEvents(100))
      {
      updated = true;
     // std::cout<<"Update ..."<<std::endl;
      }
    if(updated)
      {
      //_renderer->reload();
      //vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager()->UpdateFromRemote();
      vtkSMRenderViewProxy* rvp = vtkSMRenderViewProxy::SafeDownCast(
            vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager()->
            GetSelectionModel("ActiveView")->GetCurrentProxy());
    //  mutex.acquire();
      rvp->UpdateVTKObjects();
      rvp->StillRender();
      newData = true;
     // mutex.release();
      }
    }
}
