#ifndef _CONFIG_H
#define _CONFIG_H


// Circular Buffer 
#define BUFFER_SIZE     4096           // Size of each segment in the circular buffer
#define NUM_SEGMENTS    20             // Number of segments in the circular buffer
#define TOTAL_BUFFER_SIZE (BUFFER_SIZE * NUM_SEGMENTS)

#define FILE_EXTENSION      ".wav"
#define ENERGY_THRESHOLD    5000       // Adjust this value based on your microphone sensitivity

// STATUS LED GPIO 
#define BUILTIN_LED GPIO_NUM_21


// FILE limits
#define MAX_PATH_LENGTH 255
#define MAX_FILE_SIZE_MB    1                                 // Max file size in megabytes
#define MAX_FILE_SIZE       (MAX_FILE_SIZE_MB * 1024 * 1024)  // Max file size in bytes



#endif