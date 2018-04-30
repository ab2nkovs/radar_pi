/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Radar Plugin
 * Author:   David Register
 *           Dave Cowell
 *           Kees Verruijt
 *           Hakan Svensson
 *           Douwe Fokkema
 *           Sean D'Epagnier
 *           RM Guy
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

#include "RaymarineReceive.h"

PLUGIN_BEGIN_NAMESPACE

/*
 * This file not only contains the radar receive threads, it is also
 * the only unit that understands what the radar returned data looks like.
 * The rest of the plugin uses a (slightly) abstract definition of the radar.
 */
RaymarineReceive::RaymarineReceive(radar_pi *pi, RadarInfo *ri, NetworkAddress reportAddr) 
  : RadarReceive(pi, ri)
  , m_report_addr(reportAddr)
  , m_next_spoke(-1)
  , m_radar_status(-1)
  , m_shutdown_time_requested(0)
  , m_is_shutdown(false)
  , m_first_receive(true)
	, m_range_meters(0)
	, m_updated_range(false)
  , m_haveRadar(false)
{
  m_interface_addr = m_pi->GetRadarInterfaceAddress(ri->m_radar);
  m_receive_socket = GetLocalhostServerTCPSocket();
  m_send_socket = GetLocalhostSendTCPSocket(m_receive_socket);
  SetInfoStatus(wxString::Format(wxT("%s: %s"), m_ri->m_name.c_str(), _("Initializing")));
  m_ri->m_showManualValueInAuto = true;

  LOG_RECEIVE(wxT("radar_pi: %s receive thread created"), m_ri->m_name.c_str());

  // Set constant min/max control values
  m_stc.SetMin(0);
  m_tuneFine.SetMin(0); m_tuneFine.SetMax(255);
  m_tuneCoarse.SetMin(0); m_tuneCoarse.SetMax(255);
  m_bearingOffset.SetMin(-1800); m_bearingOffset.SetMax(1795);
  m_autoSea.SetMin(0); m_autoSea.SetMax(3);
  m_interferenceRejection.SetMin(0); m_interferenceRejection.SetMax(2);
  m_targetBoost.SetMin(0); m_targetBoost.SetMax(2);
  m_displayTiming.SetMin(0); m_displayTiming.SetMax(255);
  m_mbsEnabled.SetMin(0); m_mbsEnabled.SetMax(1);
  m_stcCurve.SetMin(1); m_stcCurve.SetMax(8);
}

#define MILLIS_PER_SELECT 250
#define SECONDS_SELECT(x) ((x)*MILLISECONDS_PER_SECOND / MILLIS_PER_SELECT)


SOCKET RaymarineReceive::PickNextEthernetCard() {
  SOCKET socket = INVALID_SOCKET;
  CLEAR_STRUCT(m_interface_addr);

  // Pick the next ethernet card
  // If set, we used this one last time. Go to the next card.
  if (m_interface) {
    m_interface = m_interface->ifa_next;
  }
  // Loop until card with a valid IPv4 address
  while (m_interface && !VALID_IPV4_ADDRESS(m_interface)) {
    m_interface = m_interface->ifa_next;
  }
  if (!m_interface) {
    if (m_interface_array) {
      freeifaddrs(m_interface_array);
      m_interface_array = 0;
    }
    if (!getifaddrs(&m_interface_array)) {
      m_interface = m_interface_array;
    }
    // Loop until card with a valid IPv4 address
    while (m_interface && !VALID_IPV4_ADDRESS(m_interface)) {
      m_interface = m_interface->ifa_next;
    }
  }
  if (m_interface && VALID_IPV4_ADDRESS(m_interface)) {
    m_interface_addr.addr = ((struct sockaddr_in *)m_interface->ifa_addr)->sin_addr;
    m_interface_addr.port = 0;
  }

  socket = GetNewReportSocket();

  return socket;
}

SOCKET RaymarineReceive::GetNewReportSocket() {
  SOCKET socket;
  wxString error;

  if (m_interface_addr.addr.s_addr == 0) {
    return INVALID_SOCKET;
  }

  error = wxT("");
  socket = startUDPMulticastReceiveSocket(m_interface_addr, m_report_addr, error);
  if (socket != INVALID_SOCKET) {
    wxString addr = FormatNetworkAddress(m_interface_addr);
    wxString rep_addr = FormatNetworkAddressPort(m_report_addr);

    LOG_RECEIVE(wxT("radar_pi: %s scanning interface %s for data from %s"), m_ri->m_name.c_str(), addr.c_str(), rep_addr.c_str());

    wxString s;
    s << _("Scanning interface") << wxT(" ") << addr;
    SetInfoStatus(s);
  } else {
    SetInfoStatus(error);
    wxLogError(wxT("radar_pi: Unable to listen to socket: %s"), error.c_str());
  }
  return socket;
}

