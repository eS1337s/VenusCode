#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SHM_NAME "/my_robot_sensors"
#define COLOR_LEN 16

typedef struct {
    //int pos_x;
    //int pos_y;
    //char size[8];
    //char color[COLOR_LEN];
    char text[128];
}RobotData;