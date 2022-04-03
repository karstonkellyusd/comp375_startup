# replace X with group number
GROUP_NUM=X

# DO NOT CHANGE ANYTHING BELOW THIS POINT

# calculate the SSH port number used for the group
GROUP_PORT=$((6100 + GROUP_NUM))

echo $GROUP_PORT

# name of the virtual machine
VM_NAME=Mininet

DISK_FILE=/var/home/comp375/group$GROUP_NUM/mininet-vm-x86_64.vdi

if [ ! -f "$DISK_FILE" ]; then
	echo "$DISK_FILE doesn't exist."
	echo "Check that you are on the correct machine and that you copied it to the correct location."
	exit
fi

# create and register a VM named VM_NAME
VBoxManage createvm --name $VM_NAME --ostype Ubuntu_64 --register

# allow VM to use 2GB of memory
VBoxManage modifyvm $VM_NAME --memory 2048

# set up a virtual hard drive to use our download image file
VBoxManage storagectl $VM_NAME --name "SATA Controller" --add sata \
	    --bootable on --portcount 1

VBoxManage storageattach $VM_NAME --storagectl "SATA Controller" --port 1 \
	    --device 0 --type hdd --medium $DISK_FILE

VBoxManage modifyvm $VM_NAME --natpf1 "ssh,tcp,,$GROUP_PORT,,22"

#VBoxManage showvminfo $VM_NAME | grep 'Rule'
