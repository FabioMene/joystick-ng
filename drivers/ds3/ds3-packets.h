/*
 * ds3-packets.h
 * 
 * Copyright 2018-2019 Fabio Meneghetti <fabiomene97@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */


#ifndef DS3_PACKETS_H
#define DS3_PACKETS_H 1

typedef struct {
    unsigned char      type;
    unsigned char      res1;
    unsigned short int keys;
    unsigned char      ps;
    unsigned char      res2;
    unsigned char      LX;
    unsigned char      LY;
    unsigned char      RX;
    unsigned char      RY;
    unsigned char      res3[4];
    unsigned char      Up;
    unsigned char      Right;
    unsigned char      Down;
    unsigned char      Left;
    unsigned char      L2;
    unsigned char      R2;
    unsigned char      L1;
    unsigned char      R1;
    unsigned char      Y;
    unsigned char      B;
    unsigned char      A;
    unsigned char      X;
    unsigned char      res4[3];
    
    unsigned char      cstate;
    // 2: in carica, 3: non in carica
    
    unsigned char      blevel;
    // 238: in carica (livello non disponibile)
    // Alcuni documenti indicano da 0 a 5 per indicare dallo 0% al 100%, ma la ps3 riporta che il joystick è scarico a 3 
    
    unsigned char      conn;
    // Sembra che sia una bitmask dello stato del joystick, da msb a lsb
    //   0 0 0 1 0 b m 0
    // dove
    //   b è 1 se connesso tramite bluetooth (0: usb)
    //   m è 1 se entrambi i motori sono spenti (altrimenti 0)
        
    unsigned char      res5[9];
    unsigned short int accelX;
    unsigned short int accelY;
    unsigned short int accelZ;
    unsigned short int gyroX;
} ds3_report_t;

static unsigned char ds3_output_report_pkt[] = {
    0x00,
    0xff, 0x00, // Small Act (Timeout, activation)
    0xff, 0x00, // Big Act (Timeout, force)
    0x00, 0x00, 0x00, 0x00,
    0x00, // Led Mask (0x02 = Led1, 4=L2, 8=L3, 0x10=L4)
    // Led blink data
    // 0xff, sync/delay?, sync/delay?, Toff, Ton
    // Charging period: 0x40
    // Low battery period: 0x10 
    0xFF, 0x27, 0x10, 0x00, 0x40, // Led 4 (offset +12, +13)
    0xFF, 0x27, 0x10, 0x00, 0x40, // 3 (+17, +18)
    0xFF, 0x27, 0x10, 0x00, 0x40, // 2 (+23, +24)
    0xFF, 0x27, 0x10, 0x00, 0x40, // 1 (+28, +29)
    0x00, 0x00, 0x00, 0x00, 0x00
};

#endif

