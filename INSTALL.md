
To install on Ubuntu, follow these instructions. For other kinds of systems, the instructions
should be adapted accordingly.


##  Install prerequisites (Ubuntu):

sudo apt-get install \
    g++ cmake libusb-1.0-0.dev libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-gfx-dev


##  Download and compile liblightstone, newest version 1.5

wget https://github.com/openyou/liblightstone/archive/refs/tags/1.5.tar.gz
tar xvfz 1.5.tar.gz
cd liblightstone-1.5
git clone https://github.com/qdot/compily_buildd.git
mkdir build
cd build
cmake ..
make
cd ../..

You can also retrieve liblightstone at https://github.com/openyou/liblightstone
if the wget line above doesn't work, you're working in Windows without wget, etc.
On GitHub, you should switch to tag 1.5, then select Code -> Download ZIP. 

You might need to bump the CMAKE_MINIMUM_REQUIRED version up
in CMakeLists.txt.


##  Make odysseus-patient-monitor

The default is to compile statically:

./make.sh

You can also compile dynamically:

LINKING="dynamic" ./make.sh


##  Linux: tell udev about the device (instructions for Ubuntu):

Add the device to udev and reload

sudo cp 50-wilddivine.rules /etc/udev/rules.d/
sudo udevadm control --reload 
sudo udevadm trigger



