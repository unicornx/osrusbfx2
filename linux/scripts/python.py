#!/usr/bin/python
# Filename: utility.py
'''Common utilities.

'''

from os import listdir, chdir
from os.path import isdir, join
import re

def find_device(search_path):
	'''Find the osrfx2 device connected.
	
	'''
	usb_dirs=[]
	pattern = re.compile(r'osrfx2_[0-255]')
	for f in os.listdir(search_path):
		if isdir(join('./', f)):
			match = pattern.match(f)
			if match:
				print("find OSR USB-FX2 device %s" %(f))
				break
	
	
# byteofpython
