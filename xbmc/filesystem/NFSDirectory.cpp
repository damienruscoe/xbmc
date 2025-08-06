/*
 *  Copyright (C) 2011-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#ifdef TARGET_WINDOWS
#include <mutex>

#include <sys\stat.h>
#endif

#include "FileItem.h"
#include "FileItemList.h"
#include "NFSDirectory.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/XTimeUtils.h"
#include "utils/log.h"

#ifdef TARGET_WINDOWS
#include <sys\stat.h>
#endif

using namespace XFILE;
#include <algorithm>
#include <limits.h>

#include <nfsc/libnfs-raw-nfs.h>
#include <nfsc/libnfs.h>

#if defined(TARGET_WINDOWS)
#define S_IFLNK 0120000
#define S_ISBLK(m) (0)
#define S_ISSOCK(m) (0)
#define S_ISLNK(m) ((m & S_IFLNK) != 0)
#define S_ISCHR(m) ((m & _S_IFCHR) != 0)
#define S_ISDIR(m) ((m & _S_IFDIR) != 0)
#define S_ISFIFO(m) ((m & _S_IFIFO) != 0)
#define S_ISREG(m) ((m & _S_IFREG) != 0)
#endif

namespace {

KODI::TIME::FileTime GetDirEntryTime(struct nfsdirent* dirent)
{
  // if modification date is missing, use create date
  const int64_t lTimeDate =
      (dirent->mtime.tv_sec == 0) ? dirent->ctime.tv_sec : dirent->mtime.tv_sec;

  KODI::TIME::FileTime fileTime, localTime;
  long long ll = lTimeDate & 0xffffffff;
  ll *= 10000000ll;
  ll += 116444736000000000ll;
  fileTime.lowDateTime = (DWORD)(ll & 0xffffffff);
  fileTime.highDateTime = (DWORD)(ll >> 32);
  KODI::TIME::FileTimeToLocalFileTime(&fileTime, &localTime);

  return localTime;
}

} // Anon namespace

CNFSDirectory::CNFSDirectory(void)
{
  gNfsConnection.AddActiveConnection();
}

CNFSDirectory::~CNFSDirectory(void)
{
  gNfsConnection.AddIdleConnection();
}

bool CNFSDirectory::GetDirectoryFromExportList(const CURL& url, CFileItemList& items)
{
  std::string strPath(url.Get());
  URIUtils::AddSlashAtEnd(strPath); //be sure the dir ends with a slash

  CURL url2(strPath);
  std::list<std::string> exportList = gNfsConnection.GetExportList(url2);

  for (const std::string& it : exportList)
  {
    const std::string& currentExport(it);
    URIUtils::RemoveSlashAtEnd(strPath);

    CFileItemPtr pItem(new CFileItem(currentExport));
    std::string path(strPath + currentExport);
    URIUtils::AddSlashAtEnd(path);
    pItem->SetPath(path);
    pItem->SetDateTime(0);
    pItem->SetFolder(true);
    items.Add(pItem);
  }

  return exportList.empty() ? false : true;
}

bool CNFSDirectory::GetServerList(CFileItemList &items)
{
  struct nfs_server_list *srvrs;
  struct nfs_server_list *srv;
  bool ret = false;

  srvrs = nfs_find_local_servers();

  for (srv=srvrs; srv; srv = srv->next)
  {
      std::string currentExport(srv->addr);

      CFileItemPtr pItem(new CFileItem(currentExport));
      std::string path("nfs://" + currentExport);
      URIUtils::AddSlashAtEnd(path);
      pItem->SetDateTime(0);
      pItem->SetPath(path);
      pItem->SetFolder(true);
      items.Add(pItem);
      ret = true; //added at least one entry
  }
  free_nfs_srvr_list(srvrs);

  return ret;
}

bool CNFSDirectory::ResolveSymlink(std::string dirName,
                                   struct nfsdirent* dirent,
                                   std::string& resolvedUrlPath)
{
  std::unique_lock lock(gNfsConnection);
  int ret = 0;
  bool retVal = true;

  URIUtils::AddSlashAtEnd(dirName);

  std::string fullpath = dirName;
  fullpath.append(dirent->name);

  char resolvedLink[MAX_PATH];

  ret = nfs_readlink(gNfsConnection.GetNfsContext(), fullpath.c_str(), resolvedLink, MAX_PATH);

  if(ret == 0)
  {
    nfs_stat_64 tmpBuffer = {};

    CURL resolvedUrl;
    resolvedUrl.SetProtocol("nfs");
    resolvedUrl.SetHostName(gNfsConnection.GetConnectedIp());
    resolvedUrl.SetPort(2049);

    //special case - if link target is absolute it could be even another export
    //intervolume symlinks baby ...
    if(resolvedLink[0] == '/')
    {
      //use the special stat function for using an extra context
      //because we are inside of a dir traversal
      //and just can't change the global nfs context here
      //without destroying something...
      fullpath = resolvedLink;
      resolvedUrl.SetFileName(fullpath);
      ret = gNfsConnection.stat(resolvedUrl, &tmpBuffer);
    }
    else
    {
      fullpath = dirName;
      fullpath.append(resolvedLink);

      ret = nfs_stat64(gNfsConnection.GetNfsContext(), fullpath.c_str(), &tmpBuffer);
      resolvedUrl.SetFileName(gNfsConnection.GetConnectedExport() + fullpath);
    }
    resolvedUrlPath = resolvedUrl.Get();

    if (ret != 0)
    {
      CLog::Log(LOGERROR, "NFS: Failed to stat({}) on link resolve {}", fullpath,
                nfs_get_error(gNfsConnection.GetNfsContext()));
      retVal = false;
    }
    else
    {
      dirent->inode = tmpBuffer.nfs_ino;
      dirent->mode = tmpBuffer.nfs_mode;
      dirent->size = tmpBuffer.nfs_size;
      dirent->atime.tv_sec = tmpBuffer.nfs_atime;
      dirent->mtime.tv_sec = tmpBuffer.nfs_mtime;
      dirent->ctime.tv_sec = tmpBuffer.nfs_ctime;

      //map stat mode to nf3type
      if (S_ISBLK(tmpBuffer.nfs_mode))
      {
        dirent->type = NF3BLK;
      }
      else if (S_ISCHR(tmpBuffer.nfs_mode))
      {
        dirent->type = NF3CHR;
      }
      else if (S_ISDIR(tmpBuffer.nfs_mode))
      {
        dirent->type = NF3DIR;
      }
      else if (S_ISFIFO(tmpBuffer.nfs_mode))
      {
        dirent->type = NF3FIFO;
      }
      else if (S_ISREG(tmpBuffer.nfs_mode))
      {
        dirent->type = NF3REG;
      }
      else if (S_ISLNK(tmpBuffer.nfs_mode))
      {
        dirent->type = NF3LNK;
      }
      else if (S_ISSOCK(tmpBuffer.nfs_mode))
      {
        dirent->type = NF3SOCK;
      }
    }
  }
  else
  {
    CLog::Log(LOGERROR, "Failed to readlink({}) {}", fullpath,
              nfs_get_error(gNfsConnection.GetNfsContext()));
    retVal = false;
  }

  return retVal;
}

bool CNFSDirectory::GetDirectory(const CURL& url, CFileItemList &items)
{
  // We accept nfs://server/path[/file]]]]
  std::unique_lock lock(gNfsConnection);

  std::string path = "";
  if (!gNfsConnection.Connect(url, path))
  {
    //connect has failed - so try to get the exported filesystems if no path is given to the url
    if(url.GetShareName().empty())
    {
      if (url.GetHostName().empty())
        return GetServerList(items);
      else
        return GetDirectoryFromExportList(url, items);
    }
    else
      return false;
  }

  struct nfsdir *nfsdir = NULL;
  int ret = nfs_opendir(gNfsConnection.GetNfsContext(), path.c_str(), &nfsdir);

  if (ret != 0)
  {
    CLog::Log(LOGERROR, "Failed to open({}) {}", path,
              nfs_get_error(gNfsConnection.GetNfsContext()));
    return false;
  }
  lock.unlock();

  std::string linkUrl{};
  std::string itemPath{};
  std::string urlDirPath{url.Get()};
  URIUtils::AddSlashAtEnd(urlDirPath); //be sure the dir ends with a slash

  const std::vector<std::string> exclusions = {".", "..", "lost+found"};
  const auto notExcluded = exclusions.end();

  std::vector<CFileItemPtr> fileItems;
  struct nfsdirent* dirent = NULL;
  while ((dirent = nfs_readdir(gNfsConnection.GetNfsContext(), nfsdir)) != NULL)
  {
    // Resolve symlinks
    // ResolveSymlink changes dirent
    const bool isSymLink = dirent->type == NF3LNK;
    if (isSymLink && !ResolveSymlink(path, dirent, linkUrl))
      continue;

    const std::string name{dirent->name};
    if (std::ranges::find(exclusions, name) == notExcluded)
    {
      const bool isDir = dirent->type == NF3DIR;
      std::string itemPath = isSymLink ? linkUrl : (urlDirPath + name);

      if (isDir)
        URIUtils::AddSlashAtEnd(itemPath);

      CFileItemPtr& pItem = fileItems.emplace_back(new CFileItem(name));
      pItem->SetPath(itemPath);
      pItem->SetDateTime(GetDirEntryTime(dirent));
      pItem->SetSize(dirent->size);
      pItem->SetFolder(isDir);
      if (name[0] == '.')
        pItem->SetProperty("file:hidden", true);
    }
  }
  items.AddItems(fileItems);

  lock.lock();
  nfs_closedir(gNfsConnection.GetNfsContext(), nfsdir); //close the dir
  lock.unlock();
  return true;
}

bool CNFSDirectory::Create(const CURL& url2)
{
  int ret = 0;
  bool success=true;

  std::unique_lock lock(gNfsConnection);
  std::string folderName(url2.Get());
  URIUtils::RemoveSlashAtEnd(folderName);//mkdir fails if a slash is at the end!!!
  CURL url(folderName);
  folderName = "";

  if(!gNfsConnection.Connect(url,folderName))
    return false;

  ret = nfs_mkdir(gNfsConnection.GetNfsContext(), folderName.c_str());

  success = (ret == 0 || -EEXIST == ret);
  if(!success)
    CLog::Log(LOGERROR, "NFS: Failed to create({}) {}", folderName,
              nfs_get_error(gNfsConnection.GetNfsContext()));
  return success;
}

bool CNFSDirectory::Remove(const CURL& url2)
{
  int ret = 0;

  std::unique_lock lock(gNfsConnection);
  std::string folderName(url2.Get());
  URIUtils::RemoveSlashAtEnd(folderName);//rmdir fails if a slash is at the end!!!
  CURL url(folderName);
  folderName = "";

  if(!gNfsConnection.Connect(url,folderName))
    return false;

  ret = nfs_rmdir(gNfsConnection.GetNfsContext(), folderName.c_str());

  if(ret != 0 && errno != ENOENT)
  {
    CLog::Log(LOGERROR, "{} - Error( {} )", __FUNCTION__,
              nfs_get_error(gNfsConnection.GetNfsContext()));
    return false;
  }
  return true;
}

bool CNFSDirectory::Exists(const CURL& url2)
{
  int ret = 0;

  std::unique_lock lock(gNfsConnection);
  std::string folderName(url2.Get());
  URIUtils::RemoveSlashAtEnd(folderName);//remove slash at end or URIUtils::GetFileName won't return what we want...
  CURL url(folderName);
  folderName = "";

  if(!gNfsConnection.Connect(url,folderName))
    return false;

  nfs_stat_64 info;
  ret = nfs_stat64(gNfsConnection.GetNfsContext(), folderName.c_str(), &info);

  if (ret != 0)
  {
    return false;
  }
  return S_ISDIR(info.nfs_mode) ? true : false;
}
