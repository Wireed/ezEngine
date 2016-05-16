#include <PCH.h>
#include <EditorFramework/EditorApp/EditorApp.moc.h>
#include <Foundation/IO/OSFile.h>

void ezQtEditorApp::AddPluginDataDirDependency(const char* szRelativePath)
{
  ezStringBuilder sPath = szRelativePath;
  sPath.MakeCleanPath();

  for (auto& dd : m_FileSystemConfig.m_DataDirs)
  {
    if (dd.m_sRelativePath == sPath)
    {
      dd.m_bHardCodedDependency = true;
      return;
    }
  }

  ezApplicationFileSystemConfig::DataDirConfig cfg;
  cfg.m_sRelativePath = sPath;
  cfg.m_bWritable = false;
  cfg.m_bHardCodedDependency = true;

  m_FileSystemConfig.m_DataDirs.PushBack(cfg);
}

void ezQtEditorApp::SetFileSystemConfig(const ezApplicationFileSystemConfig& cfg)
{
  if (m_FileSystemConfig == cfg)
    return;

  m_FileSystemConfig = cfg;
  ezQtEditorApp::GetSingleton()->AddReloadProjectRequiredReason("The data directory configuration has changed.");

  m_FileSystemConfig.CreateDataDirStubFiles();
}

void ezQtEditorApp::SetupDataDirectories()
{
  ezStringBuilder sPath = ezToolsProject::GetSingleton()->GetProjectFile();
  sPath.PathParentDirectory();

  ezApplicationConfig::SetProjectDirectory(sPath);
  m_FileSystemConfig.Load();

  ezEditorAppEvent e;
  e.m_pSender = this;
  e.m_Type = ezEditorAppEvent::Type::BeforeApplyDataDirectories;
  m_Events.Broadcast(e);

  ezQtEditorApp::GetSingleton()->AddPluginDataDirDependency("../../Base");

  // Make sure the project directory is always in the list of data directories
  {
    bool bHasProjectDirMounted = false;
    for (auto& dd : m_FileSystemConfig.m_DataDirs)
    {
      if (dd.m_sRelativePath.IsEmpty())
      {
        dd.m_bHardCodedDependency = true;
        bHasProjectDirMounted = true;
        break;
      }
    }

    if (!bHasProjectDirMounted)
    {
      ezApplicationFileSystemConfig::DataDirConfig dd;
      dd.m_bWritable = true;
      dd.m_bHardCodedDependency = true;
      m_FileSystemConfig.m_DataDirs.PushBack(dd);
    }
  }

  // Tell the tools project that all data directories are ok to put documents in
  {
    for (const auto& dd : m_FileSystemConfig.m_DataDirs)
    {
      sPath = ezApplicationConfig::GetProjectDirectory();
      sPath.AppendPath(dd.m_sRelativePath);
      ezToolsProject::GetSingleton()->AddAllowedDocumentRoot(sPath);
    }
  }
}

bool ezQtEditorApp::MakeDataDirectoryRelativePathAbsolute(ezString & sPath) const
{
  if (ezConversionUtils::IsStringUuid(sPath))
  {
    ezUuid guid = ezConversionUtils::ConvertStringToUuid(sPath);
    auto pAsset = ezAssetCurator::GetSingleton()->GetAssetInfo(guid);

    if (!pAsset)
      return false;

    sPath = pAsset->m_sAbsolutePath;
    return true;
  }

  ezStringBuilder sTemp;

  for (ezUInt32 i = m_FileSystemConfig.m_DataDirs.GetCount(); i > 0; --i)
  {
    const auto& dd = m_FileSystemConfig.m_DataDirs[i - 1];

    sTemp = m_FileSystemConfig.GetProjectDirectory();
    sTemp.AppendPath(dd.m_sRelativePath, sPath);
    sTemp.MakeCleanPath();

    if (ezOSFile::ExistsFile(sTemp) || ezOSFile::ExistsDirectory(sTemp))
    {
      sPath = sTemp;
      return true;
    }
  }

  if (!m_FileSystemConfig.m_DataDirs.IsEmpty())
  {
    sTemp = m_FileSystemConfig.GetProjectDirectory();
    sTemp.AppendPath(m_FileSystemConfig.m_DataDirs[0].m_sRelativePath, sPath);
    sTemp.MakeCleanPath();
  }

  return false;
}

bool ezQtEditorApp::MakePathDataDirectoryRelative(ezString & sPath) const
{
  ezStringBuilder sTemp;
  ezStringBuilder sResult = sPath;

  for (ezUInt32 i = m_FileSystemConfig.m_DataDirs.GetCount(); i > 0; --i)
  {
    const auto& dd = m_FileSystemConfig.m_DataDirs[i - 1];

    sTemp = m_FileSystemConfig.GetProjectDirectory();
    sTemp.AppendPath(dd.m_sRelativePath);

    if (sResult.IsPathBelowFolder(sTemp))
    {
      sResult.MakeRelativeTo(sTemp);
      sPath = sResult;
      return true;
    }
  }

  sResult.MakeRelativeTo(m_FileSystemConfig.GetProjectDirectory());
  sPath = sResult;

  return false;
}

