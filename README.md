# scream-ivshmem-pulse-transmitter

scream-ivshmem-pulse-transmitter is a [Scream IVSHMEM](https://github.com/duncanthrax/scream/) compatible transmitter for linux guest VMs using PulseAudio as audio input and IVSHMEM to share the audio ring buffer between linux guest and linux host VM.

## Compile

You need pulseaudio headers in advance.

```shell
$ sudo yum install pulseaudio-libs-devel # Redhat, CentOS, etc.
or
$ sudo apt-get install libpulse-dev # Debian, Ubuntu, etc.
```

Run `make` command.

## Usage

You need to create an IVSHMEM device for your VM. Follow the instruction in the [Scream IVSHMEM](https://github.com/duncanthrax/scream/) README.

Launch your VM and verify that you can see the IVSHMEM device with `lspci`.

The output should look like the following:

```shell
00:06.0 RAM memory: Red Hat, Inc. Inter-VM shared memory (rev 01)
        Subsystem: Red Hat, Inc. QEMU Virtual Machine
        .......
```
Now you can access the shared memory in the guest using sysfs.

From the output of `lspci` we can conclude that the device is at `00:06.0`. This will vary in your machine, use your value.

For `00:06.0` you should have this file: `/sys/devices/pci0000:00/0000:00:06.0/resource2_wc`. This is the shared memory.

You can check the file size, it should match the size you set in the host for IVSHMEM.

Once you know the shared memory file you can run the transmitter like this:

```shell
$ scream-ivshmem-pulse-transmitter /sys/devices/pci0000:00/0000:00:06.0/resource2_wc
```

Take a look at the command line parameters if you want to set sample rate, sample size or number of channels.

From pavucontrol or any other tool configure scream-ivshmem-pulse-transmitter to record from the VM sound card monitor.

If you don't have a sound card inside the VM you can create a virtual one executing the following commands.

```shell
pacmd load-module module-null-sink sink_name=Scream-IVSHMEM-sink
pacmd update-sink-proplist Scream-IVSHMEM-sink device.description=Scream-IVSHMEM-sink
```

Set this sound card as the default output if it's not already.

On the host execute one of the Scream IVSHMEM receiver from [here](https://github.com/duncanthrax/scream/tree/master/Receivers)
