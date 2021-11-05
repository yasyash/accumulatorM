#!/bin/sh

Totalvm=0


for VMID in $(vim-cmd vmsvc/getallvms |awk '/^[0-9]/ {print $1}');
do
 	isalive=$(vim-cmd vmsvc/power.getstate ${VMID})
	val=$(echo $isalive | sed -e "s/Retrieved runtime info Powered //")
	if [ $val = "on" ]; then
 	$(vim-cmd vmsvc/power.shutdown ${VMID})
 	Totalvm=$(($Totalvm + 1))
  fi
done

echo "There're (is) "
echo $Totalvm
echo "guest OS is working."

echo "All of this will be shutdown."

while :
do

echo "waiting 5 seconds for correct down..."
sleep 5

Workingvm=0
	for Cnt in $Totalvm;
	do
		isalive=$(vim-cmd vmsvc/power.getstate ${Cnt})
		val=$(echo $isalive | sed -e "s/Retrieved runtime info Powered //")
		if [ $val = "on" ]; then
		Workingvm=$(($Workingvm +1))
		fi
	done
	if [ $Workingvm -eq 0 ];
	then
	break
	fi
	
done

echo "Guest OS is shutted down successfully. Host server is shutting down..."

poweroff
