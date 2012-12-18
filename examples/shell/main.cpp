/*
 * main.cpp
 * Copyright (C) 2012 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <cstring>

#include <algorithm>
#include <map>
#include <vector>
//------------------------------------------------------------------------------
#include <openssl/md5.h>
//------------------------------------------------------------------------------
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
//------------------------------------------------------------------------------
extern "C"
{
#include "fs.h"
#include "rtc.h"
#include "interface.h"
#include "mmi.h"
#include "fat32.h"
}
//------------------------------------------------------------------------------
#ifdef DEBUG
extern uint64_t readCount, writeCount;
#endif
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
using namespace std;
using namespace boost;
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
  strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S",
      gmtime((time_t *)&tm));
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
enum cResult util_cd(struct FsHandle *handler, const vector<string> &args,
    string &loc)
{
  struct FsDir *dir;
  string newloc;

  if (args.size() < 2)
    return C_SYNTAX;
  newloc = parsePath(loc, args[1]);

  dir = fsOpenDir(handler, newloc.c_str());
  if (dir)
  {
    fsCloseDir(dir);
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
vector< map<string, string> > util_ls(struct FsHandle *handler,
    const vector<string> &args, const string &loc)
{
  vector< map<string, string> > entries;
  struct FsDir *dir;
  struct FsStat stat;
  bool details = false;
  enum result fsres;
  string path, dirPath;

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
  dir = fsOpenDir(handler, dirPath.c_str());
  if (dir)
  {
    char fname[13];
    int pos;
    for (pos = 1; (fsres = fsReadDir(dir, fname)) == E_OK; pos++)
    {
      map<string, string> retval;
      stringstream estream;
      path = parsePath(dirPath, (string)fname);
      if (fsStat(handler, path.c_str(), &stat) == E_OK)
      {
        string str_size = int2str(stat.size, 10);
        string str_atime = time2str(stat.atime);

        retval.insert(pair<string, string>("name", fname));
        retval.insert(pair<string, string>("size", str_size));
//         retval.insert(pair<string, string>("time", str_atime));
        entries.push_back(retval);

        if (details)
        {
#ifdef DEBUG
          estream.width(10);
          estream << stat.pcluster << '.';
          estream.width(3);
          estream << left << stat.pindex << right << ' ';
          estream.width(10);
          estream << stat.cluster << ' ';

          //Access
          char access[4];
          access[0] = (stat.type == FS_TYPE_DIR) ? 'd' : '-';
          access[1] = 'r';
          access[2] = (stat.access & 02) ? 'w' : '-';
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
          estream << fname;

          cout << estream.str() << endl;
        }
        else
        {
          cout.width(20);
          cout << left << fname << right;
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
    fsCloseDir(dir);
  }
  else
  {
    cout << "ls: " << loc << ": No such directory" << endl;
  }
  return entries;
}

//------------------------------------------------------------------------------
#ifdef FAT_WRITE
enum cResult util_mkdir(struct FsHandle *handler, const vector<string> &args,
    string &loc)
{
  //Process only first argument
  if (args.size() < 2)
    return C_SYNTAX;
  string newloc = parsePath(loc, args[1]);
  enum result fsres = fsMakeDir(handler, newloc.c_str());
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
enum cResult util_rm(struct FsHandle *handler, const vector<string> &args,
    string &loc)
{
  enum cResult res = C_OK;
  enum result fsres;

  for (unsigned int i = 1; i < args.size(); i++)
  {
    string newloc = parsePath(loc, args[i]);
    fsres = fsRemove(handler, newloc.c_str());
    if (fsres != E_OK)
    {
      cout << "rm: " << newloc << ": No such file" << endl;
      res = C_ERROR;
    }
  }
  return res;
}
#endif
//---------------------------------------------------------------------------
#ifdef FAT_WRITE
enum cResult util_mv(struct FsHandle *handler, const vector<string> &args,
    string &loc)
{
  //Process only first and second arguments
  if (args.size() < 3)
    return C_SYNTAX;

  string src = parsePath(loc, args[1]);
  string dst = parsePath(loc, args[2]);

  if (fsMove(handler, src.c_str(), dst.c_str()) == E_OK)
  {
    return C_OK;
  }
  else
  {
    struct FsFile *file;
    if (!(file = fsOpen(handler, src.c_str(), FS_READ)))
      cout << "mv: " << src << ": No such file" << endl;
    else
    {
      fsClose(file);
      if ((file = fsOpen(handler, dst.c_str(), FS_READ)))
      {
        cout << "mv: " << dst << ": File exists" << endl;
        fsClose(file);
      }
      else
        cout << "mv: Error moving file" << endl;
    }
    return C_ERROR;
  }
}
#endif
//---------------------------------------------------------------------------
//Write file from host filesystem to opened volume
#ifdef FAT_WRITE
enum cResult util_put(struct FsHandle *handler, const vector<string> &args,
    string &loc)
{
  //Process only first and second arguments
  if (args.size() < 3)
    return C_SYNTAX;

  string host = args[1];
  string target = parsePath(loc, args[2]);

  int bufSize = 512;

  struct FsFile *file;
  ifstream datafile;
  datafile.open(host.c_str());
  if (!datafile)
  {
    cout << "put: " << host << ": No such file" << endl;
    return C_ERROR;
  }
  if ((file = fsOpen(handler, target.c_str(), FS_WRITE)))
  {
    result ecode;
    uint32_t cnt;
    uint64_t total = 0;

    while (!datafile.eof())
    {
      char *ibuf = new char[bufSize];
      datafile.read(ibuf, bufSize); //FIXME rewrite
      ecode = fsWrite(file, (uint8_t *)ibuf, datafile.gcount(), &cnt);
      total += cnt;
      delete ibuf;
      if ((ecode != E_OK) || (cnt != datafile.gcount()))
      {
        cout << "put: " << target << ": Write error on " << total << endl;
        break;
      }
      if (!datafile.gcount())
        break;
    }
    fsClose(file);
  }
  else
  {
    cout << "put: " << target << ": Error creating file" << endl;
    datafile.close();
    return C_ERROR;
  }
  datafile.close();
  return C_OK;
}
#endif
//------------------------------------------------------------------------------
#ifdef FAT_WRITE
enum cResult util_cp(struct FsHandle *handler, const vector<string> &args,
    const string &loc)
{
  //TODO add argument parsing
  string src = parsePath(loc, args[1]);
  string dst = parsePath(loc, args[2]);

  struct FsFile *srcFile, *dstFile;
  if (!(srcFile = fsOpen(handler, src.c_str(), FS_READ)))
  {
    return C_ERROR;
  }
  if (!(dstFile = fsOpen(handler, dst.c_str(), FS_WRITE)))
  {
    fsClose(srcFile);
    return C_ERROR;
  }

  const int bufSize = 5000;
  char buf[bufSize];
  enum result fsres;
  while (!fsEof(srcFile))
  {
    uint32_t cnt, wcnt;

    fsres = fsRead(srcFile, (uint8_t *)buf, bufSize, &cnt);
    if (fsres != E_OK)
    {
      cout << "cp: Error" << endl;
      return C_ERROR;
    }
    wcnt = cnt;
    fsres = fsWrite(dstFile, (uint8_t *)buf, wcnt, &cnt);
    if (fsres != E_OK)
    {
      cout << "cp: Error" << endl;
      return C_ERROR;
    }
  }

  fsClose(dstFile);
  fsClose(srcFile);
  return C_OK;
}
#endif
//------------------------------------------------------------------------------
vector< map<string, string> > util_md5sum(struct FsHandle *handler,
    const vector<string> &args, const string &loc)
{
  const int bufSize = 64;
  vector< map<string, string> > entries;
  struct FsFile *file;
  enum result fsres;

  for (unsigned int i = 1; i < args.size(); i++)
  {
    map<string, string> retval;
    string newloc = parsePath(loc, args[i]);
    file = fsOpen(handler, newloc.c_str(), FS_READ);
    if (file)
    {
      uint32_t cnt;
      char buf[bufSize];
      MD5_CTX md5result;
      unsigned char md5str[16];

      MD5_Init(&md5result);
      while ((fsres = fsRead(file, (uint8_t *)buf, bufSize, &cnt)) == E_OK)
      {
        if (cnt)
          MD5_Update(&md5result, (const void *)buf, cnt);
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
#ifdef FAT_WRITE
enum cResult util_dd(struct FsHandle *handler, const vector<string> &args,
    const string &loc)
{
  //Test for fsSeek and fsTell
  const char *originValues[] = {"FS_SEEK_SET", "FS_SEEK_CUR", "FS_SEEK_END"};
  string src = "", dst = "";
  uint32_t blockSize = 512;
  asize_t offset = 0, length = 0, position;
  enum fsSeekOrigin origin = FS_SEEK_SET;
  struct FsStat stat;

  for (unsigned int i = 1; i < args.size(); i++)
  {
    //Input file
    if (args[i] == "-i" && i + 1 < args.size())
    {
      src = parsePath(loc, args[++i]);
      continue;
    }
    //Output file
    if (args[i] == "-o" && i + 1 < args.size())
    {
      dst = parsePath(loc, args[++i]);
      continue;
    }
    //Block size
    if (args[i] == "--block" && i + 1 < args.size())
    {
      try
      {
        blockSize = lexical_cast<uint32_t>(args[++i]);
      }
      catch (bad_lexical_cast &)
      {
        cout << "dd: Error: unsupported block size" << endl;
        return C_ERROR;
      }
      continue;
    }
    //Chunk length
    if (args[i] == "--length" && i + 1 < args.size())
    {
      try
      {
        length = lexical_cast<asize_t>(args[++i]);
      }
      catch (bad_lexical_cast &)
      {
        cout << "dd: Error: unsupported length" << endl;
        return C_ERROR;
      }
      continue;
    }
    //Origin
    if (args[i] == "--origin" && i + 1 < args.size())
    {
      i++;
      if (args[i] == "FS_SEEK_SET")
        origin = FS_SEEK_SET;
      else if (args[i] == "FS_SEEK_END")
        origin = FS_SEEK_END;
      else
      {
        cout << "dd: Error: unsupported origin" << endl;
        return C_ERROR;
      }
      continue;
    }
    //Offset from origin
    if (args[i] == "--offset" && i + 1 < args.size())
    {
      try
      {
        offset = lexical_cast<asize_t>(args[++i]);
      }
      catch (bad_lexical_cast &)
      {
        cout << "dd: Error: unsupported block count" << endl;
        return C_ERROR;
      }
      continue;
    }
  }

  if (fsStat(handler, src.c_str(), &stat) != E_OK)
  {
    cout << "dd: Error: stat failed" << endl;
    return C_ERROR;
  }
  if (length < 0)
  {
    cout << "dd: Error: unsupported length" << endl;
    return C_ERROR;
  }
  if (!length)
    length = stat.size;
  switch (origin)
  {
    case FS_SEEK_SET:
      position = offset;
      break;
    case FS_SEEK_END:
      position = stat.size + offset;
      break;
    default:
      return C_ERROR; //Never reached there
  }
#ifdef DEBUG
  cout << "dd: input: " << src << ", output: " << dst << endl;
  cout << "dd: block size: " << blockSize << ", length: " << length << endl;
  cout << "dd: origin: " << originValues[origin] <<
      ", offset: " << offset << ", position: " << position << endl;
#endif

  struct FsFile *srcFile, *dstFile;
  if (!(srcFile = fsOpen(handler, src.c_str(), FS_READ)))
  {
    return C_ERROR;
  }
  if (!(dstFile = fsOpen(handler, dst.c_str(), FS_WRITE)))
  {
    fsClose(srcFile);
    return C_ERROR;
  }

  char *buf = new char[blockSize];
  cResult res = C_OK;
  enum result fsres;

  try
  {
    asize_t check;

    fsres = fsSeek(srcFile, offset, origin);
    if (fsres != E_OK)
      throw "seek failed";

    check = fsTell(srcFile);
    if (check != position)
      throw "tell failed";

    uint32_t cnt, wcnt, rcnt, rdone = 0;
    while (!fsEof(srcFile) && rdone < length)
    {
      rcnt = length - rdone;
      rcnt = rcnt > blockSize ? blockSize : rcnt;
      fsres = fsRead(srcFile, (uint8_t *)buf, rcnt, &cnt);
      if (fsres != E_OK)
        throw "source read failed";
      rdone += cnt;
      wcnt = cnt;
      fsres = fsWrite(dstFile, (uint8_t *)buf, wcnt, &cnt);
      if (cnt != wcnt)
        throw "block length and written length differ";
      if (fsres != E_OK)
        throw "destination write failed";
    }

    fsres = fsSeek(srcFile, -length, FS_SEEK_CUR);
    if (fsres != E_OK)
      throw "validation seek failed";
    check = fsTell(srcFile);
    if (check != position)
      throw "validation tell failed";
  }
  catch (const char *err)
  {
    cout << "dd: Error: " << err << endl;
    res = C_ERROR;
  }

  delete[] buf;
  fsClose(dstFile);
  fsClose(srcFile);
  return res;
}
#endif
//------------------------------------------------------------------------------
int util_io(struct FsHandle *handler)
{
  cout << "Sectors read:    " << readCount << endl;
  cout << "Sectors written: " << writeCount << endl;
  return E_OK;
}
//------------------------------------------------------------------------------
int util_info(struct FsHandle *handler)
{
#if defined (FAT_WRITE) && defined (DEBUG)
  uint32_t sz;
  sz = countFree(handler);
#endif
//   cout << "Sectors in cluster: " << (int)(1 << handler->clusterSize) << endl;
//   cout << "FAT sector:         " << handler->tableSector << endl;
//   cout << "Data sector:        " << handler->dataSector << endl;
//   cout << "Root cluster:       " << handler->rootCluster << endl;
// #ifdef FAT_WRITE
//   cout << "FAT records count:  " << (int)handler->tableCount << endl;
//   cout << "Sectors in FAT:     " << handler->tableSize << endl;
//   cout << "Info sector:        " << handler->infoSector << endl;
//   cout << "Data clusters:      " << handler->clusterCount << endl;
//   cout << "Last allocated:     " << handler->lastAllocated << endl;
#if defined (FAT_WRITE) && defined (DEBUG)
  cout << "Free clusters:       " << sz << endl;
#endif
// #endif
  cout << "Size of FsHandle:    " << sizeof(struct FsHandle) << endl;
  cout << "Size of FsFile:      " << sizeof(struct FsFile) << endl;
  cout << "Size of FsDir:       " << sizeof(struct FsDir) << endl;
  return E_OK;
}
//------------------------------------------------------------------------------
void util_autotest(FsHandle *handler, const vector<string> &args)
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

    string comStr = "";
    string argStr = "";

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
      if (commandParser(handler, loc, comStr, &argStr) == C_OK)
        passed++;
      else
        failed++;
    }
  }
  testbench.close();
  cout << "/*--------------------------------------";
  cout << "--------------------------------------*/" << endl;
  cout << "Test result: total " << total << ", passed " << passed <<
      ", failed " << failed << endl;
}
//------------------------------------------------------------------------------
enum cResult commandParser(FsHandle *handler, string &loc, const string &str,
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
    util_io(handler);
  }
  if (args[0] == "info")
  {
    util_info(handler);
  }
  if (args[0] == "cd")
  {
    enum cResult retval;
    retval = util_cd(handler, args, loc);
    if (retval != C_OK)
    {
      cout << "Error" << endl;
      return C_ERROR;
    }
  }
  if (args[0] == "ls")
  {
    vector< map<string, string> > result;
    result = util_ls(handler, args, loc);

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
          cout << "ls: " << retvals[pos] << ": No such file or directory" << endl;
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
#ifdef FAT_WRITE
  if (args[0] == "mkdir")
  {
    enum cResult retval;
    retval = util_mkdir(handler, args, loc);
    if (retval != C_OK)
    {
      cout << "Error" << endl;
      return C_ERROR;
    }
  }
  if (args[0] == "rm")
  {
    enum cResult retval;
    retval = util_rm(handler, args, loc);
    if (retval != C_OK)
    {
      cout << "Error" << endl;
      return C_ERROR;
    }
  }
  if (args[0] == "mv")
  {
    enum cResult retval;
    retval = util_mv(handler, args, loc);
    if (retval != C_OK)
    {
      cout << "Error" << endl;
      return C_ERROR;
    }
  }
  if (args[0] == "put")
  {
    enum cResult retval;
    retval = util_put(handler, args, loc);
    if (retval != C_OK)
    {
      cout << "Error" << endl;
      return C_ERROR;
    }
  }
  if (args[0] == "cp")
  {
    enum cResult retval;
    retval = util_cp(handler, args, loc);
    if (retval != C_OK)
    {
      cout << "Error" << endl;
      return C_ERROR;
    }
  }
  if (args[0] == "dd")
  {
    enum cResult retval;
    retval = util_dd(handler, args, loc);
    if (retval != C_OK)
    {
      cout << "Error" << endl;
      return C_ERROR;
    }
  }
#endif
  if (args[0] == "md5sum")
  {
    vector< map<string, string> > result;
    result = util_md5sum(handler, args, loc);

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
          cout << "md5sum: " << (*i)["name"] <<
              ": Checksum difference, expected " << retvals[pos] <<
              ", got " << (*i)["checksum"] << endl;
          break;
        }
      }

      if (found != retvals.size())
      {
        cout << "Error" << endl;
        return C_ERROR;
      }
      else
        cout << "md5sum: completed successfully" << endl;
    }