SOCKET RaymarineReceive::GetNewDataSocket(const NetworkAddress & dataGroup)
{
	SOCKET socket;
	wxString error;

	if (m_interface_addr.addr.s_addr == 0)
	{
		return INVALID_SOCKET;
	}

	// char *mcast_ip = inet_ntoa(m_radar_mcast.sin_addr);
	// fprintf(stderr, "RMRadar_pi: creating socket for %s:%d", mcast_ip, ntohs(m_radar_mcast.sin_port));
	socket = startUDPMulticastReceiveSocket(m_interface_addr, dataGroup, error);
	if (socket != INVALID_SOCKET)
	{
    wxString addr = FormatNetworkAddress(m_interface_addr);
    wxString data_addr = FormatNetworkAddressPort(dataGroup);
	  LOG_RECEIVE(wxT("radar_pi: %s listening for data on interface %s multicast group %s"), m_ri->m_name.c_str(), addr.c_str(), data_addr.c_str());
    wxString s;
    s << _("Listening to ")  << data_addr << _(" on interface") << wxT(" ") << addr;
    SetInfoStatus(s);
	} 
	else 
	{
    SetInfoStatus(error);
		wxLogError(wxT("radar_pi: Unable to listen to socket: %s"), error.c_str());
	}
	return socket;
}

/*
 * Entry
 *
 * Called by wxThread when the new thread is running.
 * It should remain running until Shutdown is called.
 */
void *RaymarineReceive::Entry(void) 
{
  int r = 0;
  int no_data_timeout = 0;
  union {
    sockaddr_storage addr;
    sockaddr_in ipv4;
  } rx_addr;
  socklen_t rx_len;

  uint8_t data[2048];
  m_interface_array = 0;
  m_interface = 0;
  m_no_spoke_timeout = 0;
  struct sockaddr_in radarFoundAddr;
  sockaddr_in *radar_addr = 0;

  SOCKET reportSocket = INVALID_SOCKET;
  SOCKET dataSocket = INVALID_SOCKET;

  LOG_VERBOSE(wxT("radar_pi: RaymarineReceive thread %s starting"), m_ri->m_name.c_str());

  if (m_interface_addr.addr.s_addr == 0) {
    reportSocket = GetNewReportSocket();
  }

  while (m_receive_socket != INVALID_SOCKET) {
    if (reportSocket == INVALID_SOCKET) {
      reportSocket = PickNextEthernetCard();
      if (reportSocket != INVALID_SOCKET) {
        no_data_timeout = 0;
        m_no_spoke_timeout = 0;
      }
    }

    struct timeval tv = {(long)0, (long)(MILLIS_PER_SELECT * 1000)};

    fd_set fdin;
    FD_ZERO(&fdin);

    int maxFd = INVALID_SOCKET;
    if (m_receive_socket != INVALID_SOCKET) {
      FD_SET(m_receive_socket, &fdin);
      maxFd = MAX(m_receive_socket, maxFd);
    }
    if (reportSocket != INVALID_SOCKET) {
      FD_SET(reportSocket, &fdin);
      maxFd = MAX(reportSocket, maxFd);
    }
    if (dataSocket != INVALID_SOCKET) {
      FD_SET(dataSocket, &fdin);
      maxFd = MAX(dataSocket, maxFd);
    }

    wxLongLong start = wxGetUTCTimeMillis();
    r = select(maxFd + 1, &fdin, 0, 0, &tv);
    // LOG_RECEIVE(wxT("radar_pi: select maxFd=%d r=%d elapsed=%lld"), maxFd, r, wxGetUTCTimeMillis() - start);

    if (r > 0) {
      if (m_receive_socket != INVALID_SOCKET && FD_ISSET(m_receive_socket, &fdin)) {
        rx_len = sizeof(rx_addr);
        r = recvfrom(m_receive_socket, (char *)data, sizeof(data), 0, (struct sockaddr *)&rx_addr, &rx_len);
        if (r > 0) {
          LOG_VERBOSE(wxT("radar_pi: %s received stop instruction"), m_ri->m_name.c_str());
          break;
        }
      }

			if (dataSocket != INVALID_SOCKET && FD_ISSET(dataSocket, &fdin)) 
			{
				rx_len = sizeof(rx_addr);
				r = recvfrom(dataSocket, (char *)data, sizeof(data), 0, (struct sockaddr *)&rx_addr, &rx_len);
				if (r > 0) 
				{
					ProcessFrame(data, r);
          no_data_timeout = SECONDS_SELECT(-5);
				} 
				else 
				{
					closesocket(dataSocket);
					dataSocket = INVALID_SOCKET;
					LOG_INFO(wxT("radar_pi: %s data socket error"), m_ri->m_name.c_str());
				}
			}

      if (reportSocket != INVALID_SOCKET && FD_ISSET(reportSocket, &fdin)) {
        rx_len = sizeof(rx_addr);
        r = recvfrom(reportSocket, (char *)data, sizeof(data), 0, (struct sockaddr *)&rx_addr, &rx_len);

        if (r > 0) {
          NetworkAddress radar_address;
          radar_address.addr = rx_addr.ipv4.sin_addr;
          radar_address.port = rx_addr.ipv4.sin_port;

          if (ProcessReport(data, r)) {
            if (!radar_addr) {
              wxCriticalSectionLocker lock(m_lock);
              m_ri->DetectedRadar(m_interface_addr, m_radarAddr);  // enables transmit data

              // the dataSocket is opened in the next loop

              radarFoundAddr = rx_addr.ipv4;
              radar_addr = &radarFoundAddr;
              m_addr = FormatNetworkAddress(radar_address);

              if (m_ri->m_state.GetValue() == RADAR_OFF) {
                LOG_INFO(wxT("radar_pi: %s detected at %s"), m_ri->m_name.c_str(), m_addr.c_str());
                m_ri->m_state.Update(RADAR_STANDBY);
              }

              if(dataSocket == INVALID_SOCKET)
              {
                dataSocket = GetNewDataSocket(m_dataGroup);
              }
            	no_data_timeout = SECONDS_SELECT(-5);
            }
						else
						{
							// Make sure data socket is working
            	no_data_timeout++;
						}
          }
        } else {
          wxLogError(wxT("radar_pi: %s illegal report"), m_ri->m_name.c_str());
          closesocket(reportSocket);
          reportSocket = INVALID_SOCKET;
        }
      }

    } else {  // no data received -> select timeout

      if (no_data_timeout >= SECONDS_SELECT(2)) {
        no_data_timeout = 0;
        if(dataSocket != INVALID_SOCKET)
        {
          closesocket(dataSocket);
          dataSocket = INVALID_SOCKET;
					m_haveRadar = false;
        }
        if (reportSocket != INVALID_SOCKET) {
          closesocket(reportSocket);
          reportSocket = INVALID_SOCKET;
          m_ri->m_state.Update(RADAR_OFF);
          CLEAR_STRUCT(m_interface_addr);
          radar_addr = 0;
        }
      } else {
        no_data_timeout++;
      }

      if (m_no_spoke_timeout >= SECONDS_SELECT(2)) {
        m_no_spoke_timeout = 0;
        m_ri->ResetRadarImage();
      } else {
        m_no_spoke_timeout++;
      }
    }

  }  // endless loop until thread destroy

  if (reportSocket != INVALID_SOCKET) {
    closesocket(reportSocket);
  }
  if (m_send_socket != INVALID_SOCKET) {
    closesocket(m_send_socket);
    m_send_socket = INVALID_SOCKET;
  }
  if (m_receive_socket != INVALID_SOCKET) {
    closesocket(m_receive_socket);
  }

  if (m_interface_array) {
    freeifaddrs(m_interface_array);
  }

#ifdef TEST_THREAD_RACES
  LOG_VERBOSE(wxT("radar_pi: %s receive thread sleeping"), m_ri->m_name.c_str());
  wxMilliSleep(1000);
#endif
  LOG_VERBOSE(wxT("radar_pi: %s receive thread stopping"), m_ri->m_name.c_str());
  m_is_shutdown = true;
  return 0;
}

