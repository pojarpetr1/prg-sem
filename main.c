/*
 * File name: hw10-main.c
 * Date:      2017/04/14 18:51
 * Author:    Jan Faigl
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <termios.h>
#include <unistd.h> // for STDIN_FILENO

#include <pthread.h>

#include "computation.h"
#include "prg_serial_nonblock.h"
#include "messages.h"
#include "event_queue.h"
#include "utils.h"

#define SERIAL_READ_TIMOUT_MS 500 // timeout for reading from serial port

// shared data structure
typedef struct
{
    bool quit;
    int fd; // serial port file descriptor
} data_t;

pthread_mutex_t mtx;
pthread_cond_t cond;

void call_termios(int reset);

void *input_thread(void *);
void *serial_rx_thread(void *); // serial receive buffer

bool send_message(data_t *data, message *msg);
bool receive_message(uint8_t *msg_buf, uint8_t *rx_buf, int size, int *len, int rx_out);

// - main ---------------------------------------------------------------------
int main(int argc, char *argv[])
{
    data_t data = {.quit = false, .fd = -1};
    const char *serial = argc > 1 ? argv[1] : "/dev/ttyACM0";
    data.fd = serial_open(serial);

    if (data.fd == -1)
    {
        fprintf(stderr, "ERROR: Cannot open serial port %s\n", serial);
        exit(100);
    }

    enum
    {
        INPUT,
        SERIAL_RX,
        NUM_THREADS
    };
    const char *threads_names[] = {"Input", "Serial In"};

    void *(*thr_functions[])(void *) = {input_thread, serial_rx_thread};

    pthread_t threads[NUM_THREADS];
    pthread_mutex_init(&mtx, NULL); // initialize mutex with default attributes
    pthread_cond_init(&cond, NULL); // initialize mutex with default attributes

    call_termios(0);

    for (int i = 0; i < NUM_THREADS; ++i)
    {
        int r = pthread_create(&threads[i], NULL, thr_functions[i], &data);
        fprintf(stderr, "INFO: Create thread '%s' %s\n", threads_names[i], (r == 0 ? "OK" : "FAIL"));
    }

    // example of local variables for computation and messaging
    struct
    {
        uint16_t chunk_id;
        uint16_t nbr_tasks;
        uint16_t task_id;
        bool computing;
    } computation = {0, 0, 0, false};

    message msg;
    computation_init();

    bool quit = false;
    while (!quit)
    {
        /* example of the event queue*/
        event ev = queue_pop();
        if (ev.source == EV_KEYBOARD)
        {
            msg.type = MSG_NBR;
            // handle keyboard events
            switch (ev.type)
            {
            case EV_GET_VERSION:
            { // prepare packet for get version
                msg.type = MSG_GET_VERSION;
                fprintf(stderr, "INFO: Get version requested\n");
            }
            break;
            case EV_RESET_CHUNK:
            {
                fprintf(stderr, "INFO: Chunk reset request\n\r");
                if (!computation.computing)
                {
                    computation.chunk_id = 0;
                }
                else
                {
                    fprintf(stderr, "WARN: Chunk reset request discarded, it is currently computing\n\r");
                }
            }
            break;
            case EV_ABORT:
            {
                if (computation.computing)
                {
                    msg.type = MSG_ABORT;
                    computation.computing = false;
                }
                else
                {
                    fprintf(stderr, "WARN: Abort requested but it is not computing\n\r");
                }
            }
            break;
            case EV_SET_COMPUTE:
                info(set_compute(&msg) ? "set compute" : "fail set compute");
                break;
            case EV_COMPUTE:
            {
                info(compute(&msg) ? "compute" : "fail compute");
            }
            break;
            case EV_QUIT:
            {
                if (computation.computing)
                {
                    msg.type = MSG_ABORT;
                    computation.computing = false;
                }
                pthread_mutex_lock(&mtx);
                data.quit = true;
                pthread_mutex_unlock(&mtx);
                quit = true;
            }
            break;
            default:
                break;
            }
            if (msg.type != MSG_NBR)
            { // messge has been set
                if (!send_message(&data, &msg))
                {
                    fprintf(stderr, "ERROR: send_message() does not send all bytes of the message!\n");
                }
            }
        }
        else if (ev.source == EV_NUCLEO)
        { // handle nucleo events
            if (ev.type == EV_SERIAL)
            {
                message *msg = ev.data.msg;
                switch (msg->type)
                {
                case MSG_STARTUP:
                {
                    char str[STARTUP_MSG_LEN + 1];
                    for (int i = 0; i < STARTUP_MSG_LEN; ++i)
                    {
                        str[i] = msg->data.startup.message[i];
                    }
                    str[STARTUP_MSG_LEN] = '\0';
                    fprintf(stderr, "INFO: Nucleo restarted - '%s'\n", str);
                }
                break;
                case MSG_VERSION:
                    if (msg->data.version.patch > 0)
                    {
                        fprintf(stderr, "INFO: Nucleo firmware ver. %d.%d-p%d\n", msg->data.version.major, msg->data.version.minor, msg->data.version.patch);
                    }
                    else
                    {
                        fprintf(stderr, "INFO: Nucleo firmware ver. %d.%d\n", msg->data.version.major, msg->data.version.minor);
                    }
                    break;
                case MSG_OK:
                    fprintf(stderr, "INFO: Receive ok from Nucleo\r\n");
                    break;
                case MSG_ERROR:
                    fprintf(stderr, "WARN: Receive error from Nucleo\r\n");
                    break;
                case MSG_COMPUTE_DATA:
                    update_data(&(msg->data.compute_data));
                    //old:
                    if (computation.computing)
                    {
                        fprintf(stderr, "INFO: New data chunk id: %d, task id: %d - results %d\r\n", msg->data.compute_data.chunk_id, msg->data.compute_data.task_id, msg->data.compute_data.result);
                    }
                    else
                    {
                        fprintf(stderr, "WARN: Nucleo sends new data without computing \r\n");
                    }
                    break;
                case MSG_DONE:

                    fprintf(stderr, "INFO: Nucleo reports the computation is done computing: %d\r\n", computation.computing);
                    computation.computing = false;
                    break;
                case MSG_ABORT:
                    computation.computing = false;
                    fprintf(stderr, "INFO: Abort from Nucleo\r\n");
                default:

                    break;
                }
                if (msg)
                {
                    free(msg);
                }
            }
            else if (ev.type == EV_QUIT)
            {
                quit = true;
            }
            else
            {
                // ignore all other events
            }
        }
    } // end main quit

    queue_cleanup(); // cleanup all events and free allocated memory for messages.
    computation_cleanup();
    for (int i = 0; i < NUM_THREADS; ++i)
    {
        fprintf(stderr, "INFO: Call join to the thread %s\n", threads_names[i]);
        int r = pthread_join(threads[i], NULL);
        fprintf(stderr, "INFO: Joining the thread %s has been %s\n", threads_names[i], (r == 0 ? "OK" : "FAIL"));
    }
    serial_close(data.fd);
    call_termios(1); // restore terminal settings
    return EXIT_SUCCESS;
}

