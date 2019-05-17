#ifndef _FONT_H
#define _FONT_H

#include <stdint.h>

const uint8_t font0[5] = {
    0b111,
    0b101,
    0b101,
    0b101,
    0b111,
};
const uint8_t font1[5] = {
    0b001,
    0b001,
    0b001,
    0b001,
    0b001,
};
const uint8_t font2[5] = {
    0b111,
    0b001,
    0b111,
    0b100,
    0b111,
};
const uint8_t font3[5] = {
    0b111,
    0b001,
    0b111,
    0b001,
    0b111,
};
const uint8_t font4[5] = {
    0b101,
    0b101,
    0b111,
    0b001,
    0b001,
};
const uint8_t font5[5] = {
    0b111,
    0b100,
    0b111,
    0b001,
    0b111,
};
const uint8_t font6[5] = {
    0b111,
    0b100,
    0b111,
    0b101,
    0b111,
};
const uint8_t font7[5] = {
    0b111,
    0b001,
    0b010,
    0b010,
    0b100,
};
const uint8_t font8[5] = {
    0b111,
    0b101,
    0b111,
    0b101,
    0b111,
};
const uint8_t font9[5] = {
    0b111,
    0b101,
    0b111,
    0b001,
    0b111,
};

const uint8_t *fontDigits[10] = {
    font0,
    font1,
    font2,
    font3,
    font4,
    font5,
    font6,
    font7,
    font8,
    font9,
};

#endif // _FONT_H
