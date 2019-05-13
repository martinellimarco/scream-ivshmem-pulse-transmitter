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

Launch your VM, make sure to have write permission for the shared memory device of your linux guest, in the example /dev/shm/scream-ivshmem,  then execute the following command inside the VM:

```shell
$ scream-ivshmem-pulse-transmitter /dev/shm/scream-ivshmem
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
