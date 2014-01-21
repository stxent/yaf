/*
 * main.cpp
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <map>
#include <vector>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <openssl/md5.h>
//------------------------------------------------------------------------------
extern "C"
{
#include "fat32.h"
#include "fs.h"
#include "interface.h"
#include "mmi.h"
#include "rtc.h"
}
//------------------------------------------------------------------------------
using namespace boost;
using namespace std;
//------------------------------------------------------------------------------
enum cResult {
  C_OK = 0,
  C_SYNTAX,
  C_TERMINATE,
  C_ERROR
};
//------------------------------------------------------------------------------
enum cResult commandParser(FsHandle *, string &, const string &,
    const string *);
//------------------------------------------------------------------------------
string extractName(const string &path)
{
  int length = 0;
  for (int i = path.size() - 1; i >= 0; i--, length++)
    if (path[i] == '/')
    {
      if (length)
        return path.substr(i + 1);
      else
        return (string)"";
    }
  return path;
}
//------------------------------------------------------------------------------
string parsePath(const string &folder, const string &path)
{
  string res;
  if (path == "")
  {
    res = folder;
  }
  else if (path[0] != '/')
  {
    res = folder;
    if (folder.size() > 1 && *(res.end() - 1) != '/')
      res += "/";
    res += path;
  }
  else
    res = path;
  return res;
}
//------------------------------------------------------------------------------
vector<string> parseArgs(const char **args, int count)
{
  vector<string> res;
  for (int i = 0; i < count; i++)
    res.push_back((string)args[i]);
  return res;
}
//------------------------------------------------------------------------------
string int2str(long int val, int base = 10)
{
  stringstream stream;
  if (base == 16)
    stream << hex << val;
  else if (base == 10)
    stream << dec << val;
  return stream.str();
}
//------------------------------------------------------------------------------
string time2str(uint64_t tm)
{
  char tbuf[24];
  strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", gmtime((time_t *)&tm));
  return (string)tbuf;
}
//------------------------------------------------------------------------------
inline char dec2hex(unsigned char value)
{
  return value < 10 ? '0' + value : 'a' + value - 10;
}
//------------------------------------------------------------------------------
string hexdigest(const unsigned char *src)
{
  string res;
  for (uint8_t i = 0; i < 16; i++)
  {
    res += dec2hex(src[i] >> 4);
    res += dec2hex(src[i] & 0x0F);
  }
  return res;
}
//------------------------------------------------------------------------------
enum cResult util_cd(struct FsHandle *handle, const vector<string> &args,
    string &loc)
{
  string newloc;

  if (args.size() < 2)
    return C_SYNTAX;
  newloc = parsePath(loc, args[1]);

  struct FsEntry *dir = 0;
  struct FsNode *node = (struct FsNode *)fsFollow(handle, newloc.c_str(), 0);

  if (node)
  {
    dir = (struct FsEntry *)fsOpen(node, FS_ACCESS_READ);
    fsFree(node);
  }

  if (dir)
  {
    fsClose(dir);
    loc = newloc;
    return C_OK;
  }
  else
  {
    cout << "cd: " << newloc << ": No such directory" << endl;
    return C_ERROR;
  }
}
//------------------------------------------------------------------------------
vector< map<string, string> > util_ls(struct FsHandle *handle,
    const vector<string> &args, const string &loc)
{
  vector< map<string, string> > entries;
  bool details = false;
  string dirPath;

  for (unsigned int i = 1; i < args.size(); i++)
  {
    if (args[i] == "-l")
    {
      details = true;
      continue;
    }
    dirPath = args[i];
  }
  dirPath = parsePath(loc, dirPath);

  struct FsEntry *dir = 0;
  struct FsNode *node = (struct FsNode *)fsFollow(handle, dirPath.c_str(), 0);

  if (node)
    dir = (struct FsEntry *)fsOpen(node, FS_ACCESS_READ);

  if (dir)
  {
    struct FsMetadata info;
    int pos = 1;
    enum result fsres;

    //Previously allocated node is reused
    for (; (fsres = fsFetch(dir, node)) == E_OK; ++pos)
    {
      map<string, string> retval;
      stringstream estream;
//      path = parsePath(dirPath, (string)fname);
      if (fsGet(node, &info) == E_OK)
      {
        string str_size = int2str(info.size, 10);
        string str_atime = time2str(info.time);

        retval.insert(pair<string, string>("name", info.name));
        retval.insert(pair<string, string>("size", str_size));
//        retval.insert(pair<string, string>("time", str_atime));
        entries.push_back(retval);

        if (details)
        {
#ifdef DEBUG
          //FIXME
          estream.width(10);
          estream << info.pcluster << '.';
          estream.width(3);
          estream << left << info.pindex << right << ' ';
          estream.width(10);
          estream << info.cluster << ' ';

          //Access
          char access[4];
          access[0] = (info.type == FS_TYPE_DIR) ? 'd' : '-';
          access[1] = 'r';
          access[2] = (info.access & 02) ? 'w' : '-';
          access[3] = '\0';
          estream << access;
#endif
          estream.width(10);
#ifdef DEBUG
          estream << str_size << ' ';
#else
          estream << left << str_size << right << ' ';
#endif
          estream << str_atime << ' ';
          estream << info.name;

          cout << estream.str() << endl;
        }
        else
        {
          cout.width(20);
          cout << left << info.name << right;
          if (!(pos % 4))
            cout << endl;
        }
      }
      else
      {
        //Error opening file, insert empty item
        retval.insert(pair<string, string>("name", ""));
      }
    }
    if (!details && ((pos - 1) % 4))
      cout << endl;
    fsClose(dir);
    fsFree(node);
  }
  else
  {
    cout << "ls: " << loc << ": No such directory" << endl;
  }
  return entries;
}
//------------------------------------------------------------------------------
#ifdef FAT_WRITE
enum cResult util_mkdir(struct FsHandle *handle, const vector<string> &args,
    string &loc)
{
  //Process only first argument
  if (args.size() < 2)
    return C_SYNTAX;

  string newloc = parsePath(loc, args[1]); //FIXME unused

  struct FsMetadata info;
  info.access = FS_ACCESS_READ | FS_ACCESS_WRITE;
  info.size = 0;
  info.time = 0;
  info.type = FS_TYPE_DIR;
  strcpy(info.name, args[1].c_str()); //FIXME

  enum result fsres = E_ERROR;
  struct FsNode *node = (struct FsNode *)fsFollow(handle, loc.c_str(), 0); //FIXME

  if (node)
  {
    fsres = fsMake(node, &info, 0);
    fsFree(node);
  }

  if (fsres != E_OK)
  {
    cout << "mkdir: " << newloc << ": Error creating folder" << endl;
    return C_ERROR;
  }
  else
  {
    return C_OK;
  }
}
#endif
//------------------------------------------------------------------------------
#ifdef FAT_WRITE
enum cResult util_rm(struct FsHandle *handle, const vector<string> &args,
    string &loc)
{
  enum cResult res = C_OK;

  for (unsigned int i = 1; i < args.size(); i++)
  {
    enum result fsres = E_ERROR;
    string newloc = parsePath(loc, args[i]);
    struct FsNode *node = (struct FsNode *)fsFollow(handle, newloc.c_str(), 0);

    if (node)
    {
      fsres = fsTruncate(node); //Remove data
      if (fsres == E_OK)
        fsres = fsRemove(node); //Remove metadata
    }

    if (fsres != E_OK)
    {
      cout << "rm: " << newloc << ": No such file" << endl;
      res = C_ERROR;
    }
  }
  return res;
}
#endif
//------------------------------------------------------------------------------
#ifdef FAT_WRITE
enum cResult util_rmdir(struct FsHandle *handle, const vector<string> &args,
    string &loc)
{
  enum cResult res = C_OK;

  for (unsigned int i = 1; i < args.size(); i++)
  {
    enum result fsres = E_ERROR;
    string newloc = parsePath(loc, args[i]);
    struct FsNode *node = (struct FsNode *)fsFollow(handle, newloc.c_str(), 0);

    if (node)
    {
      fsres = fsTruncate(node); //Remove data
      if (fsres == E_OK)
        fsres = fsRemove(node); //Remove metadata
    }

    if (fsres != E_OK)
    {
      cout << "rmdir: " << newloc << ": No such directory or not empty" << endl;
      res = C_ERROR;
    }
  }
  return res;
}
#endif
//------------------------------------------------------------------------------
// #ifdef FAT_WRITE
// enum cResult util_mv(struct FsHandle *handle, const vector<string> &args,
//     string &loc)
// {
//   //Process only first and second arguments
//   if (args.size() < 3)
//     return C_SYNTAX;
// 
//   string src = parsePath(loc, args[1]);
//   string dst = parsePath(loc, args[2]);
// 
//   if (fsMove(handle, src.c_str(), dst.c_str()) == E_OK)
//   {
//     return C_OK;
//   }
//   else
//   {
//     struct FsFile *file;
//     if (!(file = (struct FsFile *)fsOpen(handle, src.c_str(), FS_READ)))
//       cout << "mv: " << src << ": No such file" << endl;
//     else
//     {
//       fsClose(file);
//       if ((file = (struct FsFile *)fsOpen(handle, dst.c_str(), FS_READ)))
//       {
//         cout << "mv: " << dst << ": File exists" << endl;
//         fsClose(file);
//       }
//       else
//         cout << "mv: Error moving file" << endl;
//     }
//     return C_ERROR;
//   }
// }
// #endif
//------------------------------------------------------------------------------
// //Write file from host filesystem to opened volume
// #ifdef FAT_WRITE
// enum cResult util_put(struct FsHandle *handle, const vector<string> &args,
//     string &loc)
// {
//   //Process only first and second arguments
//   if (args.size() < 3)
//     return C_SYNTAX;
// 
//   string host = args[1];
//   string target = parsePath(loc, args[2]);
// 
//   int bufSize = 512;
// 
//   struct FsFile *file;
//   ifstream datafile;
//   datafile.open(host.c_str());
//   if (!datafile)
//   {
//     cout << "put: " << host << ": No such file" << endl;
//     return C_ERROR;
//   }
//   if ((file = (struct FsFile *)fsOpen(handle, target.c_str(), FS_WRITE)))
//   {
//     uint32_t cnt;
//     uint64_t total = 0;
// 
//     char *ibuf = new char[bufSize];
//     while (!datafile.eof())
//     {
//       datafile.read(ibuf, bufSize); //FIXME rewrite
//       cnt = fsWrite(file, (uint8_t *)ibuf, datafile.gcount());
//       total += cnt;
//       if (cnt != datafile.gcount())
//       {
//         cout << "put: " << target << ": Write error on " << total << endl;
//         break;
//       }
//       if (!datafile.gcount())
//         break;
//     }
//     delete ibuf;
//     fsClose(file);
//   }
//   else
//   {
//     cout << "put: " << target << ": Error creating file" << endl;
//     datafile.close();
//     return C_ERROR;
//   }
//   datafile.close();
//   return C_OK;
// }
// #endif
//------------------------------------------------------------------------------
#ifdef FAT_WRITE
enum cResult util_cp(struct FsHandle *handle, const vector<string> &args,
    const string &loc)
{
  //TODO add argument parsing
  string src = args[1];//parsePath(loc, args[1]);
  string dst = args[2];//parsePath(loc, args[2]);
  cResult res = C_OK;

  struct FsEntry *srcFile = 0, *dstFile = 0;
  struct FsNode *rootNode, *node;

  rootNode = (struct FsNode *)fsFollow(handle, loc.c_str(), 0); //FIXME
  if (!rootNode)
    return C_ERROR;

  node = (struct FsNode *)fsFollow(handle, src.c_str(), rootNode);
  if (node)
  {
    srcFile = (struct FsEntry *)fsOpen(node, FS_ACCESS_READ);
    fsFree(node);
  }
  else
  {
    fsFree(rootNode);
  }

  struct FsMetadata info;
  info.access = FS_ACCESS_READ | FS_ACCESS_WRITE;
  info.size = 0;
  info.time = 0;
  info.type = FS_TYPE_FILE;
  strcpy(info.name, dst.c_str()); //FIXME

  if (!(node = (struct FsNode *)fsAllocate(handle)))
  {
    fsClose(srcFile);
    fsFree(rootNode);
    return C_ERROR;
  }

  if (fsMake(rootNode, &info, node) == E_OK)
  {
    dstFile = (struct FsEntry *)fsOpen(node, FS_ACCESS_WRITE);
    fsFree(node);
  }
  else
  {
    fsFree(node);
    fsClose(srcFile);
    fsFree(rootNode);
    return C_ERROR;
  }
  fsFree(rootNode);

  const int bufSize = 5000;
  char buf[bufSize];
  while (!fsEnd(srcFile))
  {
    uint32_t cnt, wcnt;

    cnt = fsRead(srcFile, (uint8_t *)buf, bufSize);
    if (!cnt)
    {
      cout << "cp: Read error" << endl;
      res = C_ERROR;
      break;
    }
    wcnt = cnt;
    cnt = fsWrite(dstFile, (uint8_t *)buf, wcnt);
    if (cnt != wcnt)
    {
      cout << "cp: Write error" << endl;
      res = C_ERROR;
      break;
    }
  }

  fsClose(dstFile);
  fsClose(srcFile);
  return res;
}
#endif
//------------------------------------------------------------------------------
vector< map<string, string> > util_md5sum(struct FsHandle *handle,
    const vector<string> &args, const string &loc)
{
  const int bufSize = 64;
  vector< map<string, string> > entries;

  for (unsigned int i = 1; i < args.size(); i++)
  {
    map<string, string> retval;
    string newloc = parsePath(loc, args[i]);

    struct FsEntry *file = 0;
    struct FsNode *node = (struct FsNode *)fsFollow(handle, newloc.c_str(), 0);
    if (node)
      file = (struct FsEntry *)fsOpen(node, FS_ACCESS_READ);

    if (file)
    {
      uint32_t cnt;
      char buf[bufSize];
      MD5_CTX md5result;
      unsigned char md5str[16];

      MD5_Init(&md5result);
      while (!fsEnd(file))
      {
        cnt = fsRead(file, (uint8_t *)buf, bufSize);
        if (cnt)
          MD5_Update(&md5result, (const void *)buf, cnt);
        else
        {
          cout << "md5sum: Read error" << endl;
          break;
        }
      }
      fsClose(file);
      MD5_Final(md5str, &md5result);

      string digest = hexdigest(md5str);
      string extracted = extractName(newloc);
      cout << digest << '\t' << extracted << endl;
      retval.insert(pair<string, string>("name", extracted));
      retval.insert(pair<string, string>("checksum", digest));
    }
    else
    {
      //Error opening file, insert empty item
      retval.insert(pair<string, string>("name", ""));
      cout << "md5sum: " << newloc << ": No such file" << endl;
    }
    entries.push_back(retval);
  }
  return entries;
    }
//------------------------------------------------------------------------------
// #ifdef FAT_WRITE
// enum cResult util_dd(struct FsHandle *handle, const vector<string> &args,
//     const string &loc)
// {
//   //Test for fsSeek and fsTell
//   const char *originValues[] = {"FS_SEEK_SET", "FS_SEEK_CUR", "FS_SEEK_END"};
//   string src = "", dst = "";
//   uint32_t blockSize = 512;
//   asize_t offset = 0, length = 0, position;
//   enum fsSeekOrigin origin = FS_SEEK_SET;
//   struct FsStat stat;
// 
//   for (unsigned int i = 1; i < args.size(); i++)
//   {
//     //Input file
//     if (args[i] == "-i" && i + 1 < args.size())
//     {
//       src = parsePath(loc, args[++i]);
//       continue;
//     }
//     //Output file
//     if (args[i] == "-o" && i + 1 < args.size())
//     {
//       dst = parsePath(loc, args[++i]);
//       continue;
//     }
//     //Block size
//     if (args[i] == "--block" && i + 1 < args.size())
//     {
//       try
//       {
//         blockSize = lexical_cast<uint32_t>(args[++i]);
//       }
//       catch (bad_lexical_cast &)
//       {
//         cout << "dd: Error: unsupported block size" << endl;
//         return C_ERROR;
//       }
//       continue;
//     }
//     //Chunk length
//     if (args[i] == "--length" && i + 1 < args.size())
//     {
//       try
//       {
//         length = lexical_cast<asize_t>(args[++i]);
//       }
//       catch (bad_lexical_cast &)
//       {
//         cout << "dd: Error: unsupported length" << endl;
//         return C_ERROR;
//       }
//       continue;
//     }
//     //Origin
//     if (args[i] == "--origin" && i + 1 < args.size())
//     {
//       i++;
//       if (args[i] == "FS_SEEK_SET")
//         origin = FS_SEEK_SET;
//       else if (args[i] == "FS_SEEK_END")
//         origin = FS_SEEK_END;
//       else
//       {
//         cout << "dd: Error: unsupported origin" << endl;
//         return C_ERROR;
//       }
//       continue;
//     }
//     //Offset from origin
//     if (args[i] == "--offset" && i + 1 < args.size())
//     {
//       try
//       {
//         offset = lexical_cast<asize_t>(args[++i]);
//       }
//       catch (bad_lexical_cast &)
//       {
//         cout << "dd: Error: unsupported block count" << endl;
//         return C_ERROR;
//       }
//       continue;
//     }
//   }
// 
//   if (fsStat(handle, &stat, src.c_str()) != E_OK)
//   {
//     cout << "dd: Error: stat failed" << endl;
//     return C_ERROR;
//   }
//   if (length < 0)
//   {
//     cout << "dd: Error: unsupported length" << endl;
//     return C_ERROR;
//   }
//   if (!length)
//     length = stat.size;
//   switch (origin)
//   {
//     case FS_SEEK_SET:
//       position = offset;
//       break;
//     case FS_SEEK_END:
//       position = stat.size + offset;
//       break;
//     default:
//       return C_ERROR; //Never reached there
//   }
// #ifdef DEBUG
//   cout << "dd: input: " << src << ", output: " << dst << endl;
//   cout << "dd: block size: " << blockSize << ", length: " << length << endl;
//   cout << "dd: origin: " << originValues[origin]
//       << ", offset: " << offset << ", position: " << position << endl;
// #endif
// 
//   struct FsFile *srcFile, *dstFile;
//   if (!(srcFile = (struct FsFile *)fsOpen(handle, src.c_str(), FS_READ)))
//   {
//     return C_ERROR;
//   }
//   if (!(dstFile = (struct FsFile *)fsOpen(handle, dst.c_str(), FS_WRITE)))
//   {
//     fsClose(srcFile);
//     return C_ERROR;
//   }
// 
//   char *buf = new char[blockSize];
//   cResult res = C_OK;
//   enum result fsres;
// 
//   try
//   {
//     asize_t check;
// 
//     fsres = fsSeek(srcFile, offset, origin);
//     if (fsres != E_OK)
//       throw "seek failed";
// 
//     check = fsTell(srcFile);
//     if (check != position)
//       throw "tell failed";
// 
//     uint32_t cnt, wcnt, rcnt, rdone = 0;
//     while (!fsEof(srcFile) && rdone < length)
//     {
//       rcnt = length - rdone;
//       rcnt = rcnt > blockSize ? blockSize : rcnt;
//       cnt = fsRead(srcFile, (uint8_t *)buf, rcnt);
//       if (!cnt)
//         throw "source read failed";
//       rdone += cnt;
//       wcnt = cnt;
//       cnt = fsWrite(dstFile, (uint8_t *)buf, wcnt);
//       if (cnt != wcnt)
//         throw "read and written lengths differ";
//     }
// 
//     fsres = fsSeek(srcFile, -length, FS_SEEK_CUR);
//     if (fsres != E_OK)
//       throw "validation seek failed";
//     check = fsTell(srcFile);
//     if (check != position)
//       throw "validation tell failed";
//   }
//   catch (const char *err)
//   {
//     cout << "dd: Error: " << err << endl;
//     res = C_ERROR;
//   }
// 
//   delete[] buf;
//   fsClose(dstFile);
//   fsClose(srcFile);
//   return res;
// }
// #endif
//------------------------------------------------------------------------------
#ifdef DEBUG
int util_io(struct FsHandle *handle)
{
//  char size2str[16];
//  uint64_t stat[4];

//  mmiGetStat(handle->interface, stat);
//  cout << "Read requests:      " << stat[0] << endl;
//  getSizeStr(stat[1], size2str);
//  cout << "Data read:          " << size2str << endl;
//  cout << "Write requests:     " << stat[2] << endl;
//  getSizeStr(stat[3], size2str);
//  cout << "Data written:       " << size2str << endl;

  return E_OK;
}
#else
int util_io(struct FsHandle *handle __attribute__((unused)))
{
  return E_OK;
}
#endif
//------------------------------------------------------------------------------
int util_info(struct FsHandle *handle)
{
//  struct FatHandle *handle = (struct FatHandle *)object;
#if defined (FAT_WRITE) && defined (DEBUG)
  uint32_t sz;
  sz = countFree(handle);
#endif
//  cout << "Sectors in cluster: " << (int)(1 << handle->clusterSize) << endl;
//  cout << "FAT sector:         " << handle->tableSector << endl;
//  cout << "Data sector:        " << handle->dataSector << endl;
//  cout << "Root cluster:       " << handle->rootCluster << endl;
#ifdef FAT_WRITE
//  cout << "FAT records count:  " << (int)handle->tableCount << endl;
//  cout << "Sectors in FAT:     " << handle->tableSize << endl;
//  cout << "Info sector:        " << handle->infoSector << endl;
//  cout << "Data clusters:      " << handle->clusterCount << endl;
//  cout << "Last allocated:     " << handle->lastAllocated << endl;
#if defined (FAT_WRITE) && defined (DEBUG)
  cout << "Free clusters:      " << sz << endl;
#endif
#endif
  return E_OK;
}
//------------------------------------------------------------------------------
void util_autotest(FsHandle *handle, const vector<string> &args)
{
  if (args.size() < 2)
    return;

  unsigned int passed = 0, failed = 0, total = 0;
  string loc = "/";
  ifstream testbench;
  testbench.open(args[1].c_str());
  if (!testbench)
  {
    cout << "autotest: " << args[1] << ": No such file" << endl;
    return;
  }

  string data;
  regex parser("\"(.*?)\"");
  smatch results;

  while (!testbench.eof())
  {
    getline(testbench, data);
    string::const_iterator dataStart = data.begin();
    string::const_iterator dataEnd = data.end();

    string comStr = "", argStr = "";

    while (regex_search(dataStart, dataEnd, results, parser))
    {
      if (comStr == "")
      {
        comStr = string(results[1].first, results[1].second);
      }
      else
      {
        argStr = string(results[1].first, results[1].second);
        break;
      }
      dataStart = results[1].second + 1;
    }
    if (comStr != "")
    {
      cout << "> " << comStr << endl;
      total++;
      if (commandParser(handle, loc, comStr, &argStr) == C_OK)
        passed++;
      else
        failed++;
    }
  }
  testbench.close();
  cout << '/' << string(78, '*') << '/' << endl;
  cout << "Test result: total " << total << ", passed " << passed
      << ", failed " << failed << endl;
}
//------------------------------------------------------------------------------
enum cResult commandParser(FsHandle *handle, string &loc, const string &str,
    const string *ret = NULL)
{
  vector<string> args;
  vector<string> retvals;
  stringstream parser;

  parser.clear();
  parser << str;
  if (parser.eof())
    return C_SYNTAX;

  for (int count = 0; !parser.eof(); count++)
  {
    string argstr;
    parser >> argstr;
    args.push_back(argstr);
  }

  if (ret)
  {
    parser.clear();
    parser << *ret;
    if (!parser.eof())
    {
      for (int count = 0; !parser.eof(); count++)
      {
        string argstr;
        parser >> argstr;
        retvals.push_back(argstr);
      }
    }
  }

  if (args[0] == "stat")
  {
    util_io(handle);
  }
  if (args[0] == "info")
  {
    util_info(handle);
  }
  if (args[0] == "cd")
  {
    enum cResult retval;
    retval = util_cd(handle, args, loc);
    if (retval != C_OK)
    {
      cout << "Error" << endl;
      return C_ERROR;
    }
  }
  if (args[0] == "ls")
  {
    vector< map<string, string> > result;
    result = util_ls(handle, args, loc);

    if (retvals.size() > 0)
    {
      if (retvals.size() != result.size())
      {
        cout << "Error" << endl;
        return C_ERROR;
      }

      vector< map<string, string> >::iterator i;
      unsigned int found = 0, pos = 0;
      for (i = result.begin(); i != result.end(); i++, pos++)
      {
        if (retvals[pos] == (*i)["name"])
          found++;
        else
          cout << "ls: " << retvals[pos]
              << ": No such file or directory" << endl;
      }

      if (found != retvals.size())
      {
        cout << "Error" << endl;
        return C_ERROR;
      }
      else
        cout << "ls: completed successfully" << endl;
    }
  }
// #ifdef FAT_WRITE
  if (args[0] == "mkdir")
  {
    enum cResult retval;
    retval = util_mkdir(handle, args, loc);
    if (retval != C_OK)
    {
      cout << "Error" << endl;
      return C_ERROR;
    }
  }
  if (args[0] == "rm")
  {
    enum cResult retval;
    retval = util_rm(handle, args, loc);
    if (retval != C_OK)
    {
      cout << "Error" << endl;
      return C_ERROR;
    }
  }
  if (args[0] == "rmdir")
  {
    enum cResult retval;
    retval = util_rmdir(handle, args, loc);
    if (retval != C_OK)
    {
      cout << "Error" << endl;
      return C_ERROR;
    }
  }
//   if (args[0] == "mv")
//   {
//     enum cResult retval;
//     retval = util_mv(handle, args, loc);
//     if (retval != C_OK)
//     {
//       cout << "Error" << endl;
//       return C_ERROR;
//     }
//   }
//   if (args[0] == "put")
//   {
//     enum cResult retval;
//     retval = util_put(handle, args, loc);
//     if (retval != C_OK)
//     {
//       cout << "Error" << endl;
//       return C_ERROR;
//     }
//   }
  if (args[0] == "cp")
  {
    enum cResult retval;
    retval = util_cp(handle, args, loc);
    if (retval != C_OK)
    {
      cout << "Error" << endl;
      return C_ERROR;
    }
  }
//   if (args[0] == "dd")
//   {
//     enum cResult retval;
//     retval = util_dd(handle, args, loc);
//     if (retval != C_OK)
//     {
//       cout << "Error" << endl;
//       return C_ERROR;
//     }
//   }
// #endif
  if (args[0] == "md5sum")
  {
    vector< map<string, string> > result;
    result = util_md5sum(handle, args, loc);

    if (retvals.size() > 0)
    {
      if (retvals.size() != result.size())
      {
        cout << "Error" << endl;
        return C_ERROR;
      }

      vector< map<string, string> >::iterator i;
      unsigned int found = 0, pos = 0;
      for (i = result.begin(); i != result.end(); i++, pos++)
      {
        if (retvals[pos] == (*i)["checksum"])
          found++;
        else
        {
          cout << "md5sum: " << (*i)["name"];
          cout << ": Checksum difference, expected " << retvals[pos];
          cout << ", got " << (*i)["checksum"] << endl;
          break;
        }
      }

      if (found != retvals.size())
      {
        cout << "Error" << endl;
        return C_ERROR;
      }
      else
      {
        cout << "md5sum: completed successfully" << endl;
      }
    }
    //     vector<string> required;
    //     required.push_back(args[i]);
  }
//   if (args[0] == "autotest")
//   {
//     util_autotest(handle, args);
//   }
  if (args[0] == "exit")
    return C_TERMINATE;
  return C_OK;
}
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  if (argc < 2)
    return 0;

  struct Interface *mmaped;
  struct FsHandle *handle;

  mmaped = (struct Interface *)init(Mmi, argv[1]);
  if (!mmaped)
  {
    printf("Error opening file\n");
    return 0;
  }

  struct MbrDescriptor mbrRecord;
  if (mmiReadTable(mmaped, 0, 0, &mbrRecord) == E_OK)
  {
    if (mmiSetPartition(mmaped, &mbrRecord) != E_OK)
      printf("Error setup partition\n");
  }
  else
    printf("No partitions found, selected raw partition at 0\n");

  struct Fat32Config fsConf;
  fsConf.interface = mmaped;
  handle = (struct FsHandle *)init(FatHandle, &fsConf);
  if (!handle)
  {
    printf("Error creating FAT32 handle\n");
    return 0;
  }

  string location = "/";
  bool terminate = false;
  enum cResult res;

  while (!terminate)
  {
    string command;
    string path;
    cout << location << "> ";
    getline(cin, command);
    res = commandParser(handle, location, command);
    switch (res)
    {
      case C_TERMINATE:
        terminate = true;
        break;
      case C_SYNTAX:
        cout << "Syntax error" << endl;
        break;
      case C_ERROR:
//         cout << "Error occured" << endl;
        break;
      case C_OK:
      default:
        break;
    }
  }

  printf("Unloading\n");
  deinit(handle);
  return 0;
}
