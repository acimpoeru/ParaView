/*=========================================================================

   Program: ParaView
   Module:    $RCSfile$

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2.

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

========================================================================*/
#include "pqPluginManager.h"

#include "pqApplicationCore.h"
#include "pqServer.h"
#include "pqServerManagerModel.h"
#include "pqSettings.h"
#include "vtkPVPluginsInformation.h"
#include "vtkPVPluginTracker.h"
#include "vtkSmartPointer.h"
#include "vtkSMPluginLoaderProxy.h"
#include "vtkSMPluginManager.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSession.h"
#include "vtkWeakPointer.h"

#include <QtDebug>
#include <QSet>

#include <vtksys/ios/sstream>
class pqPluginManager::pqInternals
{
public:
  QPointer<pqServer> ActiveServer;
  vtkWeakPointer<vtkSMPluginManager> SMPluginManager;

  QSet<QString> LocalHiddenPlugins;
  QSet<QString> RemoteHiddenPlugins;

  QString getXML(vtkPVPluginsInformation* info, bool remote)
    {
    vtksys_ios::ostringstream stream;
    stream << "<?xml version=\"1.0\" ?>\n";
    stream << "<Plugins>\n";
    for (unsigned int cc=0; cc < info->GetNumberOfPlugins(); cc++)
      {
      if ((remote &&
          this->RemoteHiddenPlugins.contains(info->GetPluginFileName(cc))) ||
        (!remote &&
         this->LocalHiddenPlugins.contains(info->GetPluginFileName(cc))))
        {
        continue;
        }

      stream << "  <Plugin name=\"" << info->GetPluginName(cc) << "\""
        << " filename=\"" << info->GetPluginFileName(cc) << "\""
        << " auto_load=\"" << (info->GetAutoLoad(cc)? 1 : 0) << "\" />\n";
      }
    stream << "</Plugins>\n";
    //cout << stream.str().c_str() << endl;
    return QString(stream.str().c_str());
    }
};

//-----------------------------------------------------------------------------
pqPluginManager::pqPluginManager(QObject* parentObject)
  : Superclass(parentObject)
{
  this->Internals = new pqInternals();

  pqServerManagerModel* smmodel =
    pqApplicationCore::instance()->getServerManagerModel();

  QObject::connect(smmodel, SIGNAL(serverAdded(pqServer*)),
    this, SLOT(onServerConnected(pqServer*)));
  QObject::connect(smmodel, SIGNAL(serverRemoved(pqServer*)),
    this, SLOT(onServerDisconnected(pqServer*)));

  // Load local plugins information and then load those plugins.
  pqSettings* settings = pqApplicationCore::instance()->settings();
  QString local_plugin_config = settings->value("/PluginsList/Local").toString();
  if (!local_plugin_config.isEmpty())
    {
    vtkPVPluginTracker::GetInstance()->LoadPluginConfigurationXMLFromString(
      local_plugin_config.toAscii().data());
    }
}

//-----------------------------------------------------------------------------
pqPluginManager::~pqPluginManager()
{
  if (this->Internals->ActiveServer)
    {
    this->onServerDisconnected(this->Internals->ActiveServer);
    }
  delete this->Internals;
}

//-----------------------------------------------------------------------------
void pqPluginManager::onServerConnected(pqServer* server)
{
  this->Internals->ActiveServer = server;

  // Tell the server to load all default-plugins.
  if (server->isRemote())
    {
    // locate the xml-config from settings associated with this server and ask
    // the server to parse it.
    pqSettings* settings = pqApplicationCore::instance()->settings();
    QString uri = server->getResource().schemeHostsPorts().toURI();
    QString key = QString("/PluginsList/%1").arg(uri);
    QString remote_plugin_config = settings->value(key).toString();
    // now pass this xml to the vtkPVPluginTracker on the remote
    // processes.
    if (!remote_plugin_config.isEmpty())
      {
      server->session()->GetPluginManager()->LoadPluginConfigurationXMLFromString(
        remote_plugin_config.toAscii().data(), true);
      }
    }

  this->initialize(server->session()->GetPluginManager());
}

//-----------------------------------------------------------------------------
void pqPluginManager::onServerDisconnected(pqServer* server)
{
  if (this->Internals->ActiveServer != server)
    {
    qCritical() << "Multiple server connections not supported yet.";
    return;
    }

  pqSettings* settings = pqApplicationCore::instance()->settings();
  if (server->isRemote())
    {
    // locate the xml-config from settings associated with this server and ask
    // the server to parse it.
    QString uri = server->getResource().schemeHostsPorts().toURI();
    QString key = QString("/PluginsList/%1").arg(uri);
    settings->setValue(key,
      this->Internals->getXML(this->loadedExtensions(true), true));
    }
  settings->setValue("/PluginsList/Local",
    this->Internals->getXML(this->loadedExtensions(false), false));

  this->Internals->ActiveServer = NULL;
  this->Internals->SMPluginManager = NULL;
}

//-----------------------------------------------------------------------------
void pqPluginManager::initialize(vtkSMPluginManager* mgr)
{
  this->Internals->SMPluginManager = mgr;
  mgr->AddObserver(vtkSMPluginManager::PluginLoadedEvent,
    this, &pqPluginManager::updatePluginLists);
  this->updatePluginLists();
}

//-----------------------------------------------------------------------------
void pqPluginManager::updatePluginLists()
{
  emit this->pluginsUpdated();
}

//-----------------------------------------------------------------------------
void pqPluginManager::removePlugin(const QString& lib, bool remote)
{
  (void)lib;
  (void)remote;
  // not supporting "forget-plugin" for now.
}

//-----------------------------------------------------------------------------
vtkPVPluginsInformation* pqPluginManager::loadedExtensions(bool remote)
{
  return remote?
    this->Internals->SMPluginManager->GetRemoteInformation():
    this->Internals->SMPluginManager->GetLocalInformation();
}

//-----------------------------------------------------------------------------
void pqPluginManager::loadExtensions(const QString& path, pqServer* server)
{
  // FIXME: code that loads plugins from the path.
}

//-----------------------------------------------------------------------------
pqPluginManager::LoadStatus pqPluginManager::loadExtension(
  pqServer* server, const QString& lib, QString* errorMsg, bool remote)
{
  bool ret_val;
  if (remote && server->isRemote())
    {
    ret_val = this->Internals->SMPluginManager->LoadRemotePlugin(
      lib.toAscii().data());
    }
  else
    {
    ret_val = this->Internals->SMPluginManager->LoadLocalPlugin(
      lib.toAscii().data());
    }

  return ret_val? LOADED : NOTLOADED;
}

//-----------------------------------------------------------------------------
QStringList pqPluginManager::pluginPaths(bool remote)
{
  QString paths = this->Internals->SMPluginManager->GetPluginSearchPaths(remote);
  return paths.split(';', QString::SkipEmptyParts);
}

//-----------------------------------------------------------------------------
void pqPluginManager::hidePlugin(const QString& lib, bool remote)
{
  if (remote)
    {
    this->Internals->RemoteHiddenPlugins.insert(lib);
    }
  else
    {
    this->Internals->LocalHiddenPlugins.insert(lib);
    }
}

//-----------------------------------------------------------------------------
bool pqPluginManager::isHidden(const QString& lib, bool remote)
{
  return remote?
    this->Internals->RemoteHiddenPlugins.contains(lib) :
    this->Internals->LocalHiddenPlugins.contains(lib);
}