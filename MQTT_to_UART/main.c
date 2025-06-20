#include <libpynq.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <unctrl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>


#define SHM_NAME "/my_robot_sensors"
#define SHM_SIZE 128


int main() {
  switchbox_init();
  switchbox_set_pin(IO_AR0, SWB_UART0_RX);
  switchbox_set_pin(IO_AR1, SWB_UART0_TX);

  uart_init(UART0);
  uart_reset_fifos(UART0);

 //--- Shared Memory setup ---
  int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
  if (shm_fd == -1){
    perror("shm_open failed");
    return 1;
  }

  char *shm_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (shm_ptr == MAP_FAILED) {
    perror("mmap failed");
    return 1;
  }

  char buf[256];
  fcntl(0, F_SETFL, fcntl(0,F_GETFL) | O_NONBLOCK);

  while(1){
    //--- Shared Memory Transmit ---
    if (shm_ptr && shm_ptr[0] != '\0') {
      uint32_t length = strnlen(shm_ptr, SHM_SIZE);
    
      if (length == SHM_SIZE) {
        printf("Error: Shared memory string not null-terminated!\n");
        //continue;  // skip sending corrupted/unsafe data
      }

      uint8_t *len_bytes = (uint8_t*)&length;

      printf("[SHM] Sending shared memory data over UART: %s\n", shm_ptr);
      fflush(NULL);

      for (uint32_t i = 0; i < 4; i ++){
        uart_send(UART0, len_bytes[i]);
      }

      for (uint32_t i = 0; i < length; i ++){
        uart_send(UART0, shm_ptr[i]);
      }

      memset(shm_ptr, 0, SHM_SIZE); // Clear after sending
    }

    // --- Typed Input --- 
    int numRead = read(0, buf, 256);

    if(numRead > 0){
      uint32_t length = numRead - 1;
      uint8_t* len_bytes = (uint8_t*)&length;
      printf("<< Outgoing Message: Size = %d\n", length);
      fflush(NULL);
      for(uint32_t i = 0; i < 4; i ++){
        uart_send(UART0, len_bytes[i]);
      }
      for(uint32_t i = 0; i < length; i++){
        uart_send(UART0, buf[i]);
      }
    }

    // --- UART receive ---
    if(uart_has_data(UART0)){
      uint8_t read_len[4];

      for(uint32_t i = 0; i<4; i++){
        read_len[i] = uart_recv(UART0);
      }

      uint32_t length = *((uint32_t*)read_len);

      if (length > 0 && length < 1024) {
        printf(">> Incoming Message: Lenght = %d\n", length);
        fflush(NULL);

        char *buffer = (char*) malloc(length+1);
        if(!buffer) {
          perror("malloc failed");
          continue;
        }

        for (uint32_t i = 0; i < length; i++){
          buffer[i] = (char)uart_recv(UART0);
        }

        buffer[length] = '\0';
        printf(" >%s\n", buffer);
        fflush(NULL);
        free(buffer);
      }
      else{
        printf(">> Incoming Message: Lenght = %d\n", length);
      }
      
      /*fflush(NULL);
      uint32_t i = 0;
      char* buffer = (char*) malloc(sizeof(char) * length);
      while(i<length){
        buffer[i] = (char)uart_recv(UART0);
        i++;
      }
      printf(" >%s\n", buffer);
      fflush(NULL);
      free(buffer);*/
    }

    sleep(1);
  }
  fflush(NULL);
  uart_reset_fifos(UART0);
  uart_destroy(UART0);
  pynq_destroy();
  return EXIT_SUCCESS;
}
