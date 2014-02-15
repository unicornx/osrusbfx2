#!/usr/bin/python
# Filename: utility.py
'''Common utilities.

'''

from os import listdir, chdir
from os.path import exists, islink, join, abspath, realpath
import re

def find_device(search_path):
	'''Find the osrfx2 device connected.
	
	'''
	found_path = ""
	
	dev_search_path = abspath(search_path)
	if exists(dev_search_path) == False:
		return ""

	usb_dirs = []
	pattern = re.compile(r'osrfx2_[0-255]')
	for file_name in listdir(dev_search_path):
		dev_path = join(dev_search_path, file_name) 
		if islink(dev_path) and pattern.match(file_name):
			found_path = realpath(dev_path)
			print("find OSR USB-FX2 device %s -> %s" %(dev_path, found_path))
			break # return juct when one and only one was found
	if found_path == "":
		print("Can't find OSR USB-FX2 device")
	
	return found_path

