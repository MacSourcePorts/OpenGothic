#include "installdetect.h"

#include <Tempest/Platform>
#include <Tempest/TextCodec>

#ifdef __WINDOWS__
#include "windows.h"
#include "shlobj.h"
#include "shlwapi.h"
#endif

#ifdef __APPLE__
#include "../msputils.h"
#endif

#include <cstring>
#include "utils/fileutil.h"

InstallDetect::InstallDetect() {
#ifdef __WINDOWS__
  pfiles    = programFiles(false);
  pfilesX86 = programFiles(true);
#endif

#ifdef __APPLE__
  pfiles    = applicationSupport();
#endif

  }

std::u16string InstallDetect::detectG2() {
  auto ret = detectG2(pfiles);
  if(ret.empty())
    ret = detectG2(pfilesX86);
  return ret;
  }

std::u16string InstallDetect::detectG2(std::u16string pfiles) {
  if(pfiles.empty())
    return u"";
  auto steam = pfiles+u"/Steam/steamapps/common/Gothic II/";
  if(FileUtil::exists(steam))
    return steam;
  auto akela = pfiles+u"/Akella Games/Gothic II/";
  if(FileUtil::exists(akela))
    return akela;
#if defined(__APPLE__)
  if(FileUtil::exists(pfiles))
    return pfiles;
#endif
  return u"";
  }

#ifdef __WINDOWS__
std::u16string InstallDetect::programFiles(bool x86) {
  WCHAR path[MAX_PATH]={};
  if(FAILED(SHGetFolderPathW(NULL, (x86 ? CSIDL_PROGRAM_FILESX86 : CSIDL_PROGRAM_FILES), NULL, 0, path)))
    return u"";
  std::u16string ret;
  size_t len=0;
  for(;path[len];++len);

  ret.resize(len);
  std::memcpy(&ret[0],path,len*sizeof(char16_t));
  return ret;
  }
#endif


#ifdef __APPLE__
std::u16string InstallDetect::applicationSupport() {
  std::u16string ret; 
  ret = Tempest::TextCodec::toUtf16(std::string(getAppSupportDirectory("OpenGothic")));
  return ret;
  }
#endif
