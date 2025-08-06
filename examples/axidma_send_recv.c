/**
 * @file axidma_send_recv.c
 * @date Tue, Jan 02, 2024
 * @author ChatGPT
 *
 * Simple program to test AXI DMA send and receive channels. It transmits a
 * buffer filled with a known pattern and verifies that the received buffer
 * matches the transmitted data. The program can optionally take the transmit
 * and receive channel IDs as well as the transfer size.
 *
 * Usage: axidma_send_recv [-t <tx channel>] [-r <rx channel>] [-s <bytes>]
 *
 * @bug No known bugs.
 **/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include "util.h"               // Command line parsing helpers
#include "conversion.h"         // BYTE_TO_MIB macro for pretty printing
#include "libaxidma.h"          // AXI DMA library interface

/*----------------------------------------------------------------------------
 * Command Line Interface
 *----------------------------------------------------------------------------*/

// Prints the usage for this program
static void print_usage(bool help)
{
    FILE *stream = (help) ? stdout : stderr;

    fprintf(stream, "Usage: axidma_send_recv [-t <DMA tx channel>] "
            "[-r <DMA rx channel>] [-s <transfer size bytes>]\n");
    if (!help) {
        return;
    }

    fprintf(stream, "\t-t <DMA tx channel>:\tDMA channel used for transmitting.\n");
    fprintf(stream, "\t-r <DMA rx channel>:\tDMA channel used for receiving.\n");
    fprintf(stream, "\t-s <transfer size>:\tNumber of bytes to transfer.\n");
    fprintf(stream, "\t-h:\t\t\tDisplays this help message.\n");
    return;
}

/* Parses the command line options and sets the parameters accordingly. */
static int parse_args(int argc, char **argv, int *tx_channel,
                      int *rx_channel, int *transfer_size)
{
    char option;
    int int_arg;
    int rc;

    // Set defaults
    *tx_channel = -1;
    *rx_channel = -1;
    *transfer_size = 1024 * 1024; // 1 MiB default
    rc = 0;

    while ((option = getopt(argc, argv, "t:r:s:h")) != (char)-1)
    {
        switch (option)
        {
            case 't':
                rc = parse_int(option, optarg, &int_arg);
                if (rc < 0) {
                    print_usage(false);
                    return rc;
                }
                *tx_channel = int_arg;
                break;
            case 'r':
                rc = parse_int(option, optarg, &int_arg);
                if (rc < 0) {
                    print_usage(false);
                    return rc;
                }
                *rx_channel = int_arg;
                break;
            case 's':
                rc = parse_int(option, optarg, &int_arg);
                if (rc < 0) {
                    print_usage(false);
                    return rc;
                }
                *transfer_size = int_arg;
                break;
            case 'h':
                print_usage(true);
                exit(0);
            default:
                print_usage(false);
                return -EINVAL;
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * Main
 *----------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
    int rc;
    int tx_channel, rx_channel, size;
    axidma_dev_t dev;
    const array_t *tx_chans, *rx_chans;
    void *tx_buf, *rx_buf;
    int i;

    if (parse_args(argc, argv, &tx_channel, &rx_channel, &size) < 0) {
        return 1;
    }

    dev = axidma_init();
    if (dev == NULL) {
        fprintf(stderr, "Error: Failed to initialize the AXI DMA device.\n");
        return 1;
    }

    tx_chans = axidma_get_dma_tx(dev);
    if (tx_chans->len < 1) {
        fprintf(stderr, "Error: No transmit channels were found.\n");
        rc = 1;
        goto destroy_dev;
    }
    rx_chans = axidma_get_dma_rx(dev);
    if (rx_chans->len < 1) {
        fprintf(stderr, "Error: No receive channels were found.\n");
        rc = 1;
        goto destroy_dev;
    }

    if (tx_channel == -1) {
        tx_channel = tx_chans->data[0];
    }
    if (rx_channel == -1) {
        rx_channel = rx_chans->data[0];
    }

    printf("AXI DMA Send/Receive Test:\n");
    printf("\tTransmit Channel: %d\n", tx_channel);
    printf("\tReceive Channel: %d\n", rx_channel);
    printf("\tTransfer Size: %.2f MiB (%d bytes)\n\n",
           BYTE_TO_MIB(size), size);

    tx_buf = axidma_malloc(dev, size);
    if (tx_buf == NULL) {
        rc = -ENOMEM;
        goto destroy_dev;
    }
    rx_buf = axidma_malloc(dev, size);
    if (rx_buf == NULL) {
        rc = -ENOMEM;
        goto free_tx;
    }

    // Initialize buffers
    for (i = 0; i < size; i++) {
        ((unsigned char *)tx_buf)[i] = (unsigned char)(i & 0xFF);
    }
    memset(rx_buf, 0, size);

    rc = axidma_twoway_transfer(dev, tx_channel, tx_buf, size, NULL,
                                rx_channel, rx_buf, size, NULL, true);
    if (rc < 0) {
        fprintf(stderr, "DMA transfer failed.\n");
        goto free_rx;
    }

    rc = memcmp(tx_buf, rx_buf, size);
    if (rc != 0) {
        fprintf(stderr, "Data mismatch between sent and received buffers.\n");
    } else {
        printf("DMA transfer successful! Data verified.\n");
    }

free_rx:
    axidma_free(dev, rx_buf, size);
free_tx:
    axidma_free(dev, tx_buf, size);

destroy_dev:
    axidma_destroy(dev);
    return (rc != 0);
}

