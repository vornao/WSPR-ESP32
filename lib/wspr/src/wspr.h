// WSPR beacon library: include this to get everything.
//   wspr_protocol.h  timing, tone frequencies, field validation (portable)
//   wspr_schedule.h  which slots to transmit in (portable)
//   wspr_message.h   message encoding with Etherkit JTEncode (Arduino)
//   wspr_beacon.h    the beacon engine: a FreeRTOS task keying a Transmitter (ESP32)
#pragma once

#include "wspr_beacon.h"
#include "wspr_message.h"
#include "wspr_protocol.h"
#include "wspr_schedule.h"
