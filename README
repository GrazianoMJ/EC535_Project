# EC535_Project
The following steps are needed to configure and run this project:

1. Connect the external hardware
2. Start the Gumstix
3. Build, install, and run the Gumstix software
4. Build, install, and run the Android software


## Connecting external hardware
The following connections are used:
 - GPIO 16: EasyDriver Step input
 - GPIO 113: EasyDriver Direction input
 - GPIO 9: EasyDriver Enable input
 - GPIO 28: Snap-action switch (NC-Hot, NO-cold)
 - GPIO 31: MOSFET control for the solenoid
 - GPIO 30: Tilt servo signal pin
 - GPIO 29: Pan servo signal pin

## Running software on the Gumstix
 - Run `make -C km` to build the kernel module. The resulting module will be at `km/DMGturret.ko`
 - Run `make -C remote_motor_control` to build the bluetooth server. The resulting program will be at `remote_motor_control/remote_motor_control`

Copy the two output files to the Gumstix device (e.g. with lrz zmodem). Then:
 - Install the kernel module with `insmod DMGturret.ko`
 - Add a device node: `mknod /dev/motor_control c 61 0`
 - Start the bluetooth server: `./remote_motor_control &`
 
The bluetooth server is running and ready to accept connections from clients. Only one client at a time is accepted, but the server does not need to restart between client connections.

### Running emulated Gumstix software
Run `./emulate.sh` to build the Gumstix software for an emulation environment. It will copy the software to rootfs and start qemu.

If it cannot find any dependencies (e.g. rootfs), it will print a message stating where to put the dependency, and offer an environment variable that can be used to specify an existing instance of the dependency to reuse from elsewhere. The environment variable can be useful to reuse rootfs across multiple homework/lab assignments and this project.

## Building and running the Android software
