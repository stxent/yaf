#include <cstdio>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <cstring>
#include <map>
#include <vector>
//------------------------------------------------------------------------------
#include <openssl/md5.h>
//------------------------------------------------------------------------------
extern "C"
{
#include "fat.h"
#include "io.h"
#include "rtc.h"
}
//------------------------------------------------------------------------------
using namespace std;
//------------------------------------------------------------------------------
extern long readExcess, readCount, writeCount;
//------------------------------------------------------------------------------
enum cResult {
  C_OK = 0,
  C_SYNTAX,
  C_TERMINATE,
  C_ERROR
};
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
  if (path[0] != '/')
  {
    res = folder;
    if (folder.size() > 1)
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
  return (value < 10) ? ('0' + value) : ('a' + value - 10);
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
  struct FsDir dir;
  enum fsResult fsres;
  string newloc;

  if (args.size() < 2)
    return C_SYNTAX;
  newloc = parsePath(loc, args[1]);

  fsres = fsOpenDir(handler, &dir, newloc.c_str());
  if (fsres == FS_OK)
  {
    fsCloseDir(&dir);
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
  struct FsDir dir;
  struct FsStat stat;
  bool details = false;
  enum fsResult fsres;
  string path;

  for (unsigned int i = 1; i < args.size(); i++)
  {
    if (args[i] == "-l")
    {
      details = true;
      continue;
    }
  }

  fsres = fsOpenDir(handler, &dir, loc.c_str());
  if (fsres == FS_OK)
  {
    char fname[13];
    int pos;
    for (pos = 1; (fsres = fsReadDir(&dir, fname)) == FS_OK; pos++)
    {
      map<string, string> retval;
      stringstream estream;
      path = parsePath(loc, (string)fname);
      if (fsStat(handler, path.c_str(), &stat) == FS_OK)
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
          estream << str_size << ' ';
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
//       else
//       {
//       }
    }
    if (!details && ((pos - 1) % 4))
      cout << endl;
    fsCloseDir(&dir);
  }
  else
  {
    cout << "ls: " << loc << ": No such directory" << endl;
  }
  return entries;
}

//------------------------------------------------------------------------------
#ifdef FS_WRITE_ENABLED
enum cResult util_mkdir(struct FsHandle *handler, const vector<string> &args,
    string &loc)
{
  //Process only first argument
  if (args.size() < 2)
    return C_SYNTAX;
  string newloc = parsePath(loc, args[1]);
  enum fsResult fsres = fsMakeDir(handler, newloc.c_str());
  if (fsres != FS_OK)
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
#ifdef FS_WRITE_ENABLED
enum cResult util_rm(struct FsHandle *handler, const vector<string> &args,
    string &loc)
{
  enum cResult res = C_OK;
  enum fsResult fsres;

  for (unsigned int i = 1; i < args.size(); i++)
  {
    string newloc = parsePath(loc, args[i]);
    fsres = fsRemove(handler, newloc.c_str());
    if (fsres != FS_OK)
    {
      cout << "rm: " << newloc << ": No such file" << endl;
      res = C_ERROR;
    }
  }
  return res;
}
#endif
//---------------------------------------------------------------------------
//Write file from host filesystem to opened volume
#ifdef FS_WRITE_ENABLED
enum cResult util_put(struct FsHandle *handler, const vector<string> &args,
    string &loc)
{
  //Process only first and second arguments
  if (args.size() < 3)
    return C_SYNTAX;

  string host = args[1];
  string target = parsePath(loc, args[2]);

  int bufSize = 512;

  struct FsFile file;
  ifstream datafile;
  datafile.open(host.c_str());
  if (!datafile)
  {
    cout << "put: " << host << ": File does not exist" << endl;
    return C_ERROR;
  }
  if (fsOpen(handler, &file, target.c_str(), FS_WRITE) == FS_OK)
  {
    fsResult ecode;
    uint16_t cnt;
    uint64_t total = 0;

    while (!datafile.eof())
    {
      char *ibuf = new char[bufSize];
      datafile.read(ibuf, bufSize);
      ecode = fsWrite(&file, (uint8_t *)ibuf, datafile.gcount(), &cnt);
      total += cnt;
      delete ibuf;
      if ((ecode != FS_OK) || (cnt != datafile.gcount()))
      {
        cout << "put: " << target << ": Write error on " << total << endl;
        break;
      }
      if (!datafile.gcount())
        break;
    }
    fsClose(&file);
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
vector< map<string, string> > util_md5sum(struct FsHandle *handler,
    const vector<string> &args, const string &loc)
{
  const int bufSize = 64;
  vector< map<string, string> > entries;
  struct FsFile file;
  enum fsResult fsres;

  for (unsigned int i = 1; i < args.size(); i++)
  {
    string newloc = parsePath(loc, args[i]);
    fsres = fsOpen(handler, &file, newloc.c_str(), FS_READ);
    if (fsres == FS_OK)
    {
      uint16_t cnt;
      char buf[bufSize];
      MD5_CTX md5result;
      unsigned char md5str[16];
      map<string, string> retval;

      MD5_Init(&md5result);
      while ((fsres = fsRead(&file, (uint8_t *)buf, bufSize, &cnt)) == FS_OK)
      {
        if (cnt)
          MD5_Update(&md5result, (const void *)buf, cnt);
      }
      fsClose(&file);
      MD5_Final(md5str, &md5result);

      string digest = hexdigest(md5str);
      string extracted = extractName(newloc);
      cout << digest << '\t' << extracted << endl;
      retval.insert(pair<string, string>("name", extracted));
      retval.insert(pair<string, string>("checksum", digest));
      entries.push_back(retval);
    }
    else
    {
      cout << "md5sum: " << newloc << ": No such file" << endl;
    }
  }
  return entries;
}
//------------------------------------------------------------------------------
int util_io(struct FsHandle *handler)
{
#ifdef DEBUG
  cout << "Read excess:     " << readExcess << endl;
#endif
  cout << "Sectors read:    " << readCount << endl;
  cout << "Sectors written: " << writeCount << endl;
  return FS_OK;
}
//------------------------------------------------------------------------------
int util_info(struct FsHandle *handler)
{
#if defined (FS_WRITE_ENABLED) && defined (DEBUG)
  uint32_t sz;
  sz = countFree(handler);
#endif
  cout << "Sectors in cluster: " << (int)(1 << handler->clusterSize) << endl;
  cout << "FAT sector:         " << handler->tableSector << endl;
  cout << "Data sector:        " << handler->dataSector << endl;
  cout << "Root cluster:       " << handler->rootCluster << endl;
#ifdef FS_WRITE_ENABLED
  cout << "FAT records count:  " << (int)handler->tableCount << endl;
  cout << "Sectors in FAT:     " << handler->tableSize << endl;
  cout << "Info sector:        " << handler->infoSector << endl;
  cout << "Data clusters:      " << handler->clusterCount << endl;
  cout << "Last allocated:     " << handler->lastAllocated << endl;
#ifdef DEBUG
  cout << "Free clusters:      " << sz << endl;
#endif
#endif
  cout << "Size of FsDevice:   " << sizeof(FsDevice) << endl;
  cout << "Size of FsHandle:   " << sizeof(FsHandle) << endl;
  cout << "Size of FsFile:     " << sizeof(FsFile) << endl;
  cout << "Size of FsDir:      " << sizeof(FsDir) << endl;
  return FS_OK;
}
//------------------------------------------------------------------------------
enum cResult commandParser(FsHandle *handler, string &loc, const char *str)
{
  vector<string> args;
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
  if (args[0] == "io")
  {
    util_io(handler);
  }
//     case "stat":
//       util_io(handler);
//       break;
  if (args[0] == "info")
  {
    util_info(handler);
  }
  if (args[0] == "cd")
  {
    enum cResult retval;
    retval = util_cd(handler, args, loc);
    if (retval != C_OK)
      cout << "Error" << endl;
  }
  if (args[0] == "ls")
  {
    vector< map<string, string> > retval;
    retval = util_ls(handler, args, loc);
//     vector<string> required;
//     required.push_back(args[i]);
  }
  if (args[0] == "mkdir")
  {
    enum cResult retval;
    retval = util_mkdir(handler, args, loc);
    if (retval != C_OK)
      cout << "Error" << endl;
  }
  if (args[0] == "rm")
  {
    enum cResult retval;
    retval = util_rm(handler, args, loc);
    if (retval != C_OK)
      cout << "Error" << endl;
  }
  if (args[0] == "put")
  {
    enum cResult retval;
    retval = util_put(handler, args, loc);
    if (retval != C_OK)
      cout << "Error" << endl;
  }
  if (args[0] == "md5sum")
  {
    vector< map<string, string> > retval;
    retval = util_md5sum(handler, args, loc);
//     vector<string> required;
//     required.push_back(args[i]);
  }
  if (args[0] == "exit")
    return C_TERMINATE;
  return C_OK;
}
//------------------------------------------------------------------------------
volatile char internalBuf[FS_BUFFER];
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
  if (argc < 2)
    return 0;

  FsDevice dev;
  FsHandle handler;

  fsSetIO(&handler, sRead, sWrite);
  if (sOpen(&dev, (uint8_t *)internalBuf, argv[1]) != FS_OK)
  {
    printf("Error opening file\n");
    return 0;
  }
  if (sReadTable(&dev, 0, 0) == FS_OK)
  {
    /*
     * 0x0B: 32-bit FAT
     * 0x0C: 32-bit FAT, using INT 13 Extensions.
     * 0x1B: Hidden 32-bit FAT
     * 0x1C: Hidden 32-bit FAT, using INT 13 Extensions
     */
    if (dev.type != 0x0B)
    {
      printf("Wrong partition descriptor, expected: 0x0B, got: 0x%02X\n",
          dev.type);
    }
    else
    {
      printf("Selected partition: offset = %d, size = %d, type = 0x%02X\n",
          dev.offset, dev.size, dev.type);
    }
  }
  else
    printf("No partitions found, selected raw partition at 0\n");

  if (fsLoad(&handler, &dev) != FS_OK)
  {
    printf("Error loading partition\n");
    return 0;
  }

  string location = "/";
  bool terminate = false;
  enum cResult res;

  while (!terminate)
  {
    char buf[512];
    string path;
    cout << location << "> ";
    cin.getline(buf, sizeof(buf));
    res = commandParser(&handler, location, buf);
    switch (res)
    {
      case C_TERMINATE:
        terminate = true;
        break;
      case C_SYNTAX:
        cout << "Syntax error" << endl;
        break;
      case C_ERROR:
        cout << "Error occured" << endl;
        break;
      case C_OK:
      default:
        break;
    }
  }

  printf("Unloading\n");
  fsUnload(&handler);
  return 0;
}
