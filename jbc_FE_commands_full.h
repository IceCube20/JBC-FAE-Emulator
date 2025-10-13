// SPDX-License-Identifier: MIT OR GPL-2.0-only
//
// Legal Notice:
// This project re-implements only functional protocol behavior (P02) for interoperability purposes.
// It contains no OEM firmware, no cryptographic keys, and does not circumvent any technical protection measures.
// Protocol constants, enums, and IDs are interface identifiers required for device communication.
// “JBC” is a trademark of its respective owners; this is an unofficial, independent project not affiliated with JBC S.A.

#pragma once

#include <stdint.h>

// Namespaces:
//   jbc_cmd::BASE
//   jbc_cmd::SOLD_01, SOLD_02
//   jbc_cmd::HA_02, FE_02, PH_02, SF_02

namespace jbc_cmd {

namespace BASE {
static const uint8_t M_HS = 0;
static const uint8_t M_ACK = 6;
static const uint8_t M_NACK = 21;
static const uint8_t M_SYN = 22;
static const uint8_t M_RESET = 32;
static const uint8_t M_FIRMWARE = 33;
} // namespace BASE

namespace FE_02 {
static const uint8_t M_HS = 0;
static const uint8_t M_EOT = 4;
static const uint8_t M_ACK = 6;
static const uint8_t M_NACK = 21;
static const uint8_t M_SYN = 22;
static const uint8_t M_R_DEVICEIDORIGINAL = 28;
static const uint8_t M_R_DISCOVER = 29;
static const uint8_t M_R_DEVICEID = 30;
static const uint8_t M_W_DEVICEID = 31;
static const uint8_t M_RESET = 32;
static const uint8_t M_FIRMWARE = 33;
static const uint8_t M_CLEARMEMFLASH = 34;
static const uint8_t M_SENDMEMADDRESS = 35;
static const uint8_t M_SENDMEMDATA = 36;
static const uint8_t M_ENDPROGR = 37;
static const uint8_t M_ENDUPD = 38;
static const uint8_t M_CONTINUEUPD = 39;
static const uint8_t M_CLEARING = 40;
static const uint8_t M_FORCEUPDATE = 41;
static const uint8_t M_R_SUCTIONLEVEL = 48;
static const uint8_t M_W_SUCTIONLEVEL = 49;
static const uint8_t M_R_FLOW = 50;
static const uint8_t M_R_SPEED = 51;
static const uint8_t M_R_SELECTFLOW = 52;
static const uint8_t M_W_SELECTFLOW = 53;
static const uint8_t M_R_STANDINTAKES = 54;
static const uint8_t M_W_STANDINTAKES = 55;
static const uint8_t M_R_INTAKEACTIVATION = 56;
static const uint8_t M_W_INTAKEACTIVATION = 57;
static const uint8_t M_R_SUCTIONDELAY = 58;
static const uint8_t M_W_SUCTIONDELAY = 59;
static const uint8_t M_R_DELAYTIME = 60;
static const uint8_t M_R_ACTIVATIONPEDAL = 61;
static const uint8_t M_W_ACTIVATIONPEDAL = 62;
static const uint8_t M_R_PEDALMODE = 63;
static const uint8_t M_W_PEDALMODE = 64;
static const uint8_t M_R_FILTERSTATUS = 65;
static const uint8_t M_R_RESETFILTER = 66;
static const uint8_t M_R_CONNECTEDPEDAL = 68;
static const uint8_t M_R_FILTERSAT = 69;
static const uint8_t M_RESETSTATION = 80;
static const uint8_t M_R_PIN = 81;
static const uint8_t M_W_PIN = 82;
static const uint8_t M_R_STATIONLOCKED = 83;
static const uint8_t M_W_STATIONLOCKED = 84;
static const uint8_t M_R_BEEP = 85;
static const uint8_t M_W_BEEP = 86;
static const uint8_t M_R_CONTINUOUSSUCTION = 87;
static const uint8_t M_W_CONTINUOUSSUCTION = 88;
static const uint8_t M_R_STATERROR = 89;
static const uint8_t M_R_DEVICENAME = 91;
static const uint8_t M_W_DEVICENAME = 92;
static const uint8_t M_R_PINENABLED = 93;
static const uint8_t M_W_PINENABLED = 94;
static const uint8_t M_W_WORKINTAKES = 96;
static const uint8_t M_R_COUNTERS = 192;
static const uint8_t M_R_RESETCOUNTERS = 193;
static const uint8_t M_R_COUNTERSP = 194;
static const uint8_t M_R_RESETCOUNTERSP = 195;
static const uint8_t M_R_USB_CONNECTSTATUS = 224;
static const uint8_t M_W_USB_CONNECTSTATUS = 225;
static const uint8_t M_R_RBT_CONNCONFIG = 240;
static const uint8_t M_W_RBT_CONNCONFIG = 241;
static const uint8_t M_R_RBT_CONNECTSTATUS = 242;
static const uint8_t M_W_RBT_CONNECTSTATUS = 243;
} // namespace FE_02



} // namespace jbc_cmd
