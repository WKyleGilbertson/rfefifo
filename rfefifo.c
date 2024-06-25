/* Filename: rfefifo.c */
#pragma comment(lib, "lib/FTD2XX.lib")
#include "inc/ftd2xx.h"
#include "inc/version.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "inc/WinTypes.h"

//#define MLEN 65536
#define MLEN 131072
#define BYTESPERMS 8184
#define MSPERSEC 1000
// #define DEBUG

typedef struct signmag
{ // start from bit 0 --> go out to bit 8
  uint8_t loQ :2, loI : 2, hiQ : 2, hiI : 2;
} signmag;

int8_t convert(uint8_t value) {
  switch (value)
  {
    case 0:
     return 1;
     break;
    case 1:
     return 3;
     break;
    case 2:
     return -1;
     break;
    case 3:
     return -3;
     break;
    default:
     return 0;
     break;
  }
}

typedef struct
{
  uint8_t MSG[MLEN];
  uint32_t SZE;
  uint32_t CNT;
} PKT;

typedef struct
{
  uint32_t lComPortNumber;
  uint32_t ftDriverVer;
  FT_PROGRAM_DATA ftData;
  FT_HANDLE ftH;
} FT_CFG;

typedef struct
{
  FILE *ifp;
  //FILE *ofp;
  int  ofp;
  char baseFname[64];
  char sampFname[64];
  char outFname[64];
  bool convertFile;
  bool logfile;
  bool useTimeStamp;
  bool FNHN;
  uint64_t sampMS;
  FT_CFG ftC;
  SWV V;
} CONFIG;

void getISO8601(char datetime[17])
{
  struct timeval curTime;
  gettimeofday(&curTime, NULL);
  strftime(datetime, 17, "%Y%m%dT%H%M%SZ", gmtime(&curTime.tv_sec));
}

void printFTDIdevInfo(FT_CFG *ftC)
{
  fprintf(stderr, "ftDrvr:0x%x  ", ftC->ftDriverVer);
  fprintf(stderr, "FIFO:%s  ",
          (ftC->ftData.IFAIsFifo7 != 0) ? "Yes" : "No");
  if (ftC->lComPortNumber == -1)
  {
    fprintf(stderr, "No COM port assigned\n");
  }
  else
  {
    fprintf(stderr, "COM Port: %d ", ftC->lComPortNumber);
  }

  fprintf(stderr, "%s %s SN:%s \n", ftC->ftData.Description, ftC->ftData.Manufacturer,
          ftC->ftData.SerialNumber);
}

void initconfig(CONFIG *cfg)
{
  strcpy(cfg->baseFname, "/tmp/rfefifo");
  strcpy(cfg->sampFname, "l1ifdata.raw");
  strcpy(cfg->outFname, "/tmp/rfefifo");
  cfg->logfile = false;
  cfg->useTimeStamp = false;
  cfg->FNHN = true;
  cfg->sampMS = 1;
}

