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
__copyright__	= "Copyright (c) 2014 Unicorn Wang"
__license__		= "GPLv2"

#from os import acccess, listdir, chdir
import os
import sys
import getopt
#from commands import getoutput
from getch import getch

__DEBUG = 1

BARGRAPH_ON = 0x80
BARGRAPH_OFF = 0x00

BARGRAPH_MAXBAR = 4 #CY001 only have 4 LED bars availabe

_device_path = ('', '') # store device path in (devFS, sysFS)

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

def set_bargraph_display(value):
	'''Function to set bar-graph display.

	'''
	str_value = "%d" %(value)
	command = 'echo' + ' ' + str_value + ' > ' + _device_path[1] + '/device/bargraph ' + '2>/dev/null'
	#(status, output) = getstatusoutput(command)
	#return status
	return 0

def handle_light_one_bar():
	print("Which Bar (input number 1 thru %d)?" %(BARGRAPH_MAXBAR))

	try:
		bar = int(getch())
	except:
		print("Error reading input!")
		return

	if(bar == 0 or bar > BARGRAPH_MAXBAR):
		print("Invalid bar number!")
		return

	bar = bar - 1 #normalize to 0 to 3

	barValue = 1 << bar
	barValue = barValue | BARGRAPH_ON

	if( 0 != set_bargraph_display(barValue)):
		print("set_bargraph_display failed with error")
		return


def handle_clear_one_bar():
	print("Which Bar (input number 1 thru %d)?" %(BARGRAPH_MAXBAR))
	try:
		bar = int(getch())
	except:
		print("Error reading input!")
		return

	if(bar == 0 or bar > BARGRAPH_MAXBAR):
		print("Invalid bar number!")
		return

	bar = bar - 1 #normalize to 0 to 3
	barValue = BARGRAPH_OFF; # reset and flag off
	barValue = barValue | (1 << bar)

	if (0 != set_bargraph_display(barValue)):
		print("set_bargraph_display failed with error")
		return

def handle_light_all_bars():
	if( 0 != set_bargraph_display(0x8F)):
		print("set_bargraph_display failed with error")
		return

def handle_clear_all_bars():
	if( 0 != set_bargraph_display(0x0F)):
		print("set_bargraph_display failed with error")
		return

def handle_get_bar_state():
	'''Function to get bar-graph display status.

	'''
	#command = 'cat' + ' ' + _device_path[1] + '/device/bargraph ' + '2>/dev/null'
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

LIGHT_ONE_BAR = b'1'
CLEAR_ONE_BAR = b'2'
LIGHT_ALL_BARS = b'3'
CLEAR_ALL_BARS = b'4'
GET_BAR_GRAPH_LIGHT_STATE = b'5'
GET_MOUSE_POSITION = b'6'
GET_MOUSE_POSITION_AS_INTERRUPT_MESSAGE = b'7'
GET_7_SEGEMENT_STATE = b'8'
SET_7_SEGEMENT_STATE = b'9'
RESET_DEVICE = b'a'
REENUMERATE_DEVICE = b'b'
GET_DEV_INFO = b'c'

def play_with_device():
	'''Function to play with the device.

	'''
	while ( True ):
		print("\nUSBFX TEST -- Functions:");
		print("\t1.  Light Bar");
		print("\t2.  Clear Bar");
		print("\t3.  Light entire Bar graph");
		print("\t4.  Clear entire Bar graph");
		print("\t5.  Get bar graph state");
		print("\t6.  Get Mouse position");
		print("\t7.  Get Mouse Interrupt Message");
		print("\t8.  Get 7 segment state");
		print("\t9.  Set 7 segment state");
		print("\ta. Reset the device");
		print("\tb. Reenumerate the device");
		print("");
		print("\t0. Exit");
		print("\tSelection: ");
	        
		try:
			function = getch()
		except:
			print("Error reading input!");
			break

		if function == LIGHT_ONE_BAR:
			handle_light_one_bar()
			
		elif function == CLEAR_ONE_BAR:
			handle_clear_one_bar()
			
		elif function == LIGHT_ALL_BARS:
			handle_light_all_bars()
			
		elif function == CLEAR_ALL_BARS:
			handle_clear_all_bars()

		elif function == GET_BAR_GRAPH_LIGHT_STATE:
			handle_get_bar_state()

		elif function == GET_MOUSE_POSITION or \
		     function == GET_MOUSE_POSITION_AS_INTERRUPT_MESSAGE or \
		     function == GET_7_SEGEMENT_STATE or \
		     function == SET_7_SEGEMENT_STATE or \
		     function == RESET_DEVICE or \
		     function == REENUMERATE_DEVICE:
		     print("OSRFX2 -- to be done")
		     
		else:
			print("Exiting ~~~")
			sys.exit()
		

def main(argv):

	global _flag_play_with_device
	
	if False == parse_arg(argv):
		sys.exit()

	_device_path = get_device_path()
	if None == _device_path:
		print("Can't find OSR USB-FX2 device")
#		sys.exit()

#	print("find OSR USB-FX2 device:")
#	print(" - %s" %(_device_path[0]))
#	print(" - %s -> %s" %(_device_path[1]))

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


