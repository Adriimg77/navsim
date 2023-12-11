# Installation



## Set up your machine

You can install NAVSIM on either a real computer or a virtual machine. In the second case, we have successfully tested it with VMware Workstation Player 17. You can obtain it [here](https://www.vmware.com/es/products/workstation-player/workstation-player-evaluation.html).

Configure a computer with **Ubuntu 22.04.3 LTS (Jammy Jellyfish)**. You can obtain a Desktop Image [here](https://releases.ubuntu.com/jammy/). Install the system and update everything.

>:warning: NAVSIM has been developed and tested in the specified Ubuntu version. While it might work in other versions, there are no compatibility guarantees.

> You can install ROS/Gazebo and MATLAB on the same computer. However, running MATLAB on a different computer, including Windows platforms, may be beneficial in the case of extensive simulations where the resources of typical machines may be insufficient. In such scenarios, make sure that the computers are connected to the same network and can communicate with each other.



## Install ROS2

NAVSIM runs on **ROS2** (Robot Operating System) **Humble Hawksbill LTS**. 
Please install it following the official [tutorial](https://docs.ros.org/en/humble/Installation/Ubuntu-Install-Debians.html).


Make sure that:
- A _full desktop_ installation is performed.
- The necessary dependencies (_ros-dev-tools_) for building packages are installed.
- Source ROS2
```bash
source /opt/ros/humble/setup.bash
echo 'source /opt/ros/humble/setup.bash' >> ~/.bashrc
```
Verify the installation with the `ros2` command.



## Install Gazebo

Instal Gazebo 11 (classic) from **Init/Ubuntu Software**. 
To check it, execute the command `gazebo` in a terminal. The graphical interface of Gazebo should open:

![Gazebo](./img/gazebo.png 'Gazebo simulator. :size=600px')

Instalar puente ROS-Gazebo
```bash
sudo apt install ros-humble-gazebo-ros-pkgs
```

Check that ROS2 and Gazebo are linked running an empty scenario:
```bash
ros2 launch gazebo_ros gazebo.launch.py
```
In another terminal run:
```bash
ros2 node list
ros2 topic list
```


## Install Git

```bash
sudo apt update
sudo apt install git
git --version
```

Optionally, install GitHub Desktop:

```bash
wget -qO - https://apt.packages.shiftkey.dev/gpg.key | gpg --dearmor | sudo tee /usr/share/keyrings/shiftkey-packages.gpg > /dev/null
sudo sh -c 'echo "deb [arch=amd64 signed-by=/usr/share/keyrings/shiftkey-packages.gpg] https://apt.packages.shiftkey.dev/ubuntu/ any main" > /etc/apt/sources.list.d/shiftkey-packages.list'
sudo apt update
sudo apt install github-desktop
```


## Clone and compile NAVSIM packages

You can download NAVSIM by cloning the repository in your computer:
```bash
cd
mkdir code
cd code
sudo git clone https://github.com/I3A-NavSys/navsim
```

Then, we need to compile a ROS2 workspace containing the simulation environment:
```bash
cd navsim/ws
colcon build
```

Now we set up .bashrc to make our workspace act as a ROS overlay (leaving Humble as a ROS underlay). Also, we configure Gazebo to find our libraries:
```bash
echo 'source ~/code/navsim/ws/install/setup.bash' >> ~/.bashrc
echo 'export GAZEBO_PLUGIN_PATH=${GAZEBO_PLUGIN_PATH}:~/code/navsim/ws/install/navsim_pkg/lib/navsim_pkg/' >> ~/.bashrc
echo 'export GAZEBO_PLUGIN_PATH=${GAZEBO_PLUGIN_PATH}:~/code/navsim/ws/install/navsim_msgs/lib/' >> ~/.bashrc
source ~/.bashrc
```

We observe two NavSim packages in the list recognized by ROS2:
```bash
ros2 pkg list | grep navsim
```

Finally, we open a world in Gazebo:
```bash
cd ~/code/navsim/ws/src/navsim_pkg/worlds/
clear; gazebo hello.world
```



## Install Python

MATLAB ROS2 Toolbox will require Python 3.10.x.
In Ubuntu, you may install it by executing the following commands in a terminal:
```bash
sudo apt install python3.10-venv
```

In Windows, for example, you may download and install Python 3.10.11 from the [official site](https://www.python.org/ftp/python/3.10.11/python-3.10.11-amd64.exe).



## Install and configure MATLAB

NAVSIM provides a variety of tools for launching and analyzing simulations. These tools are programmed in MATLAB® R2023b. To install it, you can follow these steps:

1. Download **MATLAB R2023b Update 4** from the official [MathWorks website](https://es.mathworks.com/downloads).

2. In Ubuntu, unzip the installer package by running the following commands in your terminal:
```bash
unzip matlab_R2023b_glnxa64.zip -d matlabinstaller
cd matlabinstaller
```
3. In Windows, execute the installer. In Ubuntu, execute the installation script with root privileges:
```bash
sudo ./install
```
4. Follow the installation process:

   a) Select your license and user information.
   
   b) Choose the destination folder (the default location is fine).

   c) Select the MATLAB products you want to install. Ensure that you select at least the following components:
      - MATLAB R2023b
      - Simulink
      - ROS Toolbox
      - StateFlow

   d) Configure installation options:
      - Set _Create symbolic links to MATLAB scripts in:_ `/usr/local/bin`
      - Choose _Improve MATLAB startup performance_ based on your preferences.

   e) Confirm your selections and continue the installation process.

> Once MATLAB is installed, you can delete the installation file and the `matlabinstaller` folder.

5. Open MATLAB. In Windows, start the application. In Ubuntu, execute `matlab` in a terminal.

6. Select **HOME > ENVIRONMENT tile > Preferences**. In the panel on the left, select **ROS Toolbox**. Click on **Open ROS Toolbox Preferences**.

7. In the **ROS Toolbox Preferences** dialog box, set the path to your Python installation.
Typical values may be:

   | OS      | Python path |
   |:--------|:------------|
   | Ubuntu  | /usr/bin/python3.10 |
   | Windows | C:\Users\User\AppData\Local\Programs\Python\Python310\pythonw.exe |


8. Click on **Recreate Python Environment**. When it finishes, press **OK**.
9. 
10. Finally, Matlab needs to use a specific version of the C compiler:
```bash
echo 'export LD_PRELOAD=/lib/x86_64-linux-gnu/libstdc++.so.6' >> ~/.bashrc
source ~/.bashrc
```


### Compiling ROS messages with MATLAB

To compile custom ROS messages, perform the following steps:

1. Open MATLAB.

2. Go to the folder `navsim/matlab/tools/`.

3. Run the script `compile_ros_messages.m`. It may take several minutes. If everything is correct, you should see a message in the MATLAB console saying `Build succeeded`.

4. Open the script `NAVSIM_PATHS`, and set variable **NAVSIM_PATH** with the folder path where your simulator installation resides.


## Running your first simulation with Matlab connection
