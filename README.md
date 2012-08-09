# Description
A command line tool to run a script when your Mac is going to sleep or wakes up.

## Install

You need xcode to compile the sources.

```
$ git clone git://github.com/Orion98MC/msp.git
$ cd msp/src
$ make
```

## Usage
```
Run a command on some system power condition
Usage:
	./src/build/msp [--verbose] -e|--event <an-event> [-e|--event <an-other-event> ...] -- /path/to/command arg1 arg2 ...

Available events:
	idleSleep  : When the mac is put to sleep by the energy saver settings
	forceSleep : When you put your mac to sleep or close the lid
	wakeUp     : When the mac is waking up
	wokenUp    : After all devices are woken up

Example:
	./src/build/msp --event forceSleep -- /sbin/umount /Volumes/MyUSB_hd
```