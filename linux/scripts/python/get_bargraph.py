#!/usr/bin/python
# Filename: get_bargraph.py
'''Script to get status of bar graph on osrfx2 board.

'''

from commands import getoutput
from utility import find_device

dev_path = find_device("/sys/class/usb")

command = 'cat' + ' ' + dev_path + '/device/bargraph ' + '2>/dev/null'
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

