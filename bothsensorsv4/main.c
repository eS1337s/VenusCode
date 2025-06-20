/**********************************************************************
 *  ROBOT PROGRAM – sweeping motion with auto-stop, resume *and* retreat
 *  now with Cartesian coordinate tracking
 *
 *  Update – 2025-06-15 (behaviour)
 *  Update – 2025-06-16 (coordinate system implementation)
 *  Update – 2025-06-19 (print previous colour + size)
 *
 *  NEW: The robot now keeps an (x, y) position in grid-units, where:
 *      • Origin is (0, 0) at program start
 *      • +y is the original forward direction
 *      • +x is to the robot’s right when yawPos == 0
 *      • 6 × short spins (left or right) = 90° rotation.
 *      • 100 motor steps of synchronous travel = 1 unit distance.
 *
 *  After every synchronous translation (forward or backward) the program
 *  prints the current coordinates.
 *********************************************************************/
#include <libpynq.h>
#include <iic.h>
#include "tcs3472.h"
#include "vl53l0x.h"
#include <stdio.h>
#include <unistd.h>
#include <stepper.h>
#include <string.h>
#include <stdbool.h>

#include "shared_data.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

RobotData *shm_ptr;

void init_shared_memory() {
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(RobotData));
    shm_ptr = mmap(NULL, sizeof(RobotData), PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
}

/* ────────────────────────────────────────────  Motion constants */
#define SHORT_SPIN_STEPS   100   /* 1 “short” spin   */
#define LONG_MOVE_STEPS    400   /* 1 “long” segment */
#define MOTION_DELAY_MS   1000   /* pause after each motor cmd */
#define SHORT_SPINS_90       6   /* 6 × short spins ≈ 90° */

/* ────────────────────────────────────────────  I²C / sensors */
#define MUX_ADDR            0x70
#define CH3_BUFFER_SIZE     1          /* history for downward-TOF avg */

#define SHM_SIZE 128

static inline uint8_t scale8(uint16_t x) { return x > 4095 ? 255 : (x >> 4); }

static const char *classify_color(uint16_t red, uint16_t green, uint16_t blue)
{
    uint8_t r = scale8(red), g = scale8(green), b = scale8(blue);
    uint8_t mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    uint8_t mn = r < g ? (r < b ? r : b) : (g < b ? g : b);

    if (mx < 30)                              return "Black";
    if (mn > 180 && mx - mn < 40)             return "White";

    float ratio_rg = (float)r / g;
    float ratio_rb = (float)r / b;
    if (r > 60 && r < 200 && g > 30 && g < 140 && b < 80 &&
        ratio_rg < 2.2f && ratio_rb < 4.0f)   return "Brown";

    if (r == mx) return "Red";
    if (g == mx) return "Green";
    return "Blue";
}

/* I²C raw write + MUX select */
static bool iic_write_raw(iic_index_t bus, uint8_t addr,
                          uint8_t *data, uint16_t len)
{ return iic_write_register(bus, addr, 0x00, data, len); }

static void mux_select(iic_index_t bus, uint8_t ch)
{
    uint8_t data = (1U << ch);
    iic_write_raw(bus, MUX_ADDR, &data, 1);
    sleep_msec(10);
}

/* ────────────────────────────────────────────  Globals */
static tcs3472   color_sensors[2] = { TCS3472_EMPTY, TCS3472_EMPTY };
static vl53x     tof_sensors[2];
static uint32_t  ch3_buffer[CH3_BUFFER_SIZE] = {0};
static int       ch3_index = 0, ch3_count = 0;

static int       yawPos = 0;   /* 1 short spin = 15° (left +, right –) */

/* NEW – coordinate tracking */
static int       pos_x = 0;    /* units, +x = right when yawPos == 0 */
static int       pos_y = 0;    /* units, +y = forward when yawPos == 0 */

/* ───────────  NEW – remember previous object reading (colour + size) ────── */
static char     last_down_name[16] = "Unknown";
static uint32_t last_distanceFront = 0;

