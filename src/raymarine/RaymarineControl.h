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

#ifndef _RAYMARINE_CONTROL_H_
#define _RAYMARINE_CONTROL_H_

#include "RadarInfo.h"
#include "pi_common.h"
#include "socketutil.h"

PLUGIN_BEGIN_NAMESPACE

extern int raymarine_ranges[11];

class RaymarineControl : public RadarControl {
 public:
  RaymarineControl(NetworkAddress sendMultiCastAddress);
  ~RaymarineControl();

  bool Init(radar_pi *pi, RadarInfo *ri, NetworkAddress &interfaceAddress, NetworkAddress &radarAddress);
  void RadarTxOff();
  void RadarTxOn();
  bool RadarStayAlive();
  bool SetRange(int meters);

  bool SetControlValue(ControlType controlType, RadarControlItem &item);

 private:
  void logBinaryData(const wxString &what, const void *data, int size);
  bool TransmitCmd(const void *msg, int size);

  radar_pi *m_pi;
  RadarInfo *m_ri;
  struct sockaddr_in m_addr;
  SOCKET m_radar_socket;
  wxString m_name;

  void SetGain(uint8_t value);
  void SetAutoGain(bool enable);
  void SetTune(uint8_t value);

  void SetAutoTune(bool enable);
  void SetCoarseTune(uint8_t value);
  void TurnOff();
  void SetSTCPreset(uint8_t value);
  void SetFTC(uint8_t value);
  void SetFTCEnabled(bool enable);
  void SetRain(uint8_t value);
  void SetRainEnabled(bool enable);
  void SetSea(uint8_t value);
  void SetAutoSea(uint8_t value);
  void SetDisplayTiming(uint8_t value);
  void SetBearingOffset(int32_t value);
  void SetSeaClutterCurve(uint8_t id);
  void EnableMBS(bool enable);
  bool SetInterferenceRejection(uint8_t value);
  bool SetTargetExpansion(uint8_t value);
 
};

PLUGIN_END_NAMESPACE

#endif /* _GARMIN_HD_CONTROL_H_ */