void processArgs(int argc, char *argv[], CONFIG *cfg)
{
  static int len, i, ch = ' ';
  static char *usage =
      "usage: rfefifo [options]\n"
      "       -f <filename> write to a different filename than the default\n"
      "       -l [filename] log raw data rather than binary interpretation\n"
      "       -r <filename> read raw log file and translate to binary\n"
      "       -t            use time tag for file name instead of default\n"
      "       -n            use FNLN instead of FNHN\n"
      "       -v            print version information\n"
      "       -?|h          show this usage infomation message\n"
      "  defaults: 1 ms of data logged in binary format as l1ifdata.bin\n";

  if (argc > 1)
  {
    //    printf("%d %c\n", argc, argv[1][1]);
    for (i = 1; i < argc; i++)
    {
      if (argv[i][0] == '-')
      {
        ch = argv[i][1];
        switch (ch)
        {
        case '?':
          printf("%s", usage);
          exit(0);
          break;
        case 'h':
          printf("%s", usage);
          exit(0);
          break;
        case 'n':
          cfg->FNHN = false;
          break;
        case 'f':
          if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
          {
            strcpy(cfg->outFname, argv[++i]);
          }
          else
          {
            printf("%s", usage);
            exit(1);
          }
          break;
        case 'l':
          if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
          {
            strcpy(cfg->outFname, argv[++i]);
          }
          else
          {
            strcpy(cfg->outFname, cfg->sampFname);
          }
          cfg->logfile = true;
          break;
        case 'r':
          if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
          {
            strcpy(cfg->sampFname, argv[++i]);
          }
          else
          {
            printf("%s", usage);
            exit(1);
          }
          cfg->convertFile = true;
          cfg->sampMS = 0;
          len = strlen(cfg->sampFname);
          strcpy(cfg->outFname, cfg->sampFname);
          cfg->outFname[len - 3] = '\0';
          strcat(cfg->outFname, "bin");
          break;
        case 't':
          cfg->useTimeStamp = true;

          getISO8601(cfg->baseFname); // getISO8601 for GCC platforms
#ifdef DEBUG
          printf("%s\n", cfg->baseFname);
#endif
          break;
        case 'v':
          fprintf(stdout, "%s: GitCI:%s %s v%.1d.%.1d.%.1d\n",
                  cfg->V.Name, cfg->V.GitCI, cfg->V.BuildDate,
                  cfg->V.Major, cfg->V.Minor, cfg->V.Patch);
          if (cfg->ftC.ftH != 0)
            printFTDIdevInfo(&cfg->ftC);
          exit(0);
          break;
        default:
          printf("%s", usage);
          exit(0);
          break;
        }
      }
      else
      {
        cfg->sampMS = atoi(argv[i]);
        //        printf("ms:%d\n", cfg->sampMS);
      }
    }
  }
  else // argc not greater than 1
  {
    //    printf("Do I need this? Default params\n");
    cfg->sampMS = 1; // Maybe this should be zero?
  }
  if (cfg->useTimeStamp == true)
  {
    strcpy(cfg->outFname, cfg->baseFname);
    if (cfg->logfile == true)
    {
      strcat(cfg->outFname, ".raw");
    }
    else
    {
      strcat(cfg->outFname, ".bin");
    }
  }
}

void readFTDIConfig(FT_CFG *cfg)
{
  FT_STATUS ftS;
  if (cfg->ftH != 0)
  {
    ftS = FT_GetDriverVersion(cfg->ftH, &cfg->ftDriverVer);
    if (ftS != FT_OK)
    {
      fprintf(stderr, "Couldn't read FTDI driver version.\n");
    }

    ftS = FT_SetTimeouts(cfg->ftH, 500, 500);
    if (ftS != FT_OK)
    {
      fprintf(stderr, "timeout A status not ok %d\n", ftS);
      // exit(1);
    }

    ftS = FT_EE_Read(cfg->ftH, &cfg->ftData);
    if (ftS != FT_OK)
    {
      fprintf(stderr, "FTDI EE Read did not succeed! %d\n", ftS);
    }

#if defined(_WIN32)
    ftS = FT_GetComPortNumber(cfg->ftH, &cfg->lComPortNumber);
    if (ftS != FT_OK)
    {
      fprintf(stderr, "FTDI Get Com Port Failed! %d\n", ftS);
    }
#endif
    ftS = FT_SetLatencyTimer(cfg->ftH, 2);
    //ftS = FT_SetUSBParameters(cfg->ftH, 0x10000, 0x10000);
    //ftS = FT_SetUSBParameters(cfg->ftH, 0x02000, 0x02000); // 8192
    ftS = FT_SetUSBParameters(cfg->ftH, 0x03000, 0x03000); // 16384
  }
}