/*
 RADAR REPORTS

 The radars send various reports.

 */

bool RaymarineReceive::UpdateScannerStatus(int status) 
{
  bool ret = true;

  if (status != m_radar_status) {
    m_radar_status = status;

    wxString stat;
    time_t now = time(0);

    switch (m_radar_status) {
      case 0:
        m_ri->m_state.Update(RADAR_STANDBY);
        LOG_VERBOSE(wxT("radar_pi: %s reports status STANDBY"), m_ri->m_name.c_str());
        stat = _("Standby");
        break;
      case 1:
        m_ri->m_state.Update(RADAR_TRANSMIT);
        LOG_VERBOSE(wxT("radar_pi: %s reports status TRANSMIT"), m_ri->m_name.c_str());
        stat = _("Transmit");
        break;
      case 2:
        m_ri->m_state.Update(RADAR_WARMING_UP);
        LOG_VERBOSE(wxT("radar_pi: %s reports status WARMUP"), m_ri->m_name.c_str());
        stat = _("Warmup");
        break;
      case 3:
        m_ri->m_state.Update(RADAR_OFF);
        m_ri->m_data_timeout = now + DATA_TIMEOUT;
        LOG_VERBOSE(wxT("radar_pi: %s reports status OFF"), m_ri->m_name.c_str());
        stat = _("Turning Off");
        break;
      default:
        stat << _("Unknown status") << wxString::Format(wxT(" %d"), m_radar_status);
        ret = false;
        break;
    }
    SetInfoStatus(wxString::Format(wxT("IP %s %s"), m_addr.c_str(), stat.c_str()));
  }
  return ret;
}

