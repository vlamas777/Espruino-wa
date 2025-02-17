/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2019 Gordon Williams <gw@pur3.co.uk>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * NMEA decoder
 * ----------------------------------------------------------------------------
 */

#include "jsvar.h"

typedef enum {
  NMEA_NONE = 0,
  NMEA_RMC = 1,
  NMEA_GGA = 2,
  NMEA_GSV = 4,
} NMEAFixPackets;

typedef struct {
  NMEAFixPackets packetsParsed; // which types of packet have been received?
  uint8_t packetCount; // number of packets received
  double lat,lon,alt;
  double speed; // speed in km/h
  double course; // angle of travel in degrees
  int hour,min,sec,ms;
  uint8_t day,month,year; // 1-base day, month and year (eg. as written)
  uint8_t quality; // from GGA packet, 0 = no fix
  uint8_t satellites; // how many satellites
  double hdop; // GGA HDOP - Relative accuracy of horizontal position. Multiply by 5 to get an EXTREMELY ROUGH value in meters
  bool lastWasGSV; // Was the last entry received $GPGSV?
  bool lastWasGGA; // Was the last entry received $GNGGA?
} NMEAFixInfo;

#define NMEA_MAX_SIZE 82  //  82 is the max for NMEA


bool nmea_decode(NMEAFixInfo *gpsFix, const char *nmeaLine);
JsVar *nmea_to_jsVar(NMEAFixInfo *gpsFix);
