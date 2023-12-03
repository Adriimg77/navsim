# Importa las bibliotecas necesarias
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    # Ruta al archivo de mundo
    world_file_path = "../worlds/helloooo.world"

    # Define el nodo de Gazebo
    gazebo_node = Node(
        package='gazebo_ros',
        executable='/usr/bin/gazebo',
        name='gazebo',
        output='screen',
        arguments=[world_file_path]
        # arguments=['-s', 'libgazebo_ros_factory.so', world_file_path]
    )

    # Retorna la descripción del lanzamiento con el nodo de Gazebo
    return LaunchDescription([gazebo_node])
