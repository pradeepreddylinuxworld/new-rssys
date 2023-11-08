# Getting Started
This README describe some details about the linux device driver for Syntiant NDP10x, NDP120 chips, 
and how to compile and run the module in a linux-based platform (i.e., Raspberry pi).

## <font color="red">Important Prerequisite</font>

The default python version for Syntiant PI systems is **Python 3**.  This makes installing packages via `apt` or `apt-get` difficult/impossible since many packages have setup/teardown scripts that only work with Python2.

Perform the following commands:
* `sudo update-alternatives --install /usr/bin/python python /usr/bin/python2.7 1`
* `sudo update-alternatives --install /usr/bin/python python /usr/bin/python3.5 2`


### Before using `dpkg`, `apt`, or `apt-get`:

`sudo update-alternatives --config python`

You should be presented with a list:

```
  Selection    Path                Priority   Status
------------------------------------------------------------
* 0            /usr/bin/python3.5   2         auto mode
  1            /usr/bin/python2.7   1         manual mode
  2            /usr/bin/python3.5   2         manual mode
```
Choose Python 2.7

Please follow procedures from following link to setup a Pi for kernel development.
https://syntiant.atlassian.net/wiki/spaces/SYNTIANTEM/pages/592707601/Setting+up+Raspberry+Pi+for+Kernel+Module+Development


## Make and Load ndp10x Module 

In the linux_driver directory run `make` command to automatically generate the 
`syn_ndp.ko` file.
Then, to insert the module into the kernel run the following:

```
sudo insmod syn_ndp.ko 
```
The following command remove module from the kernel:
```
sudo rmmod syn_ndp.ko 
```

## Test and Run Alexa Demo Using Device Driver

In the test directory there is two utilities program.  
Issuing `make` command will generate the binaries for both of them 
(i.e. `driver_test` and `rw_test`).  
These two programs can only be executed if the syn_ndp1 module is loaded into the kernel in advance. 

`rw_test` allows read /write from/to the mcu/spi  which mainly can be used for 
debugging purpose.
`driver_test` runs some tests and if  all of them executed successfully, 
performs `watch` command and print out the class number of the 
keyword (e.g., alexa) if it is detected. 
This test program shows how to run `INIT`, `TRANSFER`, `LOAD`, and `WATCH` 
ioctl s which are the fundamental commands to run the alexa demo.

## Device Driver Architecture (version 1)

The following figure shows the current architecture of the syn_ndp device driver.
The driver comprises the syn_ndp Ilib logic, ndp spi protocol driver (ndp10x_spi_driver) and 
multiple ioctls that expose important functionality of syn_ndp device diver to the user space.

![arch1](fig/arch1.png)


## Device Driver Architecture (version 2)
The following figure shows the tentative (version 2) ndp10x device driver architecture. 
Ndp10x driver is connected to The Advanced Linux Sound Architecture (ALSA) framework. 
ALSA provides efficient support for all types of audio interfaces, and sound card device drivers. 
The higher-level abstraction APIs and sound server in user space work on top of ALSA.
In the kernel space, an audio device driver (e.g. Qualcomm driver) is connected to ALSA 
and ALSA abstracts the sound device driver layer. 
Ndp10x device driver needs to be modified to configure and manipulate the sound data 
stream coming from the external microphone device.

![arch2](fig/arch2.png)


## Procfs nodes
The driver now employs a couple of procfs nodes to help with system integration, in particular  
related to sharing of the DMIC with an AP.

The current procfs nodes are:
- /proc/syn_ndp/info                     (read only, shows simple information about model+firmware)  
- /proc/syn_ndp/mic_ctl                  (r/w)   

Nodes can be read via cat:

```
# cat /proc/syn_ndp/info
Firmware: ndp10x-b0-kw_v27
Parameters: alexa_105
NDP MIC Output: 1 [832000 Hz]

# cat /proc/syn_ndp/mic_ctl
1
```

Application can control whether NDP should drive DMIC clk via the mic_ctl node, for instance  
via echo (regular file-based IO):
```
### disables the NDP DMIC clk, and also sets tank & DNN input to "none"
# echo 0 > /proc/syn_ndp/mic_ctl

### enables the NDP DMIC clk, and sets tank & DNN input to PDM0.
# echo 1 > /proc/syn_ndp/mic_ctl
```

The scheme can be extended to enable SPI PCM samples as well, but this is currently not implemented.
  
Idea is that e.g. Android Audio HAL can perform a file-based write to the procfs mic_ctl  
node when it detects that some other application starts recording the microphone
