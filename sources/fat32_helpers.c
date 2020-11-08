/*
 * fat32_helpers.c
 * Copyright (C) 2020 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <yaf/fat32_helpers.h>
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static char convertNameCharacter(char);
#endif /* CONFIG_WRITE */
/*----------------------------------------------------------------------------*/
size_t computeShortNameLength(const struct DirEntryImage *entry)
{
  size_t nameLength = 0;
  const char *source;

  source = entry->name;
  for (size_t i = 0; i < BASENAME_LENGTH; ++i)
  {
    if (*source++ != ' ')
      ++nameLength;
    else
      break;
  }

  if (!(entry->flags & FLAG_DIR) && entry->extension[0] != ' ')
  {
    ++nameLength;

    source = entry->extension;
    for (size_t i = 0; i < EXTENSION_LENGTH; ++i)
    {
      if (*source++ != ' ')
        ++nameLength;
      else
        break;
    }
  }

  return nameLength;
}
/*----------------------------------------------------------------------------*/
void extractShortName(char *name, const struct DirEntryImage *entry)
{
  char *destination = name;
  const char *source;

  /* Copy entry name */
  source = entry->name;
  for (size_t i = 0; i < BASENAME_LENGTH; ++i)
  {
    if (*source != ' ')
      *destination++ = *source++;
    else
      break;
  }

  /* Add dot when entry is not directory or extension exists */
  if (!(entry->flags & FLAG_DIR) && entry->extension[0] != ' ')
  {
    *destination++ = '.';

    /* Copy entry extension */
    source = entry->extension;
    for (size_t i = 0; i < EXTENSION_LENGTH; ++i)
    {
      if (*source != ' ')
        *destination++ = *source++;
      else
        break;
    }
  }
  *destination = '\0';
}
/*----------------------------------------------------------------------------*/
bool rawDateTimeToTimestamp(time64_t *timestamp, uint16_t date, uint16_t time)
{
  const struct RtDateTime dateTime = {
      .second = time & 0x1F,
      .minute = (time >> 5) & 0x3F,
      .hour = (time >> 11) & 0x1F,
      .day = date & 0x1F,
      .month = (date >> 5) & 0x0F,
      .year = ((date >> 9) & 0x7F) + 1980
  };

  time64_t unixTime;
  const enum Result res = rtMakeEpochTime(&unixTime, &dateTime);

  if (res == E_OK)
  {
    *timestamp = unixTime * 1000000;
    return true;
  }
  else
    return false;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_UNICODE
/* Calculate entry name checksum for long file name entries support */
uint8_t calcLongNameChecksum(const char *name, size_t length)
{
  uint8_t sum = 0;

  for (size_t i = 0; i < length; ++i)
    sum = ((sum >> 1) | (sum << 7)) + *name++;

  return sum;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_UNICODE
/* Extract 13 Unicode characters from long file name entry */
void extractLongName(char16_t *name, const struct DirEntryImage *entry)
{
  memcpy(name, entry->longName0, sizeof(entry->longName0));
  name += sizeof(entry->longName0) / sizeof(char16_t);
  memcpy(name, entry->longName1, sizeof(entry->longName1));
  name += sizeof(entry->longName1) / sizeof(char16_t);
  memcpy(name, entry->longName2, sizeof(entry->longName2));
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
void extractShortBasename(char *baseName, const char *shortName)
{
  size_t i = 0;

  for (; i < BASENAME_LENGTH; ++i)
  {
    if (!shortName[i] || shortName[i] == ' ')
      break;
    baseName[i] = shortName[i];
  }
  baseName[i] = '\0';
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
void fillDirEntry(struct DirEntryImage *entry, bool directory, FsAccess access,
    uint32_t payloadCluster, time64_t timestamp)
{
  /* Clear unused fields */
  entry->unused0 = 0;
  entry->unused1 = 0;
  memset(entry->unused2, 0, sizeof(entry->unused2));

  entry->flags = 0;
  if (directory)
    entry->flags |= FLAG_DIR;
  if (!(access & FS_ACCESS_WRITE))
    entry->flags |= FLAG_RO;

  entry->clusterHigh = toLittleEndian16((uint16_t)(payloadCluster >> 16));
  entry->clusterLow = toLittleEndian16((uint16_t)payloadCluster);
  entry->size = 0;

  struct RtDateTime dateTime;

  timestamp /= 1000000;
  rtMakeTime(&dateTime, timestamp);
  entry->date = toLittleEndian16(timeToRawDate(&dateTime));
  entry->time = toLittleEndian16(timeToRawTime(&dateTime));
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
/* Returns success when node has a valid short name */
bool fillShortName(char *shortName, const char *name, bool extension)
{
  const size_t length = strlen(name);
  const char *dot = name + length - 1;
  bool clean = true;

  /* Find start position of the extension */
  if (extension)
  {
    for (; dot >= name && *dot != '.'; --dot);
  }

  if (!extension || dot < name)
  {
    /* Dot not found */
    if (length > BASENAME_LENGTH)
      clean = false;
    dot = 0;
  }
  else
  {
    const size_t nameLength = dot - name;
    const size_t extensionLength = length - nameLength;

    /* Check whether file name and extension have adequate length */
    if (extensionLength > EXTENSION_LENGTH + 1 || nameLength > BASENAME_LENGTH)
      clean = false;
  }

  size_t position = 0;

  memset(shortName, ' ', NAME_LENGTH);
  for (char c = *name; c != '\0'; c = *name)
  {
    if (dot && name == dot)
    {
      position = BASENAME_LENGTH;
      ++name;
      continue;
    }
    ++name;

    const char converted = convertNameCharacter(c);

    if (converted != c)
      clean = false;
    if (!converted)
      continue;
    shortName[position++] = converted;

    if (position == BASENAME_LENGTH)
    {
      if (dot) /* Check whether extension exists */
      {
        name = dot + 1;
        continue;
      }
      else
        break;
    }
    if (position == NAME_LENGTH)
      break;
  }

  return clean;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
uint16_t timeToRawDate(const struct RtDateTime *value)
{
  return value->day | (value->month << 5) | ((value->year - 1980) << 9);
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
uint16_t timeToRawTime(const struct RtDateTime *value)
{
  return (value->second >> 1) | (value->minute << 5) | (value->hour << 11);
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_UNICODE) && defined(CONFIG_WRITE)
/* Save Unicode characters to long file name entry */
void fillLongName(struct DirEntryImage *entry, const char16_t *name,
    size_t count)
{
  char16_t buffer[LFN_ENTRY_LENGTH];
  const char16_t *position = buffer;

  memcpy(buffer, name, count * sizeof(char16_t));
  if (count < LFN_ENTRY_LENGTH)
  {
    buffer[count] = '\0';

    if (count + 1 < LFN_ENTRY_LENGTH)
    {
      const size_t padding = LFN_ENTRY_LENGTH - 1 - count;
      memset(&buffer[count + 1], 0xFF, padding * sizeof(char16_t));
    }
  }

  memcpy(entry->longName0, position, sizeof(entry->longName0));
  position += sizeof(entry->longName0) / sizeof(char16_t);
  memcpy(entry->longName1, position, sizeof(entry->longName1));
  position += sizeof(entry->longName1) / sizeof(char16_t);
  memcpy(entry->longName2, position, sizeof(entry->longName2));
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_UNICODE) && defined(CONFIG_WRITE)
void fillLongNameEntry(struct DirEntryImage *entry, uint8_t current,
    uint8_t total, uint8_t checksum)
{
  /* Clear reserved fields */
  entry->unused0 = 0;
  entry->unused3 = 0;

  entry->flags = MASK_LFN;
  entry->checksum = checksum;
  entry->ordinal = current;
  if (current == total)
    entry->ordinal |= LFN_LAST;
}
#endif
/*----------------------------------------------------------------------------*/
#if defined(CONFIG_UNICODE) && defined(CONFIG_WRITE)
size_t uniqueNameConvert(char *shortName)
{
  size_t position = strlen(shortName);

  if (!position--)
    return 0;

  size_t begin = 0;
  size_t end = 0;
  size_t nameIndex = 0;

  for (; position > 0; --position)
  {
    const char value = shortName[position];

    if (value == '~')
    {
      begin = position++;

      while (position <= end)
      {
        const size_t currentNumber = shortName[position] - '0';

        nameIndex = (nameIndex * 10) + currentNumber;
        ++position;
      }
      break;
    }
    else if (value >= '0' && value <= '9')
    {
      if (!end)
        end = position;
    }
    else
      break;
  }

  if (nameIndex)
    shortName[begin] = '\0';

  return nameIndex;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_WRITE
static char convertNameCharacter(char value)
{
  static const char forbidden[] = {
      0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x5B, 0x5C, 0x5D, 0x7C
  };

  /* Convert lower case characters to upper case */
  if (value >= 'a' && value <= 'z')
    return value - ('a' - 'A');

  /* Remove spaces */
  if (value == ' ')
    return 0;

  if (value > 0x20 && value < 0x7F && !(value >= 0x3A && value <= 0x3F))
  {
    bool found = false;

    for (size_t i = 0; i < ARRAY_SIZE(forbidden); ++i)
    {
      if (value == forbidden[i])
      {
        found = true;
        break;
      }
    }
    if (!found)
      return value;
  }

  /* Replace all other characters with underscore */
  return '_';
}
#endif
