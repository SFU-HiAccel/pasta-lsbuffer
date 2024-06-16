#include <stdio.h>

// Function to find the index of the first 0 bit in a number
int find_first_zero_bit_index(unsigned char num) {
    for (int i = 0; i < 8; i++) {
        if (!(num & (1 << i))) {
            return i;
        }
    }
    return -1;  // This won't happen for numbers 0-255
}

int main() {
    unsigned char numbers[256];
    // Fill the numbers array with values from 0 to 255
    for (int i = 0; i < 256; i++) {
        numbers[i] = i;
    }

    printf("uint8_t byte_lut[256] = {");

    // Iterate through each number and find the first 0 bit
    for (int i = 0; i < 256; i++) {
        if(i%16 == 0)
          printf("\n");
        int bit_index = find_first_zero_bit_index(numbers[i]);
        printf("%d, ", bit_index);
    }
    printf("};\n");

    return 0;
}