void writeToBinFile(CONFIG *cfg, PKT *p)
{
  int32_t idx = 0;
  int8_t numbers[4];
  int8_t array[4*BYTESPERMS];
  signmag bData;

  for (idx = 0; idx < p->CNT; idx++)
  {
    memcpy(&bData, &p->MSG[idx], sizeof(uint8_t));
    numbers[0] = convert(bData.hiI);
    array[idx*4 + 0] = numbers[0];
    numbers[1] = convert(bData.hiQ);
    array[idx*4 + 1] = numbers[1];
    numbers[2] = convert(bData.loI);
    array[idx*4 + 2] = numbers[2];
    numbers[3] = convert(bData.loQ);
    array[idx*4 + 3] = numbers[3];
  }
  write(cfg->ofp, array, sizeof(int8_t)*p->CNT*4);
}

int32_t enQueue(PKT *src, PKT *dst, uint32_t cnt)
{
  uint32_t ext = cnt + dst->SZE;

  if (ext > MLEN)
  {
    fprintf(stderr, "OUT OF SPACE!\n");
    return -1;
  }
  memcpy(&dst->MSG[dst->SZE], src->MSG, cnt);
  dst->SZE += cnt;
  return dst->SZE;
}

int32_t deQueue(PKT *src, PKT *dst, uint32_t cnt)
{
  uint32_t ext = src->SZE - cnt;

  if (ext < 0)
  {
    fprintf(stderr, "REQUEST TOO LARGE!\n");
    return -1;
  }
  memcpy(dst->MSG, src->MSG, cnt);
  dst->SZE = dst->CNT = cnt;
  memmove(src->MSG, &src->MSG[cnt], ext);
  src->SZE = ext;
  return src->SZE;
}

