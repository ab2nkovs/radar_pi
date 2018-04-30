/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Radar Plugin
 * Author:   David Register
 *           Dave Cowell
 *           Kees Verruijt
 *           Douwe Fokkema
 *           Sean D'Epagnier
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register              bdbcat@yahoo.com *
 *   Copyright (C) 2012-2013 by Dave Cowell                                *
 *   Copyright (C) 2012-2016 by Kees Verruijt         canboat@verruijt.net *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */

#include "RaymarineControl.h"

PLUGIN_BEGIN_NAMESPACE

#pragma pack(push, 1)

#pragma pack(pop)

RaymarineControl::RaymarineControl(NetworkAddress sendAddress) {
  m_addr.sin_family = AF_INET;
  m_addr.sin_addr = sendAddress.addr;  // Overwritten by actual radar addr
  m_addr.sin_port = sendAddress.port;

  m_radar_socket = INVALID_SOCKET;
  m_name = wxT("Raymarine radar");

  m_pi = 0;
  m_ri = 0;
  m_name = wxT("Raymarine");
}

RaymarineControl::~RaymarineControl() {
  if (m_radar_socket != INVALID_SOCKET) {
    closesocket(m_radar_socket);
    LOG_TRANSMIT(wxT("radar_pi: %s transmit socket closed"), m_name.c_str());
  }
}

bool RaymarineControl::Init(radar_pi *pi, RadarInfo *ri, NetworkAddress &ifadr, NetworkAddress &radaradr) {
  int r;
  int one = 1;

  m_addr.sin_addr = radaradr.addr;
  m_addr.sin_port = radaradr.port;

  m_pi = pi;
  m_ri = ri;
  m_name = ri->m_name;

  if (m_radar_socket != INVALID_SOCKET) {
    closesocket(m_radar_socket);
  }
  m_radar_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (m_radar_socket == INVALID_SOCKET) {
    r = -1;
  } else {
    r = setsockopt(m_radar_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));
  }

  if (!r) {
    struct sockaddr_in s;

    s.sin_addr = ifadr.addr;
    s.sin_port = ifadr.port;
    s.sin_family = AF_INET;
#ifdef __WX_MAC__
    s.sin_len = sizeof(sockaddr_in);
#endif

    r = ::bind(m_radar_socket, (struct sockaddr *)&s, sizeof(s));
  }

  if (r) {
    wxLogError(wxT("radar_pi: Unable to create UDP sending socket"));
    // Might as well give up now
    return false;
  }

  LOG_TRANSMIT(wxT("radar_pi: %s transmit socket open"), m_name);
  return true;
}

void RaymarineControl::logBinaryData(const wxString &what, const void *data, int size) {
  wxString explain;
  const uint8_t *d = (const uint8_t *)data;
  int i = 0;

  explain.Alloc(size * 3 + 50);
  explain += wxT("radar_pi: ") + m_name + wxT(" ");
  explain += what;
  explain += wxString::Format(wxT(" %d bytes: "), size);
  for (i = 0; i < size; i++) {
    explain += wxString::Format(wxT(" %02X"), d[i]);
  }
  LOG_TRANSMIT(explain);
}

bool RaymarineControl::TransmitCmd(const void *msg, int size) {
  if (m_radar_socket == INVALID_SOCKET) {
    wxLogError(wxT("radar_pi: Unable to transmit command to unknown radar"));
    return false;
  }
  if (sendto(m_radar_socket, (char *)msg, size, 0, (struct sockaddr *)&m_addr, sizeof(m_addr)) < size) {
    wxLogError(wxT("radar_pi: Unable to transmit command to %s: %s"), m_name.c_str(), SOCKETERRSTR);
    return false;
  }
  IF_LOG_AT(LOGLEVEL_TRANSMIT, logBinaryData(wxString::Format(wxT("%s transmit"), m_name), msg, size));
  return true;
}

static uint8_t rd_msg_tx_control[] = {
	0x01, 0x80, 0x01, 0x00,
	0x00, // Control value at offset 4 : 0 - off, 1 - on
	0x00, 0x00, 0x00
};

void RaymarineControl::RadarTxOff() {
  IF_LOG_AT(LOGLEVEL_VERBOSE | LOGLEVEL_TRANSMIT, wxLogMessage(wxT("radar_pi: %s transmit: turn off"), m_name));

  rd_msg_tx_control[4] = 0;

  TransmitCmd(rd_msg_tx_control, sizeof(rd_msg_tx_control));
}

