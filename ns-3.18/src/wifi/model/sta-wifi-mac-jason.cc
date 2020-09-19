/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2006, 2009 INRIA
 * Copyright (c) 2009 MIRKO BANCHI
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 * Author: Mirko Banchi <mk.banchi@gmail.com>
 */

#include <limits>

#include "sta-wifi-mac-jason.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/boolean.h"
#include "ns3/trace-source-accessor.h"

#include "qos-tag.h"
#include "mac-low.h"
#include "dcf-manager.h"
#include "mac-rx-middle.h"
#include "mac-tx-middle.h"
#include "wifi-mac-header.h"
#include "msdu-aggregator.h"
#include "amsdu-subframe-header.h"
#include "mgt-headers.h"

NS_LOG_COMPONENT_DEFINE ("StaWifiMacJason");


/*
 * The state machine for this STA is:
 --------------                                          -----------
 | Associated |   <--------------------      ------->    | Refused |
 --------------                        \    /            -----------
    \                                   \  /
     \    -----------------     -----------------------------
      \-> | Beacon Missed | --> | Wait Association Response |
          -----------------     -----------------------------
                \                       ^
                 \                      |
                  \    -----------------------
                   \-> | Wait Probe Response |
                       -----------------------
 */

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (StaWifiMacJason);