//     vector<string> required;
//     required.push_back(args[i]);
  }
  if (args[0] == "autotest")
  {
    util_autotest(handler, args);
  }
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
  struct FsHandle *handler;

  mmaped = (struct Interface *)init(Mmi, argv[1]);
  if (!mmaped)
  {
    printf("Error opening file\n");
    return 0;
  }

//   if (mmdReadTable(dev, 0, 0) == E_OK)
//   {
//     /*
//      * 0x0B: 32-bit FAT
//      * 0x0C: 32-bit FAT, using INT 13 Extensions.
//      * 0x1B: Hidden 32-bit FAT
//      * 0x1C: Hidden 32-bit FAT, using INT 13 Extensions
//      */
//     if (mmdGetType(dev) != 0x0B)
//     {
//       printf("Wrong partition type, expected: 0x0B, got: 0x%02X\n",
//           mmdGetType(dev));
//     }
//   }
//   else
//     printf("No partitions found, selected raw partition at 0\n");

  struct Fat32Config fsConf = {
    .interface = mmaped
  };
  handler = (struct FsHandle *)init(FatHandle, &fsConf);
  if (!handler)
  {
    printf("Error creating FAT32 handler\n");
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
    res = commandParser(handler, location, command);
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
  deinit(handler);
  return 0;
}