void RaymarineReceive::ProcessFrame(const uint8_t *data, int len) 
{
	// wxLongLong nowMillis = wxGetLocalTimeMillis();
	time_t now = time(0);
	m_ri->m_radar_timeout = now + WATCHDOG_TIMEOUT;

	int spoke = 0;
	m_ri->m_statistics.packets++;

	if(len >= 4)
	{
		uint32_t msgId = 0;
		memcpy(&msgId, data, sizeof(msgId));
		switch(msgId)
		{
		case 0x00010001:
			ProcessFeedback(data, len);
			break;
		case 0x00010002:
			ProcessPresetFeedback(data, len);
			break;
		case 0x00010003:
			ProcessScanData(data, len);
			m_ri->m_data_timeout = now + DATA_TIMEOUT;
      m_ri->m_radar_timeout = now + WATCHDOG_TIMEOUT;
      m_no_spoke_timeout = -5;
			break;
		case 0x00010005:
			ProcessCurveFeedback(data, len);
			break;
		case 0x00018801:
#if 0      
			ProcessHDFeedback(data, len);
			break;
#endif      
		case 0x00010006:
		case 0x00010007:
		case 0x00010008:
		case 0x00010009:
		case 0x00018942:
			break;	
		default:
			// fprintf(stderr, "Unknown message ID %08X.\n", (int)msgId);
			break;
		}
	}
}

#pragma pack(push, 1)

struct SRadarFeedback {
	uint32_t type;	// 0x010001
	uint32_t range_values[11];
	uint32_t something_1[33]; 
	uint8_t status;		// 2 - warmup, 1 - transmit, 0 - standby, 6 - shutting down (warmup time - countdown), 3 - shutdown
	uint8_t something_2[3];
	uint8_t warmup_time;
	uint8_t signal_strength;	// number of bars
	uint8_t something_3[7];
	uint8_t range_id;
	uint8_t something_4[2];
	uint8_t auto_gain;
	uint8_t something_5[3];
	uint32_t gain;
	uint8_t auto_sea; // 0 - disabled; 1 - harbour, 2 - offshore, 3 - coastal 
	uint8_t something_6[3];
	uint8_t sea_value;
	uint8_t rain_enabled;
	uint8_t something_7[3];
	uint8_t rain_value;
	uint8_t ftc_enabled;
	uint8_t something_8[3];
	uint8_t ftc_value;
	uint8_t auto_tune;
	uint8_t something_9[3];
	uint8_t tune;
	int16_t bearing_offset;	// degrees * 10; left - negative, right - positive
	uint8_t interference_rejection;
	uint8_t something_10[3];
	uint8_t target_expansion;
	uint8_t something_11[13];
	uint8_t mbs_enabled;	// Main Bang Suppression enabled if 1
};

struct SRadarPresetFeedback {
	uint32_t type;	// 0x010002
	uint8_t something_1[213]; // 221 - magnetron current; 233, 234 - rotation time ms (251 total)
	uint16_t magnetron_hours;
	uint8_t something_2[6];
	uint8_t magnetron_current;
	uint8_t something_3[11];
	uint16_t rotation_time;
	uint8_t something_4[13];
	uint8_t stc_preset_max;
	uint8_t something_5[2];
	uint8_t coarse_tune_arr[3];
	uint8_t fine_tune_arr[3]; // 0, 1, 2 - fine tune value for SP, MP, LP
	uint8_t something_6[6];	  
	uint8_t display_timing_value;
	uint8_t something_7[12];
	uint8_t stc_preset_value;
	uint8_t something_8[12];
	uint8_t min_gain;
	uint8_t max_gain;
	uint8_t min_sea;
	uint8_t max_sea;
	uint8_t min_rain;
	uint8_t max_rain;
	uint8_t min_ftc;
	uint8_t max_ftc;
	uint8_t gain_value;
	uint8_t sea_value;
	uint8_t fine_tune_value;
	uint8_t coarse_tune_value;
	uint8_t signal_strength_value;
	uint8_t something_9[2];
};

struct SCurveFeedback {
	uint32_t type;	// 0x010005
	uint8_t curve_value;
};

struct SHDFeedback {
	uint32_t type;
	uint32_t range_values[11];
	uint32_t something_1[19];
	uint32_t status;
	uint32_t something_2[2];
	uint32_t warmup_time;
	uint32_t something_3[3];
	uint32_t dual_range;
	uint32_t something_4[2];
	uint32_t auto_tune;
	uint32_t tune;
	uint32_t something_5[31];
	uint32_t range_id;
	uint32_t auto_color_gain;
	uint32_t color_gain_value;
	uint32_t auto_gain;
	uint32_t gain_value;
	uint32_t auto_sea;
	uint32_t sea_value;
	uint32_t rain_enabled;
	uint32_t rain_value;
	uint32_t auto_rain_1;
	uint32_t rain_1;
	uint32_t something_6[1];
	uint32_t mode;
	uint32_t something_7[15];
};

#pragma pack(pop)