// - function -----------------------------------------------------------------
void call_termios(int reset)
{
    static struct termios tio, tioOld;
    tcgetattr(STDIN_FILENO, &tio);
    if (reset)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &tioOld);
    }
    else
    {
        tioOld = tio; // backup
        cfmakeraw(&tio);
        tio.c_oflag |= OPOST;
        tcsetattr(STDIN_FILENO, TCSANOW, &tio);
    }
}

// - function -----------------------------------------------------------------
void *input_thread(void *d)
{
    data_t *data = (data_t *)d;
    bool end = false;
    int c;
    event ev = {.source = EV_KEYBOARD};
    while (!end && (c = getchar()))
    {
        ev.type = EV_TYPE_NUM;
        switch (c)
        {
        case 'g': // get version
            ev.type = EV_GET_VERSION;
            break;
        case 'q':
            end = true;
            ev.type = EV_QUIT;
            break;
        case 'r':
            ev.type = EV_RESET_CHUNK;
            break;
        case 'a':
            ev.type = EV_ABORT;
            break;
        default: // discard all other keys
            if (c >= '1' && c <= '5')
            {
                ev.type = EV_COMPUTE;
                ev.data.param = c - '0';
            }
            break;
        }
        if (ev.type != EV_TYPE_NUM)
        { // new event
            queue_push(ev);
        }
        pthread_mutex_lock(&mtx);
        end = end || data->quit; // check for quit
        pthread_mutex_unlock(&mtx);
    }
    ev.type = EV_QUIT;
    queue_push(ev);
    fprintf(stderr, "INFO: Exit input thead %p\n", (void *)pthread_self());
    return NULL;
}

