 list=[]
for f in os.listdir('.'):
	if os.path.isfile(os.path.join('./',f)):
		print(f,"is a file")
		list.append(f)
		
		pid = commands.getoutput('cat /sys/devices/pci0000:00/0000:00:1d.1/usb3/3-1/idProduct')
