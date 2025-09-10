#ifndef DEFINES_H
#define DEFINES_H

// UART Configurations
#define UART_GPS uart0
#define UART_GSM uart1

// GPS Pins
#define GPS_TX 0
#define GPS_RX 1

// GSM Pins
#define GSM_TX 4
#define GSM_RX 5

// GPIO Configurations
#define SOS_BUTTON 10
#define LED_PIN 25

// UART Speeds
#define GPS_BAUD_RATE 9600
#define GSM_BAUD_RATE 9600

// Buffer Sizes
#define GPS_LINE_BUFFER_SIZE 100
#define GSM_BUFFER_SIZE 128
#define COORD_BUFFER_SIZE 16

// Timing configurations (in milliseconds)
#define GSM_CMD_DELAY 500
#define GSM_SMS_PREP_DELAY 200
#define GSM_SMS_SEND_DELAY 5000
#define SOS_DEBOUNCE_DELAY 10000

// Phone number configuration
#define EMERGENCY_PHONE "+549XXXXXXXXXX"

#endif // DEFINES_H