TypeId
StaWifiMacJason::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::StaWifiMacJason")
    .SetParent<RegularWifiMac> ()
    .AddConstructor<StaWifiMacJason> ()
    .AddAttribute ("ProbeRequestTimeout", "The interval between two consecutive probe request attempts.",
                   TimeValue (Seconds (0.05)),
                   MakeTimeAccessor (&StaWifiMacJason::m_probeRequestTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("AssocRequestTimeout", "The interval between two consecutive assoc request attempts.",
                   TimeValue (Seconds (0.5)),
                   MakeTimeAccessor (&StaWifiMacJason::m_assocRequestTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("MaxMissedBeacons",
                   "Number of beacons which much be consecutively missed before "
                   "we attempt to restart association.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&StaWifiMacJason::m_maxMissedBeacons),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("ActiveProbing", "If true, we send probe requests. If false, we don't. NOTE: if more than one STA in your simulation is using active probing, you should enable it at a different simulation time for each STA, otherwise all the STAs will start sending probes at the same time resulting in collisions. See bug 1060 for more info.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&StaWifiMacJason::SetActiveProbing),
                   MakeBooleanChecker ())
    .AddTraceSource ("Assoc", "Associated with an access point.",
                     MakeTraceSourceAccessor (&StaWifiMacJason::m_assocLogger))
    .AddTraceSource ("DeAssoc", "Association with an access point lost.",
                     MakeTraceSourceAccessor (&StaWifiMacJason::m_deAssocLogger))
  ;
  return tid;
}

StaWifiMacJason::StaWifiMacJason ()
  : m_state (BEACON_MISSED),
    m_probeRequestEvent (),
    m_assocRequestEvent (),
    m_beaconWatchdogEnd (Seconds (0.0))
{
  NS_LOG_FUNCTION (this);

  // Let the lower layers know that we are acting as a non-AP STA in
  // an infrastructure BSS.
  SetTypeOfStation (STA);
  
  otherInterface = NULL;
  lastRate = 0;
  lastRx = 0;
  lastDelay = std::numeric_limits<int>::max();
  lastDistance = std::numeric_limits<int>::max();
  lastUtility = 0;
  
  //std::cerr << " JASON WIFI MAC" << std::endl;
}

StaWifiMacJason::~StaWifiMacJason ()
{
  NS_LOG_FUNCTION (this);
}

void
StaWifiMacJason::SetMaxMissedBeacons (uint32_t missed)
{
  NS_LOG_FUNCTION (this << missed);
  m_maxMissedBeacons = missed;
}

void
StaWifiMacJason::SetProbeRequestTimeout (Time timeout)
{
  NS_LOG_FUNCTION (this << timeout);
  m_probeRequestTimeout = timeout;
}

void
StaWifiMacJason::SetAssocRequestTimeout (Time timeout)
{
  NS_LOG_FUNCTION (this << timeout);
  m_assocRequestTimeout = timeout;
}

void
StaWifiMacJason::StartActiveAssociation (void)
{
  NS_LOG_FUNCTION (this);
  TryToEnsureAssociated ();
}

void
StaWifiMacJason::SetActiveProbing (bool enable)
{
  NS_LOG_FUNCTION (this << enable);
  if (enable)
    {
      Simulator::ScheduleNow (&StaWifiMacJason::TryToEnsureAssociated, this);
    }
  else
    {
      m_probeRequestEvent.Cancel ();
    }
}

void
StaWifiMacJason::SendProbeRequest (void)
{
  //std::cout << "PROBE REQUEST at time: " << Simulator::Now() << std::endl;
  NS_LOG_FUNCTION (this);
  WifiMacHeader hdr;
  hdr.SetProbeReq ();
  hdr.SetAddr1 (Mac48Address::GetBroadcast ());
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (Mac48Address::GetBroadcast ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  Ptr<Packet> packet = Create<Packet> ();
  MgtProbeRequestHeader probe;
  probe.SetSsid (GetSsid ());
  probe.SetSupportedRates (GetSupportedRates ());
  packet->AddHeader (probe);

  // The standard is not clear on the correct queue for management
  // frames if we are a QoS AP. The approach taken here is to always
  // use the DCF for these regardless of whether we have a QoS
  // association or not.
  m_dca->Queue (packet, hdr);

  m_probeRequestEvent = Simulator::Schedule (m_probeRequestTimeout,
                                             &StaWifiMacJason::ProbeRequestTimeout, this);
  /* added by jason
   * - reset the list of APs we have
   * - record the time when the probe was sent
   */
  //accessPoints.clear();
  m_probe_start = Simulator::Now();
}

void
StaWifiMacJason::SendAssociationRequest (void)
{
  NS_LOG_FUNCTION (this << GetBssid ());
  WifiMacHeader hdr;
  hdr.SetAssocReq ();
  hdr.SetAddr1 (GetBssid ());
  hdr.SetAddr2 (GetAddress ());
  hdr.SetAddr3 (GetBssid ());
  hdr.SetDsNotFrom ();
  hdr.SetDsNotTo ();
  Ptr<Packet> packet = Create<Packet> ();
  MgtAssocRequestHeader assoc;
  assoc.SetSsid (GetSsid ());
  assoc.SetSupportedRates (GetSupportedRates ());
  packet->AddHeader (assoc);

  // The standard is not clear on the correct queue for management
  // frames if we are a QoS AP. The approach taken here is to always
  // use the DCF for these regardless of whether we have a QoS
  // association or not.
  m_dca->Queue (packet, hdr);

  m_assocRequestEvent = Simulator::Schedule (m_assocRequestTimeout,
                                             &StaWifiMacJason::AssocRequestTimeout, this);
}

void
StaWifiMacJason::TryToEnsureAssociated (void)
{
  NS_LOG_FUNCTION (this);
  switch (m_state)
    {
    case ASSOCIATED:
      return;
      break;
    case WAIT_PROBE_RESP:
      /* we have sent a probe request earlier so we
         do not need to re-send a probe request immediately.
         We just need to wait until probe-request-timeout
         or until we get a probe response
       */
      break;
    case BEACON_MISSED:
      /* we were associated but we missed a bunch of beacons
       * so we should assume we are not associated anymore.
       * We try to initiate a probe request now.
       */
      //std::cout << "HEREHERHEHRE" << std::endl;
            
      m_linkDown ();
      SetState (WAIT_PROBE_RESP);
      SendProbeRequest ();
      break;
    case WAIT_ASSOC_RESP:
      /* we have sent an assoc request so we do not need to
         re-send an assoc request right now. We just need to
         wait until either assoc-request-timeout or until
         we get an assoc response.
       */
      break;
    case REFUSED:
      /* we have sent an assoc request and received a negative
         assoc resp. We wait until someone restarts an
         association with a given ssid.
       */
      break;
    }
}

void
StaWifiMacJason::AssocRequestTimeout (void)
{
  NS_LOG_FUNCTION (this);
  SetState (WAIT_ASSOC_RESP);
  SendAssociationRequest ();
}

int StaWifiMacJason::GetIndexBestRate(void)
{
	uint64_t max_rate = 0;
	double previousBest = otherInterface->GetLastRate();
	int max_index = -1; 
	
	//loop through all AP results and determine the best AP
	for(unsigned int i = 0; i < accessPoints.size(); i++)
	{
		
		SupportedRates rates = accessPoints.at(i).getSupportedRates();
		
		//loop through all the supported rates from the received APs
		for(unsigned int j = 0; j < rates.GetNRates(); j++)
		{
			WifiMode mode = m_phy->GetMode (j);
			if (rates.IsSupportedRate (mode.GetDataRate ()))
			{
				//see if we have a new best
				if((mode.GetDataRate() > max_rate) && (mode.GetDataRate() > previousBest))
				{
					//if(mode.GetDataRate() > previousBest)
						//std::cout << " beat previous best " << std::endl;
					max_rate = mode.GetDataRate();
					max_index = i;
					
					lastRate = max_rate;
				}
				else
				{
					//if(mode.GetDataRate() < previousBest)
						//std::cout << " didnt beat previous best" << std::endl;
				}
			}
		}
	}
	
	return max_index;
}

int StaWifiMacJason::GetIndexLowestDelay(void)
{
	int min_index = -1;
	double min_delay = std::numeric_limits<int>::max();
	double previousBest = otherInterface->GetLastDelay();
	
	if(accessPoints.size() > 0)
	{
		//loop through all AP results and determine the best AP
		for(unsigned int i = 0; i < accessPoints.size(); i++)
		{
			if((accessPoints.at(i).getDelay().GetDouble() < min_delay) && (accessPoints.at(i).getDelay().GetDouble() < previousBest))
			{
				min_delay = accessPoints.at(i).getDelay().GetDouble();
				min_index = i;
				
				lastDelay = min_delay;
			}
		}
	}
	else
	{
		std::cout << "ERROR, RECEIVED NO RESPONSES" << std::endl;
		exit(0);
	}
	return min_index;
}

int StaWifiMacJason::GetIndexBestUtility(void)
{
	int best_index = -1;
	double best_utility = 0;
	double previousBest = otherInterface->GetLastUtility();	
	
	if(accessPoints.size() > 0)
	{
		//loop through all AP results and determine the best AP
		for(unsigned int i = 0; i < accessPoints.size(); i++)
		{
			double max_rate = 0;
			SupportedRates rates = accessPoints.at(i).getSupportedRates();
			//loop through all the supported rates from the received APs
			for(unsigned int j = 0; j < rates.GetNRates(); j++)
			{
				WifiMode mode = m_phy->GetMode (j);
				if (rates.IsSupportedRate (mode.GetDataRate ()))
				{
					if(mode.GetDataRate() > max_rate)
						max_rate = mode.GetDataRate();
				}
			}
			
			double delay = accessPoints.at(i).getDelay().GetDouble();
			
			//get mobility model for this node
			Vector pos = mob->GetPosition ();
			//std::cout << "POS: x=" << pos.x << ", y=" << pos.y << std::endl;
			
			double distance = 0;
			
			//get the mobility model for the AP
			for(unsigned int j = 0; j < aps.GetN(); j++)
			{
				if(aps.Get(j)->GetAddress() == accessPoints.at(i).getBssid())
				{
					Ptr<NetDevice> d1 = aps.Get(j);
					Vector pos2 = d1->GetNode()->GetObject<MobilityModel>()->GetPosition();
					
					distance = sqrt( ((pos2.x - pos.x) * (pos2.x - pos.x)) + ((pos2.y - pos.y) * (pos2.y - pos.y)) );					
					break;
				}
			}
			
			//determine rate score (between 0 and 1)
			if(lastRate < otherInterface->GetLastRate())
				lastRate = otherInterface->GetLastRate();
				
			double rate_score = 1.0 / (1.0 + exp(-1.0 * (max_rate - lastRate)));
			//std::cout << "RATE: " << max_rate << " LASTRATE: " << lastRate << std::endl;
			
			//determine delay score (between 0 and 1)
			if(lastDelay > otherInterface->GetLastDelay())
				lastDelay = otherInterface->GetLastDelay();
				
			double delay_score = 1.0 / (1.0 + exp(-1.0 * (lastDelay - delay)));
			//std::cout << "DELAY: " << delay << " LASTDELAY: " << lastDelay << std::endl;
			
			//determine distance score (between 0 and 1)
			if(lastDistance > otherInterface->GetLastDistance())
				lastDistance = otherInterface->GetLastDistance();
				
			double distance_score = 1.0 / (1.0 + exp(-1 * (lastDistance - distance)));
			//std::cout << "DISTANCE: " << distance << " LASTDISTANCE: " << lastDistance << std::endl;
			
			//determine overall utility with equal weight
			double utility = ((1.0/3.0) * rate_score) + ((1.0/3.0) * delay_score) + ((1.0/3.0) * distance_score);
			//std::cout << "RATE SCORE: " << rate_score << " DELAY SCORE: " << delay_score << " DISTANCE SCORE: " << distance_score << " UTILITY: " << utility << std::endl;
			if((utility > 1) || (utility < 0))
			{
				std::cout << "ERROR UTILITY OUT OF BOUNDS!!!!" << std::endl;
				exit(0);
			}
			
			if((utility > best_utility) && (utility > previousBest))
			{
				best_index = i;
				best_utility = utility;
				lastUtility = utility;
				lastRate = max_rate;
				lastDelay = delay;
				lastDistance = distance;
			}
		}
	}
	else
	{
		std::cout << "ERROR, RECEIVED NO RESPONSES" << std::endl;
		exit(0);
	}
	
	return best_index;
}

/*
 * Returns the index of the AP which is closest to the current node
 * (which will be used to assume as the best RSS
 */
int StaWifiMacJason::GetIndexClosestDistance(void)
{
	int best_index = -1;
	double closest_distance = std::numeric_limits<int>::max();
	double previousBest = otherInterface->GetLastDistance();
	
	if(accessPoints.size() > 0)
	{
		//loop through all AP results and determine the best AP
		for(unsigned int i = 0; i < accessPoints.size(); i++)
		{
			//get mobility model for this node
			Vector pos = mob->GetPosition ();
			//std::cout << "POS: x=" << pos.x << ", y=" << pos.y << std::endl;
			
			double distance = 0;
			
			//get the mobility model for the AP
			for(unsigned int j = 0; j < aps.GetN(); j++)
			{
				if(aps.Get(j)->GetAddress() == accessPoints.at(i).getBssid())
				{
					Ptr<NetDevice> d1 = aps.Get(j);
					Vector pos2 = d1->GetNode()->GetObject<MobilityModel>()->GetPosition();
					
					distance = sqrt( ((pos2.x - pos.x) * (pos2.x - pos.x)) + ((pos2.y - pos.y) * (pos2.y - pos.y)) );					
					break;
				}
			}
			
			if((distance < closest_distance) && (distance < previousBest))
			{
				best_index = i;
				closest_distance = distance;
				lastDistance = distance;
				
				//std::cout << "  distance: " << distance << std::endl;
			}
		}
	}
	else
	{
		std::cout << "ERROR, RECEIVED NO RESPONSES" << std::endl;
		exit(0);
	}
	
	return best_index;
}

int StaWifiMacJason::GetIndexHighestRxPower(void)
{
	int best_index = -1;
	double highest_rx = 0;
	double previousBest = otherInterface->GetLastRx();
	
	if(accessPoints.size() > 0)
	{
		//loop through all AP results and determine the best AP
		for(unsigned int i = 0; i < accessPoints.size(); i++)
		{			
			double rx = 0;
			
			//get the mobility model for the AP
			for(unsigned int j = 0; j < aps.GetN(); j++)
			{
				if(aps.Get(j)->GetAddress() == accessPoints.at(i).getBssid())
				{
					Ptr<NetDevice> d1 = aps.Get(j);
					Ptr<MobilityModel> mob2 = d1->GetNode()->GetObject<MobilityModel>();
					Ptr<WifiNetDevice> w1 = DynamicCast<WifiNetDevice>(d1);					
					Ptr<WifiChannel> c1 = w1->GetPhy()->GetChannel();
					Ptr<YansWifiChannel> y1 = DynamicCast<YansWifiChannel>(c1);
					Ptr<PropagationLossModel> l1 = y1->GetObject<PropagationLossModel>();	//this is private and we cant get it :(
					
					if(l1 == NULL)
						std::cout << "NULL" << std::endl;
					else
						std::cout << "HERE" << std::endl;
					
					std::cout << "RX: " << l1->CalcRxPower(30, mob, mob2) << std::endl;
					
					rx = 0;
					break;
				}
			}
			
			if((rx > highest_rx) && (rx > previousBest))
			{
				best_index = i;
				highest_rx = rx;
				lastRx = rx;
			}
		}
	}
	return best_index;
}

/*
 * Updated by Jason - here is where we actually make the choice to associate to AP
 */
void
StaWifiMacJason::ProbeRequestTimeout (void)
{
	if(accessPoints.size() > 0)
	{
		int best_index = -1;
		
		best_index = GetIndexBestRate();
		//best_index = GetIndexLowestDelay();
		//best_index = GetIndexClosestDistance();
		//best_index = GetIndexBestUtility();
		//best_index = GetIndexHighestRxPower();		//doesnt work yet
		
		//std::cout << "utility " << lastUtility << std::endl;
		
		if(best_index != -1)
		{
			//std::cout << "associating: " << accessPoints.at(max_index).getBssid() << std::endl;
			
			SetBssid (accessPoints.at(best_index).getBssid());
			//disconnect other interface
			if(otherInterface->IsAssociated() || otherInterface->IsWaitAssocResp())
			{
				//std::cout << "disassociating other interface" << std::endl;
				otherInterface->SetState (REFUSED);
			}
			
			//associate our interface
			SetState (WAIT_ASSOC_RESP);
			SendAssociationRequest ();
		}
		else
		{
			//std::cout << "Didn't find a better choice for AP" << std::endl;
		}
	}
	else
	{
		//std::cout << "ERROR, RECEIVED NO RESPONSES" << std::endl;
	
		// default behaviour
		NS_LOG_FUNCTION (this);
		SetState (WAIT_PROBE_RESP);
		SendProbeRequest ();
	}
}

/*
 * Added by Jason
 */
void StaWifiMacJason::SetOtherInterface(Ptr<StaWifiMacJason>otherInterface)
{
	this->otherInterface = otherInterface;
}
void StaWifiMacJason::SetMobilityModel(Ptr<MobilityModel> mob)
{
	this->mob = mob;
}
void StaWifiMacJason::SetAPs(NetDeviceContainer aps)
{
	this->aps = aps;
}

void
StaWifiMacJason::MissedBeacons (void)
{
  NS_LOG_FUNCTION (this);
  if (m_beaconWatchdogEnd > Simulator::Now ())
    {
      m_beaconWatchdog = Simulator::Schedule (m_beaconWatchdogEnd - Simulator::Now (),
                                              &StaWifiMacJason::MissedBeacons, this);
      return;
    }
  NS_LOG_DEBUG ("beacon missed");
  SetState (BEACON_MISSED);
  TryToEnsureAssociated ();
}

void
StaWifiMacJason::RestartBeaconWatchdog (Time delay)
{
  NS_LOG_FUNCTION (this << delay);
  m_beaconWatchdogEnd = std::max (Simulator::Now () + delay, m_beaconWatchdogEnd);
  if (Simulator::GetDelayLeft (m_beaconWatchdog) < delay
      && m_beaconWatchdog.IsExpired ())
    {
      NS_LOG_DEBUG ("really restart watchdog.");
      m_beaconWatchdog = Simulator::Schedule (delay, &StaWifiMacJason::MissedBeacons, this);
    }
}

bool
StaWifiMacJason::IsAssociated (void) const
{
  return m_state == ASSOCIATED;
}

bool
StaWifiMacJason::IsWaitAssocResp (void) const
{
  return m_state == WAIT_ASSOC_RESP;
}

void
StaWifiMacJason::Enqueue (Ptr<const Packet> packet, Mac48Address to)
{
  NS_LOG_FUNCTION (this << packet << to);
  if (!IsAssociated ())
    {
      NotifyTxDrop (packet);
      TryToEnsureAssociated ();
      return;
    }
  WifiMacHeader hdr;

  // If we are not a QoS AP then we definitely want to use AC_BE to
  // transmit the packet. A TID of zero will map to AC_BE (through \c
  // QosUtilsMapTidToAc()), so we use that as our default here.
  uint8_t tid = 0;

  // For now, an AP that supports QoS does not support non-QoS
  // associations, and vice versa. In future the AP model should
  // support simultaneously associated QoS and non-QoS STAs, at which
  // point there will need to be per-association QoS state maintained
  // by the association state machine, and consulted here.
  if (m_qosSupported)
    {
      hdr.SetType (WIFI_MAC_QOSDATA);
      hdr.SetQosAckPolicy (WifiMacHeader::NORMAL_ACK);
      hdr.SetQosNoEosp ();
      hdr.SetQosNoAmsdu ();
      // Transmission of multiple frames in the same TXOP is not
      // supported for now
      hdr.SetQosTxopLimit (0);

      // Fill in the QoS control field in the MAC header
      tid = QosUtilsGetTidForPacket (packet);
      // Any value greater than 7 is invalid and likely indicates that
      // the packet had no QoS tag, so we revert to zero, which'll
      // mean that AC_BE is used.
      if (tid >= 7)
        {
          tid = 0;
        }
      hdr.SetQosTid (tid);
    }
  else
    {
      hdr.SetTypeData ();
    }

  hdr.SetAddr1 (GetBssid ());
  hdr.SetAddr2 (m_low->GetAddress ());
  hdr.SetAddr3 (to);
  hdr.SetDsNotFrom ();
  hdr.SetDsTo ();

  if (m_qosSupported)
    {
      // Sanity check that the TID is valid
      NS_ASSERT (tid < 8);
      m_edca[QosUtilsMapTidToAc (tid)]->Queue (packet, hdr);
    }
  else
    {
      m_dca->Queue (packet, hdr);
    }
}

void
StaWifiMacJason::Receive (Ptr<Packet> packet, const WifiMacHeader *hdr)
{
  NS_LOG_FUNCTION (this << packet << hdr);
  NS_ASSERT (!hdr->IsCtl ());
  if (hdr->GetAddr3 () == GetAddress ())
    {
      NS_LOG_LOGIC ("packet sent by us.");
      return;
    }
  else if (hdr->GetAddr1 () != GetAddress ()
           && !hdr->GetAddr1 ().IsGroup ())
    {
      NS_LOG_LOGIC ("packet is not for us");
      NotifyRxDrop (packet);
      return;
    }
  else if (hdr->IsData ())
    {
      if (!IsAssociated ())
        {
          NS_LOG_LOGIC ("Received data frame while not associated: ignore");
          NotifyRxDrop (packet);
          return;
        }
      if (!(hdr->IsFromDs () && !hdr->IsToDs ()))
        {
          NS_LOG_LOGIC ("Received data frame not from the DS: ignore");
          NotifyRxDrop (packet);
          return;
        }
      if (hdr->GetAddr2 () != GetBssid ())
        {
          NS_LOG_LOGIC ("Received data frame not from the BSS we are associated with: ignore");
          NotifyRxDrop (packet);
          return;
        }

      if (hdr->IsQosData ())
        {
          if (hdr->IsQosAmsdu ())
            {
              NS_ASSERT (hdr->GetAddr3 () == GetBssid ());
              DeaggregateAmsduAndForward (packet, hdr);
              packet = 0;
            }
          else
            {
              ForwardUp (packet, hdr->GetAddr3 (), hdr->GetAddr1 ());
            }
        }
      else
        {
          ForwardUp (packet, hdr->GetAddr3 (), hdr->GetAddr1 ());
        }
      return;
    }
  else if (hdr->IsProbeReq ()
           || hdr->IsAssocReq ())
    {
      // This is a frame aimed at an AP, so we can safely ignore it.
      NotifyRxDrop (packet);
      return;
    }
  else if (hdr->IsBeacon ())
    {
      MgtBeaconHeader beacon;
      packet->RemoveHeader (beacon);
      bool goodBeacon = false;
      if (GetSsid ().IsBroadcast ()
          || beacon.GetSsid ().IsEqual (GetSsid ()))
        {
          goodBeacon = true;
        }
      if ((IsWaitAssocResp () || IsAssociated ()) && hdr->GetAddr3 () != GetBssid ())
        {
          goodBeacon = false;
        }
      if (goodBeacon)
        {
		  //std::cout << "HERE, MSTATE:" << m_state << " AP MAC: " << hdr->GetAddr3 () << " MY MAC: " << GetAddress () << std::endl;
          Time delay = MicroSeconds (beacon.GetBeaconIntervalUs () * m_maxMissedBeacons);
          RestartBeaconWatchdog (delay);
          SetBssid (hdr->GetAddr3 ());
        }
      if (goodBeacon && m_state == BEACON_MISSED)
        {
		  //std::cout << "NOT ASSOCIATED AND GOT A BEACON FROM: " << std::endl;
		  //std::cout << hdr->GetAddr3() << std::endl;
          SetState (WAIT_ASSOC_RESP);
          SendAssociationRequest ();
        }
      return;
    }
  else if (hdr->IsProbeResp ())
    {
      if (m_state == WAIT_PROBE_RESP)
        {
          MgtProbeResponseHeader probeResp;
          packet->RemoveHeader (probeResp);
          if (!probeResp.GetSsid ().IsEqual (GetSsid ()))
            {
              //not a probe resp for our ssid.
              return;
            }
          
          //figure out if this is from closest AP, if so associate (except when we want to use our approach
          //(assume closest will have highest rss
          
          
          //std::cout << "RECV PROB RESP FROM: " << hdr->GetAddr3() << " TIME: " << Simulator::Now() << std::endl;
          
          /* Added by Jason - AP selection stuff */          
          AccessPoint newAP = AccessPoint(hdr->GetAddr3(), Simulator::Now() - m_probe_start, probeResp.GetSupportedRates(), probeResp.GetBeaconIntervalUs ());
          accessPoints.push_back(newAP);
        }
      return;
    }
  else if (hdr->IsAssocResp ())
    {
      if (m_state == WAIT_ASSOC_RESP)
        {
          MgtAssocResponseHeader assocResp;
          packet->RemoveHeader (assocResp);
          if (m_assocRequestEvent.IsRunning ())
            {
              m_assocRequestEvent.Cancel ();
            }
          if (assocResp.GetStatusCode ().IsSuccess ())
            {
              SetState (ASSOCIATED);
              NS_LOG_DEBUG ("assoc completed");
              SupportedRates rates = assocResp.GetSupportedRates ();
              for (uint32_t i = 0; i < m_phy->GetNModes (); i++)
                {
                  WifiMode mode = m_phy->GetMode (i);
                  if (rates.IsSupportedRate (mode.GetDataRate ()))
                    {
                      m_stationManager->AddSupportedMode (hdr->GetAddr2 (), mode);
                      if (rates.IsBasicRate (mode.GetDataRate ()))
                        {
                          m_stationManager->AddBasicMode (mode);
                        }
                    }
                }
              if (!m_linkUp.IsNull ())
                {
                  m_linkUp ();
                }
            }
          else
            {
              NS_LOG_DEBUG ("assoc refused");
              SetState (REFUSED);
            }
        }
      return;
    }

  // Invoke the receive handler of our parent class to deal with any
  // other frames. Specifically, this will handle Block Ack-related
  // Management Action frames.
  RegularWifiMac::Receive (packet, hdr);
}

SupportedRates
StaWifiMacJason::GetSupportedRates (void) const
{
  SupportedRates rates;
  for (uint32_t i = 0; i < m_phy->GetNModes (); i++)
    {
      WifiMode mode = m_phy->GetMode (i);
      rates.AddSupportedRate (mode.GetDataRate ());
    }
  return rates;
}

void
StaWifiMacJason::SetState (MacState value)
{
  if (value == ASSOCIATED
      && m_state != ASSOCIATED)
    {
      m_assocLogger (GetBssid ());
    }
  else if (value != ASSOCIATED
           && m_state == ASSOCIATED)
    {
		//std::cout << "DEASSOC: " << value << std::endl;
      m_deAssocLogger (GetBssid ());
    }
  m_state = value;
}

/* Added by Jason to support 2 channel MAC */
double StaWifiMacJason::GetLastRate(void)
{
	return lastRate;
}
double StaWifiMacJason::GetLastDelay(void)
{
	return lastDelay;
}
double StaWifiMacJason::GetLastDistance(void)
{
	return lastDistance;
}
double StaWifiMacJason::GetLastUtility(void)
{
	return lastUtility;
}

double StaWifiMacJason::GetLastRx(void)
{
	return lastRx;
}

AccessPoint::AccessPoint(Mac48Address bssid, Time delay, SupportedRates rates, uint64_t beaconIntervalUs)
{
	this->bssid = bssid;
	this->delay = delay;
	this->rates = rates;
	this->beaconIntervalUs = beaconIntervalUs;
}

Mac48Address AccessPoint::getBssid(void)
{
	return this->bssid;
}

Time AccessPoint::getDelay(void)
{
	return this->delay;
}

SupportedRates AccessPoint::getSupportedRates(void)
{
	return this->rates;	
}

uint64_t AccessPoint::GetBeaconIntervalUs(void)
{
	return this->beaconIntervalUs;
}

} // namespace ns3

