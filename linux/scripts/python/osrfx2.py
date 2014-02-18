#!/usr/bin/python
# Filename: osrfx2.py
'''Test app for osrfx2 driver.

Usage: python osrfx2.py [options]

Options:
	-r [n] where n is number of bytes to read
	-w [n] where n is number of bytes to write
	-c [n] where n is number of iterations (default = 1)
	-v verbose -- dumps read data
	-p to control bar LEDs, seven segment, and dip switch
	-n to perform select I/O (default is blocking I/O)
	-u to dump USB configuration and pipe info

'''
__author__		= "unicon wang (unicorn_wang@outlook.com)"
__version__		= "$Revision: 0.1 $"
__date__		= "$Date: 2004/05/05 21:57:19 $"
__copyright__	= "Copyright (c) 2014 Unicorn Wang"
__license__		= "GPLv2"

#from os import acccess, listdir, chdir
import os
import sys
import getopt
#from commands import getoutput
from getch import getch

__DEBUG = 1
DEVICE_PATH = ('', '') # store device path in devFS and sysFS

_flag_read					= False
_len_read					= 512
_flag_write					= False
_len_write					= 512
_iteration_count 			= 1
_flag_dump_usb_config 		= False
_flag_play_with_device 		= False
_flag_perform_blocking_io 	= True
_flag_dump_read_data		= False

def find_sys_device(search_path):
	'''Find the osrfx2 device connected in sysFS.
	
	'''
	found_path = ""

	dev_path = os.path.abspath(search_path)
	if os.path.exists(dev_path) == False:
		return ""

	if os.path.islink(dev_path):
		found_path = os.path.realpath(dev_path)
		
	return found_path


def get_device_path():
	''' Get dev and sys path for the osrfx2 device

	'''
	device_dev_path = "/dev/osrfx2_0"
	device_sys_path = find_sys_device("/sys/class/usb/osrfx2_0")

	if False == os.access(device_dev_path, os.R_OK):
		device_dev_path = ""

	if "" == device_dev_path or "" == device_sys_path:
		return None
		
	return [device_dev_path, device_sys_path]

def usage():
	print(__doc__)

def parse_arg(argv):
	'''Parse command line arguments

	True, parse OK, else, parse failed
    When parse OK, sets global flags as per user function request'''
	global _flag_read					
	global _len_read					
	global _flag_write					
	global _len_write					
	global _iteration_count 			
	global _flag_dump_usb_config 		
	global _flag_play_with_device 		
	global _flag_perform_blocking_io
	global _flag_dump_read_data		

	retval = True
	
	try:
		opts, args = getopt.getopt(argv, "r:R:w:W:c:C:hHuUpPnNvV")
	except getopt.GetoptError:
		usage()
		return False

	if ( [] == opts ):
		usage()
		return False

	for opt, arg in opts:

		if opt in ("-r", "-R"):
			_flag_read = True
			_len_read = int(arg)
			
		elif opt in ("-w", "-W"):
			_flag_write = True
			_len_write = int(arg)

		elif opt in ("-c", "-C"):
			_iteration_count = int(arg)
			
		elif opt in ("-u", "-U"):
			_flag_dump_usb_config = True
			
		elif opt in ("-p", "-P"):
			_flag_play_with_device = True
			
		elif opt in ("-n", "-N"):
			_flag_perform_blocking_io = False
			
		elif opt in ("-v", "-V"):
			_flag_dump_read_data = True

		else:
			usage()
			retval = False
			
	return retval

def get_mouse_position_by_interrupt():
	'''Function to get mouse position by interrupt EP.

	'''
	pass

def get_bargraph_display():
	'''Function to get bar-graph display status.

	'''
	#command = 'cat' + ' ' + DEVICE_PATH[1] + '/device/bargraph ' + '2>/dev/null'
	bargraph = getoutput(command)

	print("Bar Graph:")
	bar = 'ON' if bargraph[7] == '*' else 'OFF'; print("    Bar8 is %s" %bar)
	bar = 'ON' if bargraph[6] == '*' else 'OFF'; print("    Bar7 is %s" %bar)
	bar = 'ON' if bargraph[5] == '*' else 'OFF'; print("    Bar6 is %s" %bar)
	bar = 'ON' if bargraph[4] == '*' else 'OFF'; print("    Bar5 is %s" %bar)
	bar = 'ON' if bargraph[3] == '*' else 'OFF'; print("    Bar4 is %s" %bar)
	bar = 'ON' if bargraph[2] == '*' else 'OFF'; print("    Bar3 is %s" %bar)
	bar = 'ON' if bargraph[1] == '*' else 'OFF'; print("    Bar2 is %s" %bar)
	bar = 'ON' if bargraph[0] == '*' else 'OFF'; print("    Bar1 is %s" %bar)


def set_bargraph_display(value):
	'''Function to set bar-graph display.

	'''
	str_value = "%d" %(value)
	command = 'echo' + ' ' + str_value + ' > ' + DEVICE_PATH[1] + '/device/bargraph ' + '2>/dev/null'
	#(status, output) = getstatusoutput(command)
	return status

def play_with_device():
	'''Function to play with the device.

	'''
	while ( True ):
		print("\nUSBFX TEST -- Functions:\n\n");
		print("\t1.  Light Bar\n");
		print("\t2.  Clear Bar\n");
		print("\t3.  Light entire Bar graph\n");
		print("\t4.  Clear entire Bar graph\n");
		print("\t5.  Get bar graph state\n");
		print("\t6.  Get Mouse position\n");
		print("\t7.  Get Mouse Interrupt Message\n");
		print("\t8.  Get 7 segment state\n");
		print("\t9.  Set 7 segment state\n");
		print("\ta. Reset the device\n");
		print("\tb. Reenumerate the device\n");
		print("\n\t0. Exit\n");
		print("\n\tSelection: ");
	        
		try:
			function = int(getch()) # TBD this getch can not accept char other than [0-9]
		except:
			print("Error reading input!");
			break

		print("select %d" %(function))
		#if 
		

def main(argv):

	global _flag_play_with_device
	
	if False == parse_arg(argv):
		sys.exit()

	DEVICE_PATH = get_device_path()
	if None == DEVICE_PATH:
		print("Can't find OSR USB-FX2 device")
#		sys.exit()

#	print("find OSR USB-FX2 device:")
#	print(" - %s" %(DEVICE_PATH[0]))
#	print(" - %s -> %s" %(DEVICE_PATH[1]))

	# start process

	# play
	if True == _flag_play_with_device:
		play_with_device()

	# doing a read, write, or both test
	if True == _flag_read or True == _flag_write:
		if True == _flag_perform_blocking_io:
			print("Starting read/write with Blocking I/O ... ")
			#rw_blocking();
		else:
			print("Starting read/write with NoBlocking I/O ... ")
			#rw_noblocking();

	
import platform
if __name__ == "__main__":

	(major, minor, patchlevel)=platform.python_version_tuple()
	if int(major) >= 3 or int(major) < 2:
		print ("This script should be run with Python version 2.x")
		#sys.exit()

	main(sys.argv[1:])


