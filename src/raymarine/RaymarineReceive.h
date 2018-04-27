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

#ifndef _RAYMARINE_RECEIVE_H_
#define _RAYMARINE_RECEIVE_H_

#include <exception>
#include "RadarReceive.h"
#include "socketutil.h"

PLUGIN_BEGIN_NAMESPACE

struct value_not_set : public std::exception {
	const char * what () const throw ()
	{
		return "Value not set";
	}
};

struct invalid_control : public std::exception {
	const char * what () const throw ()
	{
		return "Invalid control";
	}
};
	

class CValue {
	bool m_set;
	int m_value;
    public:
	CValue(int value = 0, bool set = false) : m_value(value), m_set(set) { };
	int Get() const { if(!m_set) throw value_not_set(); return m_value; }
	bool IsSet() const { return m_set; }
	void Set(int value) { m_value = value; m_set = true; }
};

class CControlItem {
	CValue m_value;
	CValue m_min, m_max;
	bool m_active; // True if Auto or Off
    public:
	CControlItem(int value = 0, int min = 0, int max = 0, bool active = true, bool set = false) 
		: m_value(value, set), m_min(min, set), m_max(max, set), m_active(active) { };
	int Get() const { return m_value.Get(); }
	bool IsSet() const { return m_value.IsSet(); }
	bool Check(int value) const { return value <= m_max.Get() && value >= m_min.Get(); }
	void Set(int value) { m_value.Set(value); }
	void SetMin(int min) { m_min.Set(min); }
	void SetMax(int max) { m_max.Set(max); }
	bool IsActive() const { if(!m_value.IsSet()) throw value_not_set(); return m_active; }
	bool SetActive(bool active) { m_active = active; }
	const CValue & Min() const { return m_min; }
	const CValue & Max() const { return m_max; }
};

struct SMiscRadarInfo {
	int m_warmupTime;
	int m_signalStrength;
	int m_magnetronCurrent;
	int m_magnetronHours;
	int m_rotationPeriod;
};

class RaymarineReceive : public RadarReceive {
 public:
  RaymarineReceive(radar_pi *pi, RadarInfo *ri, NetworkAddress reportAddr);
  ~RaymarineReceive() {}

  void *Entry(void);
  void Shutdown(void);
  wxString GetInfoStatus();

  NetworkAddress m_interface_addr;
  NetworkAddress m_report_addr;
  NetworkAddress m_dataGroup;
  NetworkAddress m_radarAddr;

  wxLongLong m_shutdown_time_requested;  // Main thread asks this thread to stop
  volatile bool m_is_shutdown;

 private:
	enum CRMType { RM_D, RM_HD };
	CRMType m_radarType;

//  void ProcessFrame(radar_line *packet);
 	void ProcessFrame(const UINT8 *data, int len);
	bool ProcessReport(const UINT8 *data, int len);

	void ProcessScanData(const UINT8 *data, int len);

	// RM...D
	void ProcessFeedback(const UINT8 *data, int len);
	void ProcessPresetFeedback(const UINT8 *data, int len);
	void ProcessCurveFeedback(const UINT8 *data, int len);


  SOCKET PickNextEthernetCard();
  SOCKET GetNewReportSocket();
  SOCKET GetNewDataSocket(const NetworkAddress & dataGroup);

  wxString m_ip;

//  SOCKET m_receive_socket;  // Where we listen for message from m_send_socket
//  SOCKET m_send_socket;     // A message to this socket will interrupt select() and allow immediate shutdown

  struct ifaddrs *m_interface_array;
  struct ifaddrs *m_interface;

  int m_next_spoke;
  int m_radar_status;
  bool m_first_receive;

  wxString m_addr;  // Radar's IP address
  SOCKET m_receive_socket;  // Where we listen for message from m_send_socket
  SOCKET m_send_socket;     // A message to this socket will interrupt select() and allow immediate shutdown

  wxCriticalSection m_lock;  // Protects m_status
  wxString m_status;         // Userfriendly string

#if 0
  bool m_auto_gain;               // True if auto gain mode is on
  int m_gain;                     // 0..100
  RadarControlState m_sea_mode;   // RCS_OFF, RCS_MANUAL, RCS_AUTO_1
  int m_sea_clutter;              // 0..100
  RadarControlState m_rain_mode;  // RCS_OFF, RCS_MANUAL, RCS_AUTO_1
  int m_rain_clutter;             // 0..100
#endif
  bool m_no_transmit_zone_mode;   // True if there is a zone
  int m_no_spoke_timeout;

  bool UpdateScannerStatus(int status);
  void SetInfoStatus(wxString status);

	CControlItem m_gain;
	CControlItem m_stc;
	CControlItem m_rain;
	CControlItem m_sea;
	CControlItem m_autoSea;
	CControlItem m_ftc;
	CControlItem m_interferenceRejection;
	CControlItem m_targetBoost;
	CControlItem m_bearingOffset;
	CControlItem m_tuneFine;
	CControlItem m_tuneCoarse;
	CControlItem m_displayTiming;
	CControlItem m_stcCurve;
	CControlItem m_mbsEnabled;
  SMiscRadarInfo m_miscInfo;

	int m_range_meters;    // Last received range in meters
  bool m_updated_range;  // m_range_meters has changed
  bool m_haveRadar;
};

PLUGIN_END_NAMESPACE

#endif /* _RAYMARINE_RECEIVE_H_ */