/* ────────────────────────────────────────────  Forward declarations */
static bool sensor_cycle(void);

/* ────────────────────────────────────────────  Helpers */
static void spin_left_short(void)
{
    stepper_steps(SHORT_SPIN_STEPS, -SHORT_SPIN_STEPS);
    sleep_msec(MOTION_DELAY_MS);
    ++yawPos;
}

static void spin_right_short(void)
{
    stepper_steps(-SHORT_SPIN_STEPS, SHORT_SPIN_STEPS);
    sleep_msec(MOTION_DELAY_MS);
    --yawPos;
}

/* Bring robot back to centre orientation (yawPos → 0) */
static void spin_to_centre(void)
{
    while (yawPos > 0)  spin_right_short();
    while (yawPos < 0)  spin_left_short();
}

/* ────────────────────────────────────────────  Coordinate update & movement */
static void translate_steps(int steps)
{
    /* Perform the synchronous move */
    stepper_steps(steps, steps);
    sleep_msec(MOTION_DELAY_MS);

    /* Update internal (x, y) grid-location */
    int units = steps / 100;          /* 100 steps = 1 unit */
    if (units != 0) {
        int dir24 = ((yawPos % 24) + 24) % 24;  /* 0-23, 24 ticks = 360° */
        int dir = dir24 / 6;                   /* 0:N, 1:E, 2:S, 3:W */
        switch (dir) {
            case 0:  pos_y += units; break;    /* facing +y (north) */
            case 1:  pos_x += units; break;    /* facing +x (east)  */
            case 2:  pos_y -= units; break;    /* facing –y (south) */
            case 3:  pos_x -= units; break;    /* facing –x (west)  */
        }

        printf("Location -> (x=%d, y=%d)\n", pos_x, pos_y);
        //shm_ptr->pos_x = pos_x;
        //shm_ptr->pos_y = pos_y;
        fflush(stdout);
    }
}

/* ────────────────────────────────────────────  Black-line interrupt during sidestep */
static void handle_black_line_interrupt(void)
{
    /* 1. Return to centre orientation */
    spin_to_centre();

    /* 2. 6 × short right spins (≈90°) */
    for (int i = 0; i < SHORT_SPINS_90; ++i) {
        spin_right_short();
        spin_right_short();
    }

    /* 3. Treat this heading as the new centre */
    yawPos = 0;
}

/* ────────────────────────────────────────────  Retreat + avoidance */
static void retreat_and_avoid(void)
{
    /* A. Spin back to centre (yawPos → 0) */
    spin_to_centre();

    /* B. One long *backward* */
    translate_steps(-LONG_MOVE_STEPS);

    /* Early abort if black line appears */
    if (!sensor_cycle()) {
        handle_black_line_interrupt();
        return;
    }

    /* C. Sidestep sequence */

    /*   1) 90° right turn */
    for (int i = 0; i < SHORT_SPINS_90; ++i) {
        spin_right_short();
        if (!sensor_cycle()) {
            handle_black_line_interrupt();
            return;
        }
    }

    /*   2) Long forward */
    translate_steps(LONG_MOVE_STEPS);
    if (!sensor_cycle()) {
        handle_black_line_interrupt();
        return;
    }

    /*   3) 90° left turn (back to original heading) */
    for (int i = 0; i < SHORT_SPINS_90; ++i) {
        spin_left_short();
        if (!sensor_cycle()) {
            handle_black_line_interrupt();
            return;
        }
    }

    /*   4) Optional extra lane-shift forward (still commented) */
    // translate_steps(LONG_MOVE_STEPS);
    // if (!sensor_cycle()) {
    //     handle_black_line_interrupt();
    //     return;
    // }

    /* Centre orientation restored; sidestep completed */
}

/* ────────────────────────────────────────────  Sensor cycle
 *  Returns: true  → path is clear, robot may move
 *           false → object detected *or* black line seen, robot must halt
 */