void RaymarineControl::RadarTxOn() {
  IF_LOG_AT(LOGLEVEL_VERBOSE | LOGLEVEL_TRANSMIT, wxLogMessage(wxT("radar_pi: %s transmit: turn on"), m_name));

  rd_msg_tx_control[4] = 1;

  TransmitCmd(rd_msg_tx_control, sizeof(rd_msg_tx_control));
}

static uint8_t rd_msg_1s[] = {
	0x00, 0x80, 0x01, 0x00, 0x52, 0x41, 0x44, 0x41, 0x52, 0x00, 0x00, 0x00
};

static uint8_t rd_msg_5s[] = {
	0x03, 0x89, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x68, 0x01, 0x00, 0x00, 0x9e, 0x03, 0x00, 0x00, 0xb4, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

bool RaymarineControl::RadarStayAlive() 
{
  TransmitCmd(rd_msg_1s, sizeof(rd_msg_1s));
  return true;
}

static uint8_t rd_msg_set_range[] = {
	0x01, 0x81, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 
	0x01,	// Range at offset 8 (0 - 1/8, 1 - 1/4, 2 - 1/2, 3 - 3/4, 4 - 1, 5 - 1.5, 6 - 3...)
	0x00, 0x00, 0x00
};

bool RaymarineControl::SetRange(int meters)
{
  for(int i = 0; i < sizeof(raymarine_ranges) / sizeof(raymarine_ranges[0]); i++)
  {
    if(meters <= raymarine_ranges[i])
    {
      rd_msg_set_range[8] = i;
      LOG_VERBOSE(wxT("radar_pi: %s transmit: range %d (%d) meters"), m_name.c_str(), meters, raymarine_ranges[i]);
      return TransmitCmd(rd_msg_set_range, sizeof(rd_msg_set_range));
    }
  }
  return false;
}

static uint8_t rd_msg_mbs_control[] = {
	0x01, 0x82, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, // MBS Enable (1) at offset 16
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t rd_msg_set_display_timing[] = {
	0x02, 0x82, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 
	0x6d, // Display timing value at offset 8
	0x00, 0x00, 0x00
};

static uint8_t rd_msg_set_stc_preset[] = {
	0x03, 0x82, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 
	0x74, // STC preset value at offset 8
	0x00, 0x00, 0x00
};

static uint8_t rd_msg_tune_coarse[] = {
	0x04, 0x82, 0x01, 0x00, 
	0x00, // Coarse tune at offset 4
	0x00, 0x00, 0x00
};

static uint8_t rd_msg_bearing_offset[] = {
	0x07, 0x82, 0x01, 0x00, 
	0x14, 0x00, 0x00, 0x00
};

static uint8_t rd_msg_set_sea[] = {
	0x02, 0x83, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 
	0x00, // Sea value at offset 20
	0x00, 0x00, 0x00
};

static uint8_t rd_msg_sea_auto[] = {
	0x02, 0x83, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, // Sea auto value at offset 16
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t rd_msg_set_gain[] = {
	0x01, 0x83, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, // Gain value at offset 20
	0x00, 0x00, 0x00
};

static uint8_t rd_msg_set_gain_auto[] = {
	0x01, 0x83, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, // Gain auto - 1, manual - 0 at offset 16
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t rd_msg_rain_on[] = {
	0x03, 0x83, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, // Rain on at offset 16 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t rd_msg_rain_set[] = {
	0x03, 0x83, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 
	0x33, // Rain value at offset 20 
	0x00, 0x00, 0x00
};

static uint8_t rd_msg_ftc_on[] = {
	0x04, 0x83, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, // FTC on at offset 16
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t rd_msg_ftc_set[] = {
	0x04, 0x83, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 
	0x1a, // FTC value at offset 20
	0x00, 0x00, 0x00
};

static uint8_t rd_msg_tune_auto[] = {
	0x05, 0x83, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x01, // Enable at offset 12
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t rd_msg_tune_fine[] = {
	0x05, 0x83, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, // Tune value at offset 16
	0x00, 0x00, 0x00
};

static uint8_t rd_msg_target_expansion[] = {
	0x06, 0x83, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 
	0x01,	// Expansion value 0 - disabled, 1 - low, 2 - high 
	0x00, 0x00, 0x00
};

static uint8_t rd_msg_interference_rejection[] = {
	0x07, 0x83, 0x01, 0x00, 
	0x01,	// Interference rejection at offset 4, 0 - off, 1 - normal, 2 - high 
	0x00, 0x00, 0x00
};

static uint8_t curve_values[] = { 0, 1, 2, 4, 6, 8, 10, 13 };
static uint8_t rd_msg_curve_select[] = {
	0x0a, 0x83, 0x01, 0x00, 
	0x01	// Curve value at offset 4
};

void RaymarineControl::SetGain(uint8_t value)
{
	rd_msg_set_gain[20] = value;
	TransmitCmd(rd_msg_set_gain, sizeof(rd_msg_set_gain));
}

void RaymarineControl::SetAutoGain(bool enable)
{
	rd_msg_set_gain_auto[16] = enable ? 1 : 0;
	TransmitCmd(rd_msg_set_gain_auto, sizeof(rd_msg_set_gain_auto));
}

void RaymarineControl::SetTune(uint8_t value)
{
	rd_msg_tune_fine[16] = value;
	TransmitCmd(rd_msg_tune_fine, sizeof(rd_msg_tune_fine));
}

void RaymarineControl::SetAutoTune(bool enable)
{
	rd_msg_tune_auto[12] = enable ? 1 : 0;
	TransmitCmd(rd_msg_tune_auto, sizeof(rd_msg_tune_auto));
}

void RaymarineControl::SetCoarseTune(uint8_t value)
{
	rd_msg_tune_coarse[4] = value;
	TransmitCmd(rd_msg_tune_coarse, sizeof(rd_msg_tune_coarse));
}

void RaymarineControl::TurnOff()
{
  rd_msg_tx_control[4] = 3;
  TransmitCmd(rd_msg_tx_control, sizeof(rd_msg_tx_control));
}

void RaymarineControl::SetSTCPreset(uint8_t value)
{
	rd_msg_set_stc_preset[8] = value;
	TransmitCmd(rd_msg_set_stc_preset, sizeof(rd_msg_set_stc_preset));
}

void RaymarineControl::SetFTC(uint8_t value)
{
	rd_msg_ftc_set[20] = value;
	TransmitCmd(rd_msg_ftc_set, sizeof(rd_msg_ftc_set));
}

void RaymarineControl::SetFTCEnabled(bool enable)
{
	rd_msg_ftc_on[16] = enable ? 1 : 0;
	TransmitCmd(rd_msg_ftc_on, sizeof(rd_msg_ftc_on));
}

void RaymarineControl::SetRain(uint8_t value)
{
	rd_msg_rain_set[20] = value;
	TransmitCmd(rd_msg_rain_set, sizeof(rd_msg_rain_set));
}

void RaymarineControl::SetRainEnabled(bool enable)
{
	rd_msg_rain_on[16] = enable ? 1 : 0;
	TransmitCmd(rd_msg_rain_on, sizeof(rd_msg_rain_on));
}

void RaymarineControl::SetSea(uint8_t value)
{
	rd_msg_set_sea[20] = value;
	TransmitCmd(rd_msg_set_sea, sizeof(rd_msg_set_sea));
}

void RaymarineControl::SetAutoSea(uint8_t value)
{
	rd_msg_sea_auto[16] = value;
	TransmitCmd(rd_msg_sea_auto, sizeof(rd_msg_sea_auto));
}

void RaymarineControl::SetDisplayTiming(uint8_t value)
{
	rd_msg_set_display_timing[8] = value;
	TransmitCmd(rd_msg_set_display_timing, sizeof(rd_msg_set_display_timing));
}

void RaymarineControl::SetBearingOffset(int32_t value)
{
	rd_msg_bearing_offset[4] = value & 0xff;
	rd_msg_bearing_offset[5] = (value >> 8) & 0xff;
	rd_msg_bearing_offset[6] = (value >> 16) & 0xff;
	rd_msg_bearing_offset[7] = (value >> 24) & 0xff;
	TransmitCmd(rd_msg_bearing_offset, sizeof(rd_msg_bearing_offset));
}

void RaymarineControl::SetSeaClutterCurve(uint8_t id)
{
	rd_msg_curve_select[4] = curve_values[id - 1];
	TransmitCmd(rd_msg_curve_select, sizeof(rd_msg_curve_select));
}

void RaymarineControl::EnableMBS(bool enable)
{
	rd_msg_mbs_control[16] = enable ? 1 : 0;
	TransmitCmd(rd_msg_mbs_control, sizeof(rd_msg_mbs_control));
}

bool RaymarineControl::SetInterferenceRejection(uint8_t value)
{
	if(value >= 0 && value <= 2)
	{
		rd_msg_interference_rejection[4] = value;
		TransmitCmd(rd_msg_interference_rejection, sizeof(rd_msg_interference_rejection));
	}
	else return false;
}

bool RaymarineControl::SetTargetExpansion(uint8_t value)
{
	if(value >= 0 && value <= 2)
	{
		rd_msg_target_expansion[8] = value;
		TransmitCmd(rd_msg_target_expansion, sizeof(rd_msg_target_expansion));
	}
	else return false;
}

bool RaymarineControl::SetControlValue(ControlType controlType, RadarControlItem &item) {
  bool r = false;
  int value = item.GetValue();
  RadarControlState state = item.GetState();

  switch (controlType) {
    // The following are settings that are not radar commands. Made them explicit so the
    // compiler can catch missing control types.
    case CT_NONE:
    case CT_RANGE:
    case CT_TIMED_IDLE:
    case CT_TIMED_RUN:
    case CT_TRANSPARENCY:
    case CT_REFRESHRATE:
    case CT_TARGET_TRAILS:
    case CT_TRAILS_MOTION:
    case CT_MAX:
    case CT_ANTENNA_FORWARD:
    case CT_ANTENNA_STARBOARD:
    case CT_ORIENTATION:
    case CT_OVERLAY:
    case CT_TARGET_ON_PPI:

    // The following are settings not supported by Garmin HD.
    case CT_SIDE_LOBE_SUPPRESSION:
    case CT_TARGET_EXPANSION:
    case CT_LOCAL_INTERFERENCE_REJECTION:
    case CT_NOISE_REJECTION:
    case CT_TARGET_SEPARATION:
    case CT_ANTENNA_HEIGHT:
    case CT_NO_TRANSMIT_END:
    case CT_NO_TRANSMIT_START:
    case CT_SCAN_SPEED:

      break;

    // Ordering the radar commands by the first byte value.
    // Some interesting holes here, seems there could be more commands!

    case CT_BEARING_ALIGNMENT: {
      LOG_VERBOSE(wxT("radar_pi: %s Bearing alignment: %d"), m_name.c_str(), value);
      SetBearingOffset(value);
      break;
    }

    case CT_GAIN: {
      LOG_VERBOSE(wxT("radar_pi: %s Gain: value=%d state=%d"), m_name.c_str(), value, (int)state);
      if (state >= RCS_AUTO_1) {
        SetAutoGain(true);
      } else if (state == RCS_MANUAL) {
        SetGain(value);
      }
      break;
    }

    case CT_SEA: {
      LOG_VERBOSE(wxT("radar_pi: %s Sea: value=%d state=%d"), m_name.c_str(), value, (int)state);

      if (state == RCS_AUTO_1) {
        SetAutoSea(1);
      } else if (state == RCS_AUTO_2) {
        SetAutoSea(2);
      } else if (state == RCS_AUTO_3) {
        SetAutoSea(3);
      } else if (state == RCS_OFF) {
         SetAutoSea(0);
         SetSea(0);
      } else if (state == RCS_MANUAL) {
         SetAutoSea(0);
         SetSea(value);
      }
      break;
    }

    case CT_FTC: {
      if(state == RCS_OFF)
      {
        SetFTCEnabled(0);
      }
      else if(state == RCS_MANUAL)
      {
        SetFTCEnabled(1);
        SetFTC(value);
      }
      break;
    }

    case CT_RAIN: {  // Rain Clutter
      LOG_VERBOSE(wxT("radar_pi: %s Rain: value=%d state=%d"), m_name.c_str(), value, (int)state);

      if (state == RCS_OFF) {
        SetRainEnabled(0);
      } else if (state == RCS_MANUAL) {
        SetRainEnabled(1);
        SetRain(value);
      }
      break;
    }

    case CT_INTERFERENCE_REJECTION: {
      LOG_VERBOSE(wxT("radar_pi: %s Interference Rejection / Crosstalk: %d"), m_name.c_str(), value);
      SetInterferenceRejection(value);
      break;
    }

    case CT_TARGET_BOOST: {
      LOG_VERBOSE(wxT("radar_pi: %s Target Boost: %d"), m_name.c_str(), value);
      SetTargetExpansion(value);
      break;
    }

    case CT_MAIN_BANG_SIZE: {
      LOG_VERBOSE(wxT("radar_pi: %s Target Boost: %d"), m_name.c_str(), value);
      EnableMBS(state == RCS_OFF);
      break;
    }
  }

  return r;
}

PLUGIN_END_NAMESPACE
