#include <ctype.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

void usage(char *name) {
  fprintf(stderr, "Run a command on some system power condition\n"
    "Usage:\n"
    "\t%s [--verbose] -e|--event <an-event> [-e|--event <an-other-event> ...] -- /path/to/command arg1 arg2 ...\n\n"
    "Available events:\n"
    "\tidleSleep  : When the mac is put to sleep by the energy saver settings\n"
    "\tforceSleep : When you put your mac to sleep or close the lid\n"
    "\twakeUp     : When the mac is waking up\n"
    "\twokenUp    : After all devices are woken up\n\n"
    "Example:\n"
    "\t%s --event forceSleep -- /sbin/umount /Volumes/MyUSB_hd\n\n", name, name);
  
  exit(1);
}

io_connect_t rootPort;
int verbose_flag;
uint8_t interest;
  
void addInterest(char *stringMessageType, uint8_t *interest) {
  if (strcmp(stringMessageType, "idleSleep") == 0) {
    *interest |= 1 << 3;
    return;
  } 
  if (strcmp(stringMessageType, "forceSleep") == 0) {
    *interest |= 1 << 2;
    return;
  }
  if (strcmp(stringMessageType, "wakeUp") == 0) {
    *interest |= 1 << 1;
    return;
  }
  if (strcmp(stringMessageType, "wokenUp") == 0) {
    *interest |= 1 << 0;
    return;
  }
  fprintf(stderr, "Unknown event. Valid events are: energySleep forceSleep wakeUp wokenUp\n");
}

_Bool isInterestedBy(natural_t messageType, uint8_t *interest) {
  switch (messageType) {
    case kIOMessageCanSystemSleep:
    return *interest & (1 << 3);
    case kIOMessageSystemWillSleep:
    return *interest & (1 << 2);
    case kIOMessageSystemWillPowerOn:
    return *interest & (1 << 1);
    case kIOMessageSystemHasPoweredOn:
    return *interest & (1 << 0);
    default:
    return FALSE;
  }
  return TRUE;
}

char *eventName(natural_t messageType) {
  switch (messageType) {
    case kIOMessageCanSystemSleep:
    return "idleSleep";
    case kIOMessageSystemWillSleep:
    return "forceSleep";
    case kIOMessageSystemWillPowerOn:
    return "wakeUp";
    case kIOMessageSystemHasPoweredOn:
    return "wokenUp";
    default:
    return "unknown event";
  }
  
}

void dumpInterest(uint8_t *interest) {
  static uint interests[] = {kIOMessageCanSystemSleep, kIOMessageSystemWillSleep, kIOMessageSystemWillPowerOn, kIOMessageSystemHasPoweredOn, 0};
  uint *i = &interests[0];
  while(*i) {
    fprintf(stdout, "%12s: %s\n", eventName(*i), isInterestedBy(*i, interest) ? "YES" : "NO");
    i++;
  }
}

char* commandFromArgv(char**argv) {
  char** arg = &argv[0];
  int length = 0;
  while(*arg != NULL) {
    length += strlen(*arg) + 1;
    arg++;
  }
  
  char *cmd = malloc(sizeof(char)*length + 1);
  memset(cmd, '\0', length + 1);
  
  arg = &argv[0];
  while(*arg != NULL) {
    strcat(cmd, *arg);
    strcat(cmd, " ");
    arg++;
  }
  return cmd;
}

void runCommand(char ** argv) {
  pid_t pid;
  pid = fork();
  if(pid == 0) {
      execv(argv[0], argv);
      fprintf(stderr, "Unknown command: %s: %s\n", argv[0], strerror(errno));
      exit(1);
  } else {
    if (pid == -1) {
      fprintf(stderr, "Fork failed: %s\n", strerror(errno));
    } else {
      int childStatus;
      if (waitpid(pid, &childStatus, 0) == -1) {
        fprintf(stderr, "Wait failed: %s\n", strerror(errno));
        } else {
          if (verbose_flag) fprintf(stdout, "Child exit status: %d\n", childStatus);
        }
    }    
  }
}


void systemCallback(void * command, io_service_t service, natural_t messageType, void * messageArgument) {
  if (verbose_flag) { 
    fprintf(stdout, "Got event \'%s\'\n", eventName(messageType));
    fflush(stdout);
  }
  
  switch (messageType) {
    case kIOMessageCanSystemSleep:
      if (command != NULL) runCommand(command);
      IOAllowPowerChange(rootPort, (long)messageArgument);
      break;

    case kIOMessageSystemWillSleep:
      if (isInterestedBy(messageType, &interest)) {
        if (command != NULL) runCommand(command);
      }
      IOAllowPowerChange(rootPort, (long)messageArgument );
      break;

    case kIOMessageSystemWillPowerOn:
      if (isInterestedBy(messageType, &interest)) {
        if (command != NULL) runCommand(command);
      }
      break;

    case kIOMessageSystemHasPoweredOn:
      if (isInterestedBy(messageType, &interest)) {
        if (command != NULL) runCommand(command);
      }
      break;

    default:
      break;
  }
}

int main(int argc, char **argv) {
  char **command;
  
  static struct option long_options[] = {
    {"verbose", no_argument, &verbose_flag, 1},
    {"event",   required_argument, 0, 'e'},
    {0, 0, 0, 0}
  };
  
  int c;  
  while ((c = getopt_long (argc, argv, "e:?h", long_options, NULL)) != -1) {
         
    switch (c) {
      case 'e':
      addInterest(optarg, &interest);
      break;
      
      case '?':
      case 'h':
      usage(argv[0]);
      break;
     
      default:
      break;
    }
  }
     
  if (verbose_flag) fprintf(stdout, "Verbose mode\n");
  
  _Bool observing = FALSE;
           
  if (argc > 1) {
    if (optind < argc) {
      command = malloc(sizeof(char*) * (argc - optind) + 1 /* NULL terminated */);
      int i;
      for (i = optind; i < argc; i++) command[i - optind] = argv[i];
      command[argc - optind] = NULL;
      if (verbose_flag) fprintf(stdout, "Command: %s\n", commandFromArgv(command));
    } else {
      if (!verbose_flag) usage(argv[0]);
      fprintf(stdout, "No command to run, observing are we?\n");
      observing = TRUE;
    }
  } 
  
  if ((interest == 0) && !observing) { usage(argv[0]); }
  
  if (verbose_flag) {
    fprintf(stdout, "Waiting for events:\n" "-------------------\n");
    dumpInterest(&interest);
  } 
  
  fflush(stdout);

  IONotificationPortRef notifyPortRef; 
  io_object_t notifierObject;     
    
  rootPort = IORegisterForSystemPower((void*)command, &notifyPortRef, systemCallback, &notifierObject);
  if (rootPort == MACH_PORT_NULL) {
    fprintf(stderr, "Failed to setup system power notification\n");
    exit(1);
  }
    
  /* Add a source */
  CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(notifyPortRef), kCFRunLoopCommonModes); 
    
  /* Wait here */
  CFRunLoopRun();

  exit(0);
}