static uint8_t radar_signature_id[4] = { 1, 0, 0, 0 };
static char *radar_signature = (char *)"Ethernet Dome";
struct SRMRadarFunc {
	uint32_t type;
	uint32_t dev_id;
	uint32_t func_id;	// 1
	uint32_t something_1;
	uint32_t something_2;
	uint32_t mcast_ip;
	uint32_t mcast_port;
	uint32_t radar_ip;
	uint32_t radar_port;
};	

bool RaymarineReceive::ProcessReport(const UINT8 *report, int len)
{
	LOG_BINARY_RECEIVE(wxT("ProcessReport"), report, len);

	if(len == sizeof(SRMRadarFunc))
	{
		SRMRadarFunc *rRec = (SRMRadarFunc *)report;
		if(rRec->func_id == 1)
		{
			if(!m_haveRadar)
			{
				m_dataGroup.addr.s_addr = ntohl(rRec->mcast_ip);
				m_dataGroup.port = htons(rRec->mcast_port);

				m_radarAddr.addr.s_addr = ntohl(rRec->radar_ip);
				m_radarAddr.port = ntohs(rRec->radar_port);
				m_haveRadar = true;
#if 0
				if(m_pi->m_settings.enable_transmit)
				{
					fprintf(stderr, "Sending initial messages to %d.%d.%d.%d:%d.\n", 
						(rRec->radar_ip >> 24) & 0xff, (rRec->radar_ip >> 16) & 0xff,
						(rRec->radar_ip >> 8) & 0xff, rRec->radar_ip & 0xff, rRec->radar_port);
				}
				else
				{
					fprintf(stderr, "Transmit not enabled.\n");
				}
#endif        
			}
			return true;
		}
	}

	if (m_pi->m_settings.verbose >= 2) {
		LOG_BINARY_RECEIVE(wxT("received unknown message"), report, len);
	}
	return false;
}

int raymarine_ranges[] = { 1852 / 4, 1852 / 2, 1852, 1852 * 3 / 2, 1852 * 3, 1852 * 6, 1852 * 12, 1852 * 24, 1852 * 48, 1852 * 96, 1852 * 144 };
static int current_ranges[11] = { 125, 250, 500, 750, 1500, 3000, 6000, 12000, 24000, 48000, 72000 };
static enum RadarControlState auto_values[] = {
  RCS_MANUAL,  RCS_AUTO_1,  RCS_AUTO_2,  RCS_AUTO_3,  RCS_AUTO_4,  RCS_AUTO_5,  RCS_AUTO_6,
  RCS_AUTO_7,  RCS_AUTO_8,  RCS_AUTO_9
};

void RaymarineReceive::ProcessFeedback(const UINT8 *data, int len)
{
	if(len == sizeof(SRadarFeedback))
	{
		SRadarFeedback *fbPtr = (SRadarFeedback *)data;
		if(fbPtr->type == 0x010001)
		{
      UpdateScannerStatus(fbPtr->status);

			if(fbPtr->range_values[0] != current_ranges[0]) // Units must have changed
			{
				for(int i = 0; i < sizeof(current_ranges) / sizeof(current_ranges[0]); i++)
				{
					current_ranges[i] = fbPtr->range_values[i];
					raymarine_ranges[i] = 1852 * fbPtr->range_values[i] / 500;

					// fprintf(stderr, "%d (%d)\n", current_ranges[i], raymarine_ranges[i]);
				}
			}
			if(raymarine_ranges[fbPtr->range_id] != m_range_meters) {
				if (m_pi->m_settings.verbose >= 1)
				{
				  LOG_VERBOSE(wxT("RMRadar_pi: %s now scanning with range %d meters (was %d meters)"), m_ri->m_name.c_str(),
					  raymarine_ranges[fbPtr->range_id],
					  m_range_meters);
				}
				m_range_meters = raymarine_ranges[fbPtr->range_id];
				m_updated_range = true;
				m_ri->m_range.Update(m_range_meters / 2); // RM MFD shows half of what is received
			}

			m_gain.Set(fbPtr->gain);
			m_gain.SetActive(fbPtr->auto_gain == 0);
      m_ri->m_gain.Update(fbPtr->gain, fbPtr->auto_gain == 0 ? RCS_MANUAL : RCS_AUTO_1);

			m_sea.Set(fbPtr->sea_value);
			m_sea.SetActive(fbPtr->auto_sea == 0);
			m_autoSea.Set(fbPtr->auto_sea);
			m_autoSea.SetActive(fbPtr->auto_sea != 0);
			m_ri->m_sea.Update(fbPtr->sea_value, auto_values[fbPtr->auto_sea]);

			m_rain.SetActive(fbPtr->rain_enabled == 1);
			m_rain.Set(fbPtr->rain_value);
      m_ri->m_rain.Update(fbPtr->rain_value, fbPtr->rain_enabled == 1 ? RCS_MANUAL : RCS_OFF );

			m_targetBoost.Set(fbPtr->target_expansion);
			m_interferenceRejection.Set(fbPtr->interference_rejection);

			int ba = (int)fbPtr->bearing_offset;
			m_bearingOffset.Set(ba);
      m_ri->m_bearing_alignment.Update((int32_t)ba);

			m_tuneFine.SetActive(fbPtr->auto_tune == 0);
			m_tuneFine.Set(fbPtr->tune);
			m_tuneCoarse.SetActive(fbPtr->auto_tune == 0);

			m_mbsEnabled.Set(fbPtr->mbs_enabled);

			m_miscInfo.m_warmupTime = fbPtr->warmup_time;
			m_miscInfo.m_signalStrength = fbPtr->signal_strength;

			m_ftc.SetActive(fbPtr->ftc_enabled == 1);
			m_ftc.Set(fbPtr->ftc_value);
		}
	}
}

