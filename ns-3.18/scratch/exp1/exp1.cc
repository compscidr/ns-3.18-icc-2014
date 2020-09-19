/*
 * Jason Ernst, University of Guelph
 * This experiment uses two clients and one GW and compares the
 * throughput, delay and jitter when one client or two are using the AP
 *
 * video source is at GW (assumes the clients are streaming from the
 * Internet
 * 
 * (used to establish the effect of interference with one AP)
 * - assumes everything operates on the same channel
 */

#include <iomanip>		//required to set precision of iosteam

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;
using namespace std;

#define MAX_VIDEO_PACKETS	100000

int main(int argc, char * argv[])
{
	//default simulation parameters
	double max_x = 30;
	double max_y = 30;
	double total_time = 60.0;
	int num_clients = 10;
	int runs = 1;
	
	//parse command line args
	CommandLine cmd;
	cmd.AddValue ("num_clients", "number of client nodes", num_clients);
	cmd.AddValue ("max_x", "width of the random area for client placement", max_x);
	cmd.AddValue ("max_y", "height of the random area for client placement", max_y);
	cmd.AddValue ("total_time", "total running time (s) for the simulation", total_time);
	cmd.AddValue ("runs", "number of repeats to run with different seeds", runs);
	cmd.Parse (argc,argv);
	
	//prepare output file for reporting results
	string filename = "";
	ostringstream convert;
	
	convert << "clients-" << num_clients << "-max_x-" << max_x << "-max_y-" << max_y << "-total_time-" << total_time << "-runs-" << runs;	
	convert << "tr.txt";
	filename = convert.str();
	
	ofstream output;
	output.open(filename.c_str(), ios::out | ios::trunc);
	if(!output.good())
	{
		cerr << "ERROR: could not open '" << filename << "' for output." << endl;
		exit(1);
	}
	
	//prepare for each repetition of the experiment with the same
	//conditions but different random seed
	int run;
	for(run = 0; run < runs; run++)
	{
		NodeContainer clients;
		NodeContainer aps;
		NodeContainer gws;
		NodeContainer csmaNodes;
		
		//set the random seed for reproducable experiments
		RngSeedManager::SetRun(run);
		
		//create nodes
		clients.Create(num_clients);
		aps.Create(1);
		gws.Create(1);
		csmaNodes = NodeContainer(gws,aps);
		
		//set mobility models for the nodes
		//clients are static, but randomly distributed within the simulation area
		MobilityHelper client_mobility;
		client_mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
		Ptr <RandomBoxPositionAllocator> randomBox = CreateObject<RandomBoxPositionAllocator> ();
		Ptr <RandomVariableStream> x = CreateObject<UniformRandomVariable> ();
		x->SetAttribute ("Min", DoubleValue (0.0));
		x->SetAttribute ("Max", DoubleValue (max_x));
		Ptr <RandomVariableStream> y = CreateObject<UniformRandomVariable> ();
		y->SetAttribute ("Min", DoubleValue (0.0));
		y->SetAttribute ("Max", DoubleValue (max_y));
		Ptr <RandomVariableStream> z = CreateObject<UniformRandomVariable> ();
		z->SetAttribute ("Min", DoubleValue (0.0));
		z->SetAttribute ("Max", DoubleValue (0.0));
		randomBox->SetX(x);
		randomBox->SetY(y);
		randomBox->SetZ(z);
		client_mobility.SetPositionAllocator(randomBox);
		client_mobility.Install(clients);
		
		//put AP right in the middle
		MobilityHelper ap_mobility;
		Ptr<ListPositionAllocator> ap_positionAlloc = CreateObject<ListPositionAllocator> ();
		ap_positionAlloc->Add (Vector (max_x / 2, max_y / 2, 0.0));
		ap_mobility.SetPositionAllocator (ap_positionAlloc);
		ap_mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
		ap_mobility.Install(aps);
		
		//attach the gw and ap nodes to their csma (wired) interfaces
		CsmaHelper csma;
		csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));				//values taken from second.cc in /examples ns3 directory
		csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
		NetDeviceContainer csmaDevices;
		csmaDevices = csma.Install (csmaNodes);
		
		//create the wifi devices
		//APs
		YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
		wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 
		Ssid ssid = Ssid ("default-ssid");
		NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
		wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
		YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
		wifiPhy.SetChannel (wifiChannel.Create ());
		string phyMode ("DsssRate1Mbps");		//see rates.txt for explanation
		WifiHelper wifiAP = WifiHelper::Default ();
		wifiAP.SetStandard (WIFI_PHY_STANDARD_80211b);
		wifiAP.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode",StringValue (phyMode), "ControlMode",StringValue (phyMode));
		NetDeviceContainer apWifiDevs = wifiAP.Install (wifiPhy, wifiMac, aps);;
		
		//clients
		WifiHelper wifi = WifiHelper::Default ();
		wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
		NetDeviceContainer clientDevs;
		Ipv4InterfaceContainer clientInterfaces;
		wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (true));
		clientDevs = wifi.Install (wifiPhy, wifiMac, clients);	
		
		//Assign IP addresses to devices
		InternetStackHelper stack;
		stack.Install (gws);
		stack.Install (aps);
		stack.Install (clients);
		Ipv4AddressHelper address;
		address.SetBase ("10.1.1.0", "255.255.255.0");
		Ipv4InterfaceContainer csmaInterfaces;
		csmaInterfaces = address.Assign (csmaDevices);
		//iterate through all the aps and bridge their wifi and csma interfaces
		//assume GW = node 0, so skip it
		BridgeHelper bridge;
		NetDeviceContainer bridgeDev;
		Ipv4InterfaceContainer apInterfaces;
		for(unsigned int i = 1; i < csmaNodes.GetN(); i++)
			bridgeDev.Add(bridge.Install(csmaNodes.Get(i), NetDeviceContainer(apWifiDevs.Get(i-1), csmaDevices.Get(i))));
		apInterfaces = address.Assign (bridgeDev);
		clientInterfaces = address.Assign (clientDevs);	
		
		//create traffic sinks, ie) servers at the clients
		ApplicationContainer videoApps;
		uint16_t videoPort = 6970;
		PacketSinkHelper videoSink("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), videoPort));
		videoApps.Add(videoSink.Install(clients));
		videoApps.Start(Seconds(1.0));
		videoApps.Stop(Seconds(total_time - 0.1));
		
		//create traffic sources at the GW (one for each client)
		for(unsigned int i = 0; i < clientInterfaces.GetN(); i++)
		{
			Ipv4Address clientAddress = clientInterfaces.GetAddress(i); //assume that the device only has one interface for now, otherwise we need to give a second parameter
			OnOffHelper videoSource ("ns3::UdpSocketFactory", InetSocketAddress (clientAddress, videoPort));
			videoSource.SetAttribute ("PacketSize", UintegerValue (1024));
			videoSource.SetConstantRate (DataRate ("2.0Mbps"));
			videoSource.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
			videoSource.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
			videoApps.Add(videoSource.Install(gws));
			videoApps.Start(Seconds(1.0));
			videoApps.Stop(Seconds(total_time - 0.1));
		}
		
		//install flow monitor to keep stats on traffic
		FlowMonitorHelper flowmon;
		Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
		
		Simulator::Stop (Seconds (total_time));
		Simulator::Run ();
		
		monitor->CheckForLostPackets ();
		Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
		std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
		double total_throughput = 0;
		double total_pdr = 0;
		double total_e2e_delay = 0;
		double total_jitter = 0;
		int count = 0;
		for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
		{
			count++;
			if(i->second.rxPackets > 0)
			{
				total_e2e_delay += (i->second.delaySum).GetMicroSeconds() / i->second.rxPackets;
				total_jitter += (i->second.jitterSum).GetMicroSeconds() / i->second.rxPackets;
			}
			else
			{
				total_e2e_delay += 0;
				total_jitter += 0;
			}
			total_throughput += ((i->second.rxBytes * 8.0 / 1024 / 1024) / total_time);
			total_pdr += (double)i->second.rxPackets / (double)i->second.txPackets;
		}
		
		double average_throughput = total_throughput / count;
		double average_pdr = total_pdr / count;
		double average_e2e_delay = total_e2e_delay / count;
		double average_jitter = total_jitter / count;
		
		cout << fixed;
		cout << "RUN: " << run+1 << " of " << runs << " with " << num_clients << " clients" << endl;
		cout << "  total Throughput: " << total_throughput << " Mbps\n";
		cout << "  avg Thoughput: " << average_throughput << " Mbps\n";
		cout << "  PDR: " << average_pdr << "\n";
		cout << "  e2e Delay: " << average_e2e_delay << " microseconds\n";
		cout << "  jitter: " << average_jitter << " microseconds\n";
		cout << "---------------------------------------\n";
		
		output << fixed;		//prevents a bug with delay and jitter and precision
		output << average_throughput << "," << total_throughput << "," << average_e2e_delay << "," << average_pdr << "," << average_jitter << endl;
		
		Simulator::Destroy ();
	}
	return 0;
}
