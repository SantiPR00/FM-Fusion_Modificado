
# FM-Fusion: LiDAR-RGB Fusion for Semantic Mapping (Modified Version)

This repository is based on the original project [FM-Fusion](https://github.com/HKUST-Aerial-Robotics/FM-Fusion), developed by HKUST Aerial Robotics Group. 
The modified version presented here has been adapted as part of a Bachelor's Thesis to facilitate installation, resolve compatibility issues, and support further experimentation in semantic segmentation using sensor fusion.

Additionally, this repository references the associated paper published by the original authors: 
**LiDAR-RGB Fusion for Semantic Mapping using Multi-layered Height Maps**. 
Available at: [https://ieeexplore.ieee.org/document/10403989](https://ieeexplore.ieee.org/document/10403989)

## Overview of Modifications

- Simplified instructions for installation and compilation
- Integration of FM-Fusion as part of a larger comparative study on segmentation methods
- Parameters modified in order to improve the results of the system
- Clarified usage of datasets and parameters for reproducibility

## How to Use

1. Clone the repository:

```bash
git clone https://github.com/SantiPR00/FM-Fusion_Modificado.git
cd FM-Fusion_Modificado
```

2. Install required dependencies as listed in the `README` or `install.sh` (OpenCV, PCL, Eigen, etc.).

3. Build the workspace:

```bash
cd ~/catkin_ws
catkin_make
source devel/setup.bash
```

4. Launch the mapping system using provided ROS launch files and configuration settings.

## Folder Structure

```
repolidar/
├── FM-Fusion/
│   ├── catkin_ws/
|        └── src/
|            └── sgloop_ros/
|                └── data/
|                └── include/
|                └── launch/
|                └── msg/
|                └── src/
│   ├── config/
│   ├── install/
│   └── scripts/
├── Glog/
│   └── glog/
├── open3d/
│   └── Open3D/
```

## Notes

This implementation demonstrates a practical fusion of LiDAR and RGB data for real-time semantic mapping, and has been included in the thesis to explore performance trade-offs when compared to single-sensor segmentation systems.

## Author

Modified and prepared by [SantiPR00](https://github.com/SantiPR00) as part of an academic thesis focused on the adaptation and evaluation of semantic segmentation techniques for robotic perception.