int main(int argc, char *argv[])
{
  CONFIG cnfg;
  char ManufacturerBuf[32];
  char ManufacturerIdBuf[16];
  char DescriptionBuf[64];
  char SerialNumberBuf[16];

  cnfg.ftC.ftData.Signature1 = 0x00000000;
  cnfg.ftC.ftData.Signature2 = 0xffffffff;
  cnfg.ftC.ftData.Version = 0x00000003; // 2232H extensions
  cnfg.ftC.ftData.Manufacturer = ManufacturerBuf;
  cnfg.ftC.ftData.ManufacturerId = ManufacturerIdBuf;
  cnfg.ftC.ftData.Description = DescriptionBuf;
  cnfg.ftC.ftData.SerialNumber = SerialNumberBuf;

  PKT rx, bfr, ms;
  FT_STATUS ftS;

  float sampleTime = 0.0;
  unsigned long i = 0, totalBytes = 0, targetBytes = 0;
  unsigned char sampleValue;
  char valueToWrite;

  uint32_t idx = 0, mscount = 0;
  int32_t len = 0;
  uint8_t blankLine[120];
  uint8_t ch;

  initconfig(&cnfg);
  processArgs(argc, argv, &cnfg);

  if (cnfg.sampMS == 0) {
    cnfg.sampMS = ULONG_MAX;
  }

  cnfg.V.Major = MAJOR_VERSION;
  cnfg.V.Minor = MINOR_VERSION;
  cnfg.V.Patch = PATCH_VERSION;
#ifdef CURRENT_HASH
  strncpy(cnfg.V.GitCI, CURRENT_HASH, 40);
  cnfg.V.GitCI[40] = '\0';
#endif
#ifdef CURRENT_DATE
  strncpy(cnfg.V.BuildDate, CURRENT_DATE, 16);
  cnfg.V.BuildDate[16] = '\0';
#endif
#ifdef CURRENT_NAME
  strncpy(cnfg.V.Name, CURRENT_NAME, 10);
  cnfg.V.Name[10] = '\0';
#endif

  memset(blankLine, '\b', 119);
  blankLine[119] = '\0';

  memset(rx.MSG, 0, 65536);
  memset(bfr.MSG, 0, 65536);
  memset(ms.MSG, 0, 65536);
  bfr.CNT = bfr.SZE = 0;
  ms.CNT = ms.SZE = 0;

  // ftS = FT_Open(0, &ftH);
  ftS = FT_OpenEx("USB<->GPS A", FT_OPEN_BY_DESCRIPTION, &cnfg.ftC.ftH);
  if (ftS != FT_OK)
  {
    fprintf(stderr, "Device not present. Perhaps just not plugged in?\n");
    // fprintf(stderr, "open device status not ok %d\n", ftS);
  }
  readFTDIConfig(&cnfg.ftC);
  if (cnfg.ftC.ftH == 0)
    exit(0);

mkfifo("/tmp/rfefifo", 0666);
cnfg.ofp = open("/tmp/rfefifo", O_WRONLY); // Open FIFO

/* After Arguments Parsed, Open [Optional] Files */
#ifdef DEBUG
  fprintf(stdout, "base:%s out:%s samp:%s ",
          cnfg.baseFname, cnfg.outFname, cnfg.sampFname);
  fprintf(stdout, "Raw? %s, TS:%s CF: %s FNHN? %s ",
          cnfg.logfile == true ? "yes" : "no",
          cnfg.useTimeStamp == true ? "yes" : "no",
          cnfg.convertFile == true ? "yes" : "no",
          cnfg.FNHN == true ? "yes" : "no");
  fprintf(stdout, "ms: %d\n", cnfg.sampMS);
#endif

  targetBytes = BYTESPERMS * cnfg.sampMS;
  sampleTime = (float)(targetBytes / BYTESPERMS) / MSPERSEC;

  //  fprintf(stdout, "%d ms requested.\n", cnfg.sampMS);
  fprintf(stdout, "Collecting %10lu Bytes (Nms*8184) [%6.3f sec] in %s\n",
          targetBytes, sampleTime, cnfg.outFname);

  ftS = FT_Purge(cnfg.ftC.ftH, FT_PURGE_RX | FT_PURGE_TX); // Purge both Rx and Tx buffers
  if (ftS != FT_OK)
  {
    fprintf(stderr, "Couldn't purge FTDI FIFO buffer! %d\n", ftS);
    close(cnfg.ofp);
    exit(1);
  }

  while (totalBytes < targetBytes)
  {
    ftS = FT_GetQueueStatus(cnfg.ftC.ftH, &rx.CNT);
    fprintf(stdout, "%s", blankLine);
    fprintf(stdout, "Collected: %10lu Bytes [%10lu left with %5d in queue] ",
            totalBytes, targetBytes - totalBytes, rx.CNT);
    rx.SZE = rx.CNT; // tell it you want the whole buffer
    ftS = FT_Read(cnfg.ftC.ftH, rx.MSG, rx.SZE, &rx.CNT);
    if (ftS != FT_OK)
    {
      fprintf(stderr, "Status not OK %d\n", ftS);
      exit(1);
    }
    else
    {
      if ((rx.CNT < 65536) && (rx.CNT > 0))
      {
        enQueue(&rx, &bfr, rx.CNT);
        while (bfr.SZE > BYTESPERMS)
        {
          //          fprintf(stderr, "ms# %8u", ++mscount);
          deQueue(&bfr, &ms, BYTESPERMS);
          if (mscount++ < cnfg.sampMS)
          {
            if (cnfg.logfile == true)
            {
              printf("Not dealing with this right now\n");
              //fwrite(ms.MSG, sizeof(uint8_t), BYTESPERMS, cnfg.ofp);
            }
            else
            {
            //  printf("DO I need this?\n");
              writeToBinFile(&cnfg, &ms);
            }
          }
          else{break;}
        }

        if ((totalBytes + rx.CNT) > targetBytes)
        {
          rx.CNT = targetBytes - totalBytes;
        }
        totalBytes += rx.CNT;
        fprintf(stdout, "ms# %8u", mscount);
      }                          // end buffer read not too big
      memset(rx.MSG, 0, rx.CNT); // May not be necessary
    }                            // end Read was not an error
  }                              // end while loop

  if (FT_W32_PurgeComm(cnfg.ftC.ftH, PURGE_TXCLEAR | PURGE_RXCLEAR))
  {
    fprintf(stdout, "\n\t   %10lu Bytes written to %s\n",
            cnfg.logfile == true ? totalBytes : totalBytes * 2, cnfg.outFname);
  }
  ftS = FT_Close(cnfg.ftC.ftH);
  close(cnfg.ofp);

  return 0;
}