void RaymarineReceive::ProcessPresetFeedback(const UINT8 *data, int len)
{
	if(len == sizeof(SRadarPresetFeedback))
	{
		SRadarPresetFeedback *fbPtr = (SRadarPresetFeedback *)data;

		m_tuneCoarse.Set(fbPtr->coarse_tune_value);
		m_stc.Set(fbPtr->stc_preset_value);
		m_displayTiming.Set(fbPtr->display_timing_value);
		m_stc.SetMax(fbPtr->stc_preset_max);
		m_gain.SetMin(fbPtr->min_gain); m_gain.SetMax(fbPtr->max_gain);
		m_sea.SetMin(fbPtr->min_sea); m_sea.SetMax(fbPtr->max_sea);
		m_rain.SetMin(fbPtr->min_rain); m_rain.SetMax(fbPtr->max_rain);
		m_ftc.SetMin(fbPtr->min_ftc); m_ftc.SetMax(fbPtr->max_ftc);

		m_miscInfo.m_signalStrength = fbPtr->signal_strength_value;
		m_miscInfo.m_magnetronCurrent = fbPtr->magnetron_current;
		m_miscInfo.m_magnetronHours = fbPtr->magnetron_hours;
		m_miscInfo.m_rotationPeriod = fbPtr->rotation_time;

	}
}

void RaymarineReceive::ProcessCurveFeedback(const UINT8 *data, int len)
{
	if(len == sizeof(SCurveFeedback))
	{
		SCurveFeedback *fbPtr = (SCurveFeedback *)data;
		switch(fbPtr->curve_value)
		{
		case 0:
			m_stcCurve.Set(1);
			break;
		case 1:
			m_stcCurve.Set(2);
			break;
		case 2:
			m_stcCurve.Set(3);
			break;
		case 4:
			m_stcCurve.Set(4);
			break;
		case 6:
			m_stcCurve.Set(5);
			break;
		case 8:
			m_stcCurve.Set(6);
			break;
		case 10:
			m_stcCurve.Set(7);
			break;
		case 13:
			m_stcCurve.Set(8);
			break;
		default:
			fprintf(stderr, "ProcessCurveFeedback: unknown curve value %d.\n", (int)fbPtr->curve_value);
		}
	}
	else
	{
		fprintf(stderr, "ProcessCurveFeedback: got %d bytes, expected %d.\n", len, (int)sizeof(SCurveFeedback));
	}
}

// Radar data

struct CRMPacketHeader {
    uint32_t type;		// 0x00010003
    uint32_t zero_1;
    uint32_t something_1;	// 0x0000001c
    uint32_t nspokes;		// 0x00000008 - usually but changes
    uint32_t spoke_count;	// 0x00000000 in regular, counting in HD
    uint32_t zero_3;
    uint32_t something_3;	// 0x00000001
    uint32_t something_4;	// 0x00000000 or 0xffffffff in regular, 0x400 in HD
};

struct CRMRecordHeader {
    uint32_t type;
    uint32_t length;
    // ...
};

struct CRMScanHeader {
    uint32_t type;		// 0x00000001
    uint32_t length;		// 0x00000028
    uint32_t azimuth;
    uint32_t something_2;	// 0x00000001 - 0x03 - HD
    uint32_t something_3;	// 0x00000002
    uint32_t something_4;	// 0x00000001 - 0x03 - HD
    uint32_t something_5;	// 0x00000001 - 0x00 - HD
    uint32_t something_6;	// 0x000001f4 - 0x00 - HD
    uint32_t zero_1;
    uint32_t something_7;	// 0x00000001
};

struct CRMOptHeader {		// No idea what is in there
    uint32_t type;		// 0x00000002
    uint32_t length;		// 0x0000001c
    uint32_t zero_2[5];
};

struct CRMScanData {
    uint32_t type;		// 0x00000003
    uint32_t length;
    uint32_t data_len;
    // unsigned char data[rec_len - 8];
};

