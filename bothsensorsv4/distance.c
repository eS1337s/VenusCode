#include <libpynq.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "vl53l0x.h"


#define MUX_ADDR 0x70
#define SHM_NAME "/distance_shm"
#define SHM_SIZE 64

static bool iic_write_raw(iic_index_t iic, const uint8_t addr, uint8_t *data, uint16_t length){
    return iic_write_register(iic, addr, 0x00, data, length);
}

static void mux_select(iic_index_t iic, uint8_t channel){
    uint8_t data = (1 << channel);
    iic_write_raw(iic, MUX_ADDR, &data, 1);
    sleep_msec(10);
}

int get_distance_on_channel(iic_index_t iic, uint8_t channel){
    mux_select(iic, channel);

    vl53x sensor;
    if (tofPing(iic, 0x29)!=0){
        printf("Sensor not found on channel %d\n", channel);
        return -1;
    }

    if(tofInit(&sensor, iic, 0x29, 1) != 0){
        printf("Sensor init failed on channel %d\n", channel);
        return -1;
    }

    sleep_msec(100);
    uint32_t dist = tofReadDistance(&sensor);

    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if(shm_fd == -1){
        perror("shm_open failed");
        return dist;
    }

    if(ftruncate(shm_fd, SHM_SIZE) == -1){
        perror("ftruncate failed");
        close(shm_fd);
        return dist;
    }

    char *shm_ptr = mmap(NULL, SHM_SIZE, PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(shm_ptr == MAP_FAILED){
        perror("mmap failed");
        close(shm_fd);
        return dist;
    }

    snprintf(shm_ptr, SHM_SIZE, "CH%d:%u\n", channel, dist);

    munmap(shm_ptr,SHM_SIZE);
    close(shm_fd);

    return dist;
}