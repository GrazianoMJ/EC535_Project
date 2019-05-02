# EC535_Project

## Building the Gumstix software
 - Run `make -C km` to build the kernel module. The resulting module will be at `km/DMGturret.ko`
 - Run `make -C remote_motor_control` to build the bluetooth server. The resulting program will be at `remote_motor_control/remote_motor_control`

### Running in the emulator environment
Run `./emulate.sh` to build the Gumstix software for an emulation environment. It will copy the software to rootfs and start qemu.

If it cannot find any dependencies (e.g. rootfs), it will print a message stating where to put the dependency, and offer an environment variable that can be used to specify an existing instance of the dependency to reuse from elsewhere. The environment variable can be useful to reuse rootfs across multiple homework/lab assignments and this project.

## Building the Android software