//
//
#define SCALE_RAW_TO_DEGREES(raw) ((raw) * (double)DEGREES_PER_ROTATION / RAYMARINE_SPOKES)
#define SCALE_DEGREES_TO_RAW(angle) ((int)((angle) * (double)RAYMARINE_SPOKES / DEGREES_PER_ROTATION))

void RaymarineReceive::ProcessScanData(const UINT8 *data, int len)
{
	if(len > sizeof(CRMPacketHeader) + sizeof(CRMScanHeader))
	{
		CRMPacketHeader *pHeader = (CRMPacketHeader *)data;
		if(pHeader->type != 0x00010003 || pHeader->something_1 != 0x0000001c || 
			pHeader->something_3 != 0x0000001)
		{
			fprintf(stderr, "ProcessScanData::Packet header mismatch %x, %x, %x, %x.\n", pHeader->type, pHeader->something_1, 
				pHeader->nspokes, pHeader->something_3);
			return;
		}

		m_ri->m_state.Update(RADAR_TRANSMIT);

		if(pHeader->something_4 == 0x400)
		{
			if(m_radarType != RM_HD)
			{
				m_radarType = RM_HD;
			}
		}
		else
		{
			if(m_radarType != RM_D)
			{
				m_radarType = RM_D;
			}
		}

		// wxLongLong nowMillis = wxGetLocalTimeMillis();
    wxLongLong time_rec = wxGetUTCTimeMillis();
		int headerIdx = 0;
		int nextOffset = sizeof(CRMPacketHeader);


		while(nextOffset < len)
		{
			CRMScanHeader *sHeader = (CRMScanHeader *)(data + nextOffset);
			if(sHeader->type != 0x00000001 || sHeader->length != 0x00000028)
			{
				fprintf(stderr, "ProcessScanData::Scan header #%d (%d) - %x, %x.\n", headerIdx, nextOffset, sHeader->type, sHeader->length);
				break;
			}
		
			if(sHeader->something_2 != 0x00000001 || sHeader->something_3 != 0x00000002 || sHeader->something_4 != 0x00000001 ||
    				sHeader->something_5 != 0x00000001 || sHeader->something_6 != 0x000001f4 || sHeader->something_7 != 0x00000001)
			{
				if(sHeader->something_2 != 3 || sHeader->something_3 != 2 || sHeader->something_4 != 3 ||
					sHeader->something_5 != 0 || sHeader->something_6 != 0 || sHeader->something_7 != 1)
				{
					fprintf(stderr, "ProcessScanData::Scan header #%d part 2 check failed.\n", headerIdx);
					break;
				}
				else if(m_radarType != RM_HD)
				{
					m_radarType = RM_HD;
					fprintf(stderr, "ProcessScanData::Scan header #%d HD second header with regular first.\n", headerIdx);
				}				
				
			}
			else if(m_radarType != RM_D)
			{
				m_radarType = RM_D;
				fprintf(stderr, "ProcessScanData::Scan header #%d regular second header with HD first.\n", headerIdx);
			}

			nextOffset += sizeof(CRMScanHeader);

			CRMRecordHeader *nHeader = (CRMRecordHeader *)(data + nextOffset);
			if(nHeader->type == 0x00000002)
			{
				if(nHeader->length != 0x0000001c)
				{
					// fprintf(stderr, "ProcessScanData::Opt header #%d part 2 check failed.\n", headerIdx);
				}
				nextOffset += nHeader->length;
			}

			CRMScanData *pSData = (CRMScanData *)(data + nextOffset);
			
			if((pSData->type & 0x7fffffff) != 0x00000003 || pSData->length < pSData->data_len + 8)
			{
				fprintf(stderr, "ProcessScanData::Scan data header #%d check failed %x, %d, %d.\n", headerIdx, 
					pSData->type, pSData->length, pSData->data_len);
				break;
			}

			UINT8 unpacked_data[RAYMARINE_MAX_SPOKE_LEN], *dataPtr = 0;
			if(m_radarType == RM_D)
			{
				uint8_t *dData = (uint8_t *)unpacked_data;
				uint8_t *sData = (uint8_t *)data + nextOffset + sizeof(CRMScanData);

				int iS = 0;
				int iD = 0;
				while(iS < pSData->data_len)
				{
					if(*sData != 0x5c)
					{
						*dData++ = ((*sData) & 0x03) * 85;
						*dData++ = (((*sData) & 0x0c) >> 2) * 85;
						*dData++ = (((*sData) & 0x30) >> 4) * 85;
						*dData++ = (((*sData) & 0xc0) >> 6) * 85;
						sData++;
						iS++; iD += 4;
					}
					else
					{					
						uint8_t nFill = sData[1];
						uint8_t cFill = sData[2];
					
						for(int i = 0; i < nFill; i++)
						{
							*dData++ = (cFill & 0x03) * 85;
							*dData++ = ((cFill & 0x0c) >> 2) * 85;
							*dData++ = ((cFill & 0x30) >> 4) * 85;
							*dData++ = ((cFill & 0xc0) >> 6) * 85;
						}
						sData += 3;
						iS += 3;
						iD += nFill * 4;
					}
	  
				}
				if(iD != RAYMARINE_MAX_SPOKE_LEN)
				{
					while(iS < pSData->length - 8 && iD < RAYMARINE_MAX_SPOKE_LEN)
					{
						*dData++ = ((*sData) & 0x03) * 85;
						*dData++ = (((*sData) & 0x0c) >> 2) * 85;
						*dData++ = (((*sData) & 0x30) >> 4) * 85;
						*dData++ = (((*sData) & 0xc0) >> 6) * 85;
						sData++;
						iS++; iD += 4;
					}
				}
				if(iD != RAYMARINE_MAX_SPOKE_LEN)
				{
					// fprintf(stderr, "ProcessScanData::Packet %d line %d (%d/%x) not complete %d.\n", packetIdx, headerIdx,
					// 	scan_idx, scan_idx, iD);
				}
				dataPtr = unpacked_data;
			}
			else if(m_radarType == RM_HD)
			{
				if(pSData->data_len != RAYMARINE_MAX_SPOKE_LEN)
				{
					m_ri->m_statistics.broken_spokes++;
					fprintf(stderr, "ProcessScanData data len %d should be %d.\n", pSData->data_len, RAYMARINE_MAX_SPOKE_LEN);
					break;
				}
				// if(m_range_meters == 0) m_range_meters = 1852 / 4; // !!!TEMP delete!!!
#if 0
				dataPtr = (UINT8 *)data + nextOffset + sizeof(CRMScanData);
#else
				uint8_t *dData = (uint8_t *)unpacked_data;
				uint8_t *sData = (uint8_t *)data + nextOffset + sizeof(CRMScanData);
        memcpy(dData, sData, pSData->data_len);
#if 0
        // For when 1024 points is too many
				for(int i = 0; i < pSData->data_len / 2; i++)
				{
					*dData++ = (sData[0] + sData[1]) / 2;
					sData += 2; 
				}
#endif
				dataPtr = unpacked_data;
#endif
			}
			else
			{
				fprintf(stderr, "ProcessScanData::Packet radar type is not set somehow.\n");
				break;
			}

			nextOffset += pSData->length;
			m_ri->m_statistics.spokes++;
			unsigned int spoke = sHeader->azimuth;
			if (m_next_spoke >= 0 && spoke != m_next_spoke) {
				if (spoke > m_next_spoke) {
					m_ri->m_statistics.missing_spokes += spoke - m_next_spoke;
				} else {
					m_ri->m_statistics.missing_spokes += RAYMARINE_SPOKES + spoke - m_next_spoke;
				}
			}
			m_next_spoke = (spoke + 1) % RAYMARINE_SPOKES;

			if((pSData->type & 0x80000000) != 0 && nextOffset < len)
			{
				// fprintf(stderr, "ProcessScanData::Last record %d (%d) in packet %d but still data to go %d:%d.\n",
				// 	headerIdx, scan_idx, packetIdx, nextOffset, len); 
			}
				
			headerIdx++;
			
			int angle_raw = (spoke + RAYMARINE_SPOKES / 2) % RAYMARINE_SPOKES;

      short int heading_raw = 0;
      int bearing_raw;

      heading_raw = SCALE_DEGREES_TO_RAW(m_pi->GetHeadingTrue());  // include variation
      bearing_raw = angle_raw + heading_raw;

      SpokeBearing a = MOD_SPOKES(angle_raw);
      SpokeBearing b = MOD_SPOKES(bearing_raw);

			m_ri->ProcessRadarSpoke(a, b, dataPtr, RAYMARINE_MAX_SPOKE_LEN, m_range_meters, time_rec);
		}
	}
}

// Called from the main thread to stop this thread.
// We send a simple one byte message to the thread so that it awakens from the select() call with
// this message ready for it to be read on 'm_receive_socket'. See the constructor in RaymarineReceive.h
// for the setup of these two sockets.

void RaymarineReceive::Shutdown() {
  if (m_send_socket != INVALID_SOCKET) {
    m_shutdown_time_requested = wxGetUTCTimeMillis();
    if (send(m_send_socket, "!", 1, MSG_DONTROUTE) > 0) {
      LOG_VERBOSE(wxT("radar_pi: %s requested receive thread to stop"), m_ri->m_name.c_str());
      return;
    }
  }
  LOG_INFO(wxT("radar_pi: %s receive thread will take long time to stop"), m_ri->m_name.c_str());
}

wxString RaymarineReceive::GetInfoStatus()
{
  wxCriticalSectionLocker lock(m_lock);
  // Called on the UI thread, so be gentle

  return m_status;
}

void RaymarineReceive::SetInfoStatus(wxString status)
{
  wxCriticalSectionLocker lock(m_lock);
  m_status = status;
}


PLUGIN_END_NAMESPACE