static bool sensor_cycle(void)
{
    tcsReading colorFront = {0}, colorDown = {0};
    uint32_t   distanceFront = 0, distanceDown = 0;
    char       front_name[16] = "Unknown";

    /* CH0 – front colour */
    mux_select(IIC0, 0);
    if (tcs_get_reading(&color_sensors[0], &colorFront) == TCS3472_SUCCES) {
        strncpy(front_name,
                classify_color(colorFront.red, colorFront.green, colorFront.blue),
                sizeof front_name);
    } else {
        strncpy(front_name, "Error", sizeof front_name);
    }
    front_name[sizeof front_name - 1] = '\0';

    /* rolling average of downward TOF (CH3) */
    float ch3_avg = 0.0f;
    if (ch3_count >= CH3_BUFFER_SIZE) {
        uint32_t s = 0; for (int j = 0; j < CH3_BUFFER_SIZE; ++j) s += ch3_buffer[j];
        ch3_avg = s / (float)CH3_BUFFER_SIZE;
    }

    /* Decide if it is safe to move */
    bool safe_to_move = true;

    if ((ch3_avg >= 80.0f || ch3_count < CH3_BUFFER_SIZE)) {
        if (strcmp(front_name, "Black") == 0) {
            printf("Object: [Line]\n");
            safe_to_move = false;           /* black line */
        }
    } else { /* object very close */
        /* ───────────  NEW: Print previous reading  ─────────── */
        printf("Object: [%s %s]\n",
               (last_distanceFront < 290 ? "Big" : "Small"),
               last_down_name);

        /* build text for shared memory using previous reading */
        snprintf(shm_ptr->text, SHM_SIZE,
                 "[%d, %d, False, %s, %s, False, False]",
                 pos_x, pos_y,
                 last_down_name,
                 (last_distanceFront < 280 ? "6" : "3"));

        /* --------- NOW take fresh measurements for *next* time --------- */
        mux_select(IIC0, 1);
        const char *down_name_now = "Unknown";
        if (tcs_get_reading(&color_sensors[1], &colorDown) == TCS3472_SUCCES)
            down_name_now = classify_color(colorDown.red, colorDown.green, colorDown.blue);

        mux_select(IIC0, 3);
        distanceFront = tofReadDistance(&tof_sensors[0]);

        /* Update “last-cycle” buffers */
        strncpy(last_down_name, down_name_now, sizeof last_down_name);
        last_down_name[sizeof last_down_name - 1] = '\0';
        last_distanceFront = distanceFront;

        safe_to_move = false;
    }

    /* Pretty console output */
    char line[512] = "", seg[128];

    uint8_t r8 = scale8(colorFront.red), g8 = scale8(colorFront.green), b8 = scale8(colorFront.blue);
    snprintf(seg, sizeof seg,
             "FrontColor:\033[48;2;%hhu;%hhu;%hhum   \033[0m "
             "R:%4u G:%4u B:%4u => %-6s | ",
             r8, g8, b8,
             colorFront.red, colorFront.green, colorFront.blue,
             front_name);
    strncat(line, seg, sizeof(line) - strlen(line) - 1);

    mux_select(IIC0, 1);
    if (tcs_get_reading(&color_sensors[1], &colorDown) == TCS3472_SUCCES) {
        r8 = scale8(colorDown.red); g8 = scale8(colorDown.green); b8 = scale8(colorDown.blue);
        const char *dn = classify_color(colorDown.red, colorDown.green, colorDown.blue);
        snprintf(seg, sizeof seg,
                 "DownColor:\033[48;2;%hhu;%hhu;%hhum   \033[0m "
                 "R:%4u G:%4u B:%4u => %-6s | ",
                 r8, g8, b8,
                 colorDown.red, colorDown.green, colorDown.blue,
                 dn);

        /* Keep the very latest reading ready for next cycle */
        strncpy(last_down_name, dn, sizeof last_down_name);
        last_down_name[sizeof last_down_name - 1] = '\0';
    } else {
        snprintf(seg, sizeof seg, "DownColor: RGB ERR                    | ");
    }
    strncat(line, seg, sizeof(line) - strlen(line) - 1);

    mux_select(IIC0, 3);
    distanceFront = tofReadDistance(&tof_sensors[0]);
    snprintf(seg, sizeof seg, "FrontDistance: %4u mm | ", distanceFront);
    strncat(line, seg, sizeof(line) - strlen(line) - 1);

    /* update last_distanceFront each cycle */
    last_distanceFront = distanceFront;

    mux_select(IIC0, 2);
    distanceDown = tofReadDistance(&tof_sensors[1]);
    ch3_buffer[ch3_index] = distanceDown;
    ch3_index = (ch3_index + 1) % CH3_BUFFER_SIZE;
    if (ch3_count < CH3_BUFFER_SIZE) ++ch3_count;

    snprintf(seg, sizeof seg, "DownDistance: %4u mm", distanceDown);
    strncat(line, seg, sizeof(line) - strlen(line) - 1);

    printf("%s\n", line);
    fflush(stdout);
    sleep_msec(80);

    return safe_to_move;
}