// - function -----------------------------------------------------------------
void *serial_rx_thread(void *d)
{ // read bytes from the serial and puts the parsed message to the queue
    data_t *data = (data_t *)d;
    uint8_t msg_buf[sizeof(message)]; // maximal buffer for all possible messages defined in messages.h
    event ev = {.source = EV_NUCLEO, .type = EV_SERIAL, .data.msg = NULL};
    message msg_;
    bool end = false;
    unsigned char c;
    int rx_in = 0; // pointer to message buffer
    int msg_size = 0;
    while (serial_getc_timeout(data->fd, SERIAL_READ_TIMOUT_MS, &c) > 0)
    {
    }; // discard garbage

    while (!end)
    {
        pthread_mutex_lock(&mtx);
        end = end || data->quit; // check for quit
        if (end)
        {
            data->quit = true;
        }
        pthread_mutex_unlock(&mtx);
        int r = serial_getc_timeout(data->fd, SERIAL_READ_TIMOUT_MS, &c);
        if (r > 0)
        { // character has been read
            if (rx_in == 0)
            {
                if (!get_message_size(c, &msg_size))
                {
                    fprintf(stderr, "ERROR: Unknown message type has been received 0x%x\n - '%c'\r", c, c);
                }
            }
            rx_in %= sizeof(message);
            msg_buf[rx_in++] = c;
            if (rx_in == msg_size)
            {
                if (parse_message_buf((uint8_t *)&msg_buf, msg_size, &msg_))
                {
                    message *p_msg = malloc(sizeof(message));
                    if (p_msg == NULL)
                    {
                        fprintf(stderr, "ERROR: Malloc\r\n");
                        end = true;
                        pthread_mutex_lock(&mtx);
                        data->quit = true;
                        pthread_mutex_unlock(&mtx);
                    }
                    else
                    {
                        p_msg->cksum = msg_.cksum;
                        p_msg->data = msg_.data;
                        p_msg->type = msg_.type;
                        ev.data.msg = p_msg;
                        queue_push(ev);
                        rx_in = 0;
                    }
                }
                else
                {
                    fprintf(stderr, "ERROR: Cannot parse message type %d\n\r", msg_buf[0]);
                }
            }
        }
        else if (r == 0)
        { // read but nothing has been received
            rx_in = 0;
        }
        else
        {
            fprintf(stderr, "ERROR: Cannot receive data from the serial port\n");
            end = true;
        }
    }
    ev.type = EV_QUIT;
    queue_push(ev);
    fprintf(stderr, "INFO: Exit serial_rx_thread %p\n", (void *)pthread_self());
    return NULL;
}

// - function -----------------------------------------------------------------
bool send_message(data_t *data, message *msg)
{
    int ret = true;
    uint8_t msg_buf[sizeof(message)];
    int len = 0;
    if (fill_message_buf(msg, (uint8_t *)&msg_buf, sizeof(message), &len))
    {
        for (int i = 0; i < len; ++i)
        {
            if (serial_putc(data->fd, msg_buf[i]) != 1)
            {
                ret = false;
                break;
            }
        }
    }
    else
    {
        ret = false;
    }
    return ret;
}

/* end of prg-sem.c */