/* ────────────────────────────────────────────  MAIN */
int main(void)
{
    /* Hardware & stepper initialisation */
    pynq_init();
    switchbox_set_pin(IO_AR_SCL, SWB_IIC0_SCL);
    switchbox_set_pin(IO_AR_SDA, SWB_IIC0_SDA);
    iic_init(IIC0);

    stepper_init();
    stepper_enable();
    stepper_set_speed(-32536, -32536);
    init_shared_memory();

    /* Initialise sensors on all 4 MUX channels */
    for (int ch = 0; ch < 4; ++ch) {
        mux_select(IIC0, ch);
        if (ch == 0 || ch == 1) {
            tcs_set_integration(&color_sensors[ch], tcs3472_integration_from_ms(60));
            tcs_set_gain(&color_sensors[ch], x4);
            if (tcs_init(IIC0, &color_sensors[ch]) != 0)
                printf("Color sensor init failed on channel %d\n", ch);
        } else {
            if (tofInit(&tof_sensors[ch - 2], IIC0, 0x29, 0) != 0)
                printf("TOF sensor init failed on channel %d\n", ch);
        }
    }
    printf("\n\n");

    /* Sweeping state-machine */
    enum MotionPhase { FORWARD, SPIN_LEFT_A, SPIN_RIGHT, SPIN_LEFT_B };
    enum MotionPhase phase = FORWARD;

    int spin_left_a_cnt = 0;
    int spin_right_cnt  = 0;
    int spin_left_b_cnt = 0;

    while (1) {
        /* 1. Run one sensor frame — retreat if unsafe */
        if (!sensor_cycle()) {
            /* Perform retreat-and-avoid manoeuvre */
            retreat_and_avoid();

            /* Wait until sensors report clear path again */
            while (!sensor_cycle()) {
                /* idle-sensing loop */
            }

            /* Re-initialise sweeping pattern */
            phase = FORWARD;
            spin_left_a_cnt = spin_right_cnt = spin_left_b_cnt = 0;
            yawPos = 0;
            continue;
        }

        /* 2. Safe to move – perform the next step in the pattern */
        switch (phase) {
        case FORWARD:                 /* one long forward */
            translate_steps(LONG_MOVE_STEPS);
            phase = SPIN_LEFT_A;
            break;

        case SPIN_LEFT_A:             /* 6 × short left spins */
            spin_left_short();
            if (++spin_left_a_cnt >= 6) phase = SPIN_RIGHT;
            break;

        case SPIN_RIGHT:              /* 12 × short right spins */
            spin_right_short();
            if (++spin_right_cnt >= 12) phase = SPIN_LEFT_B;
            break;

        case SPIN_LEFT_B:             /* 6 × short left spins */
            spin_left_short();
            if (++spin_left_b_cnt >= 6) {
                /* pattern finished – start over */
                phase = FORWARD;
                spin_left_a_cnt = spin_right_cnt = spin_left_b_cnt = 0;
            }
            break;
        }
    }

    /* Never reached */
    stepper_destroy();
    iic_destroy(IIC0);
    pynq_destroy();
    return 0;
}
