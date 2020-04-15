/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

// Network topology
//
//       n0 ----------- n1
//            1 Mbps
//             10 ms
//
// - Flow from n0 to n1 using BulkSendApplication.

#include <iostream>
#include <fstream>
#include <string>

#include <ns3/core-module.h>
#include <ns3/network-module.h>
#include <ns3/internet-module.h>
#include <ns3/flow-monitor-helper.h>
#include <ns3/point-to-point-module.h>
#include <ns3/applications-module.h>
#include <ns3/flow-monitor-module.h>
#include <ns3/error-model.h>
#include <ns3/tcp-header.h>
#include <ns3/udp-header.h>
#include <ns3/enum.h>
#include <ns3/event-id.h>
#include <ns3/ipv4-global-routing-helper.h>
#include <ns3/scheduler.h>
#include <ns3/calendar-scheduler.h>
#include <ns3/gnuplot.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TCP_Comparison");

  AsciiTraceHelper ascii;

class MyApp : public Application
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t pktSize, uint32_t numPkt, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     app_socket;
  Address         app_peer;
  uint32_t        app_pktSize;
  uint32_t        app_numPkt;
  DataRate        app_dataRate;
  EventId         app_sendEvent;
  bool            app_running;
  uint32_t        app_packetsSent;
};

MyApp::MyApp ()
  : app_socket (0),
    app_peer (),
    app_pktSize (0),
    app_numPkt (0),
    app_dataRate (0),
    app_sendEvent (),
    app_running (false),
    app_packetsSent (0)
{
}

MyApp::~MyApp()
{
  app_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t pktSize, uint32_t numPkt, DataRate dataRate)
{
  app_socket = socket;
  app_peer = address;
  app_pktSize = pktSize;
  app_numPkt = numPkt;
  app_dataRate = dataRate;

}

void
MyApp::StartApplication (void)
{
  app_running = true;
  app_packetsSent = 0;
  app_socket->Bind ();
  app_socket->Connect (app_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  app_running = false;

  if (app_sendEvent.IsRunning ())
    {
      Simulator::Cancel (app_sendEvent);
    }

  if (app_socket)
    {
      app_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (app_pktSize);
  app_socket->Send (packet);

  if (++app_packetsSent < app_numPkt)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (app_running)
    {
      Time tNext (Seconds (app_pktSize * 8 / static_cast<double> (app_dataRate.GetBitRate ())));
      app_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

/*--------------------class definitions over----------------------*/

//to keep track of changes in congestion window, using callbacks  from TCP when window is changed
static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream() << Simulator::Now ().GetSeconds () << "\t" << ((double)newCwnd/1000) << "\n";
}

static void
RxDrop (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p)
{
  static int i=1;
  *stream->GetStream() << Simulator::Now ().GetSeconds () << "\t" << i << "\n";
  i++;
}

void ThroughputMonitor (Ptr<OutputStreamWrapper> stream, FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon)
{
  static double time = 0;
  double localThrou=0;
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
  Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin (); stats != flowStats.end (); ++stats)
  {
    localThrou += stats->second.rxBytes;
  }
  *stream->GetStream() << time << "\t" << ((double)localThrou/1000) << '\n';
  time += 0.05;
  Simulator::Schedule(Seconds(0.05),&ThroughputMonitor, stream , fmhelper, flowMon);
  {
    flowMon->SerializeToXmlFile ("ThroughputMonitor.xml", true, true);
  }
}
void menu()
{
  std::cout<<"Enter [1-5] for TCP variant:\n1. TCP New Reno\n2. TCP Hybla\n3. TCP Westwood\n4. TCP Scalable\n5. TCP Vegas\n";
}

/*------------------------ Helper functions defined --------------------*/
int
main (int argc, char *argv[])
{

  float simulation_time = 1.8; //seconds
   //change these parameters for different simulations
    std::string tcp_variant;
    int option;
    menu();
    std::cin>>option;
    

    switch (option)
    {
      case 1:
      tcp_variant = "TcpNewReno";
      Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId()));
      break;

      case 2:
      tcp_variant ="TcpHybla";
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpHybla::GetTypeId()));
      break;

      case 3:
      tcp_variant ="TcpWestwood";
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      Config::SetDefault ("ns3::TcpWestwood::FilterType", EnumValue (TcpWestwood::TUSTIN));
      break;

      case 4:
      tcp_variant ="TcpScalable";
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpScalable::GetTypeId()));
      break;

      case 5:
      tcp_variant ="TcpVegas";
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpVegas::GetTypeId ()));
      break;

      default:
      fprintf (stderr, "Invalid TCP version\n");
      exit (1);
    }


      std::string a_s = "bytes_"+tcp_variant+".txt";
      std::string b_s = "dropped_"+tcp_variant+".txt";
      std::string c_s = "cwnd_"+tcp_variant+".txt";

    // Create file streams for data storage
    Ptr<OutputStreamWrapper> total_bytes_data = ascii.CreateFileStream (a_s);
    Ptr<OutputStreamWrapper> dropped_packets_data = ascii.CreateFileStream (b_s);
    Ptr<OutputStreamWrapper> cwnd_data = ascii.CreateFileStream (c_s);
    

  // creating nodes
  NodeContainer nodes;
  nodes.Create (2);
  NS_LOG_INFO ("Created 2 nodes.");

  //NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));

  // Install internet stack

  InternetStackHelper stack;
  stack.Install (nodes);
  NS_LOG_INFO ("Installed internet stacks on the nodes");

  PointToPointHelper pointToPoint;
  pointToPoint.SetQueue ("ns3::DropTailQueue");
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NS_LOG_INFO ("P2P link created....");
  NS_LOG_INFO ("Bandwidth : 1Mbps");
  NS_LOG_INFO ("Delay : 10ms");
  NS_LOG_INFO ("Queue Type : DropTailQueue");

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);
  NS_LOG_INFO ("Installing net device to nodes, adding MAC Address and Queue.");


  Ptr<RateErrorModel> error_model = CreateObject<RateErrorModel> ();
  error_model->SetAttribute ("ErrorRate", DoubleValue (0.0001));
  devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (error_model));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);
  NS_LOG_INFO ("Assigning IP base addresses to the nodes");




  // Turn on global static routing so we can actually be routed across the network.
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t tcp_sink_port = 4641;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), tcp_sink_port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (interfaces.GetAddress (1), tcp_sink_port));
  ApplicationContainer tcp_sink_app_first = packetSinkHelper.Install (nodes.Get (1));
  tcp_sink_app_first.Start (Seconds (0.));
  tcp_sink_app_first.Stop (Seconds (simulation_time));

  Ptr<Socket> ns3TcpSocket1 = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ()); // source at no

  // Trace CongestionWindow
  ns3TcpSocket1->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange,cwnd_data));


  // create TCP application at n0
  Ptr<MyApp> tcp_ftp_agent = CreateObject<MyApp> ();
  tcp_ftp_agent->Setup (ns3TcpSocket1, sinkAddress, 1040, 1000, DataRate ("250kbps"));
  nodes.Get (0)->AddApplication (tcp_ftp_agent);
  tcp_ftp_agent->SetStartTime (Seconds (0.));
  tcp_ftp_agent->SetStopTime (Seconds (simulation_time));

  devices.Get(1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&RxDrop,dropped_packets_data));

  // udp application 1

  uint16_t cbr_sink_port_first = 4642;
  Address cbr_sink_address_1 (InetSocketAddress (interfaces.GetAddress (1), cbr_sink_port_first));
  PacketSinkHelper packetSinkHelper2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), cbr_sink_port_first));
  ApplicationContainer cbr_sink_app_first = packetSinkHelper2.Install (nodes.Get (1)); //n1 as sink
  cbr_sink_app_first.Start (Seconds (0.));
  cbr_sink_app_first.Stop (Seconds (simulation_time));

  Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ()); //source at n0

  // Create UDP application  at n0
  Ptr<MyApp> first_cbr_agent = CreateObject<MyApp> ();
  first_cbr_agent->Setup (ns3UdpSocket, cbr_sink_address_1, 1040, 100000, DataRate ("250Kbps"));
  nodes.Get(0)->AddApplication (first_cbr_agent);
  first_cbr_agent->SetStartTime (Seconds (0.2));
  first_cbr_agent->SetStopTime (Seconds (simulation_time));

  // udp application 2

  uint16_t cbr_sink_port_second = 4643;
  Address cbr_sink_address_2 (InetSocketAddress (interfaces.GetAddress (1), cbr_sink_port_second));
  PacketSinkHelper packetSinkHelper3("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), cbr_sink_port_second));
  ApplicationContainer sinkApps3 = packetSinkHelper3.Install (nodes.Get (1)); //n1 as sink
  sinkApps3.Start (Seconds (0.));
  sinkApps3.Stop (Seconds (simulation_time));

  ns3UdpSocket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ()); //source at n0

  // Create UDP application 2 at n0
  Ptr<MyApp> second_cbr_agent = CreateObject<MyApp> ();
  second_cbr_agent->Setup (ns3UdpSocket, cbr_sink_address_2, 1040, 100000, DataRate ("250Kbps"));
  nodes.Get(0)->AddApplication (second_cbr_agent);
  second_cbr_agent->SetStartTime (Seconds (0.4));
  second_cbr_agent->SetStopTime (Seconds (simulation_time));

  // udp application 3

  uint16_t cbr_sink_port_third = 4644;
  Address cbr_sink_address_3 (InetSocketAddress (interfaces.GetAddress (1), cbr_sink_port_third));
  PacketSinkHelper packetSinkHelper4("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), cbr_sink_port_third));
  ApplicationContainer cbr_sink_app_third = packetSinkHelper4.Install (nodes.Get (1)); //n1 as sink
  cbr_sink_app_third.Start (Seconds (0.));
  cbr_sink_app_third.Stop (Seconds (simulation_time));

  ns3UdpSocket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ()); //source at n0

  // Create UDP application 3 at n0
  Ptr<MyApp> third_cbr_agent = CreateObject<MyApp> ();
  third_cbr_agent->Setup (ns3UdpSocket, cbr_sink_address_3, 1040, 100000, DataRate ("250Kbps"));
  nodes.Get(0)->AddApplication (third_cbr_agent);
  third_cbr_agent->SetStartTime (Seconds (0.6));
  third_cbr_agent->SetStopTime (Seconds (1.2));

  // udp application 4

  uint16_t cbr_sink_port_fourth= 4645;
  Address cbr_sink_address_4 (InetSocketAddress (interfaces.GetAddress (1), cbr_sink_port_fourth));
  PacketSinkHelper packetSinkHelper5("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), cbr_sink_port_fourth));
  ApplicationContainer cbr_sink_app_fourth = packetSinkHelper5.Install (nodes.Get (1)); //n1 as sink
  cbr_sink_app_fourth.Start (Seconds (0.));
  cbr_sink_app_fourth.Stop (Seconds (simulation_time));

  ns3UdpSocket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ()); //source at n0

  // Create UDP application 4 at n0
  Ptr<MyApp> fourth_cbr_agent = CreateObject<MyApp> ();
  fourth_cbr_agent->Setup (ns3UdpSocket, cbr_sink_address_4, 1040, 100000, DataRate ("250Kbps"));
  nodes.Get(0)->AddApplication (fourth_cbr_agent);
  fourth_cbr_agent->SetStartTime (Seconds (0.8));
  fourth_cbr_agent->SetStopTime (Seconds (1.4));

  // udp application 5

  uint16_t cbr_sink_port_fifth = 4647;
  Address cbr_sink_address_5 (InetSocketAddress (interfaces.GetAddress (1), cbr_sink_port_fifth));
  PacketSinkHelper packetSinkHelper6("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), cbr_sink_port_fifth));
  ApplicationContainer cbr_sink_app_fifth = packetSinkHelper6.Install (nodes.Get (1)); //n1 as sink
  cbr_sink_app_fifth.Start (Seconds (0.));
  cbr_sink_app_fifth.Stop (Seconds (simulation_time));

  ns3UdpSocket = Socket::CreateSocket (nodes.Get (0), UdpSocketFactory::GetTypeId ()); //source at n0

  // Create UDP application 5 at n0
  Ptr<MyApp> fifth_cbr_agent = CreateObject<MyApp> ();
  fifth_cbr_agent->Setup (ns3UdpSocket, cbr_sink_address_5, 1040, 100000, DataRate ("250Kbps"));
  nodes.Get(0)->AddApplication (fifth_cbr_agent);
  fifth_cbr_agent->SetStartTime (Seconds (1.0));
  fifth_cbr_agent->SetStopTime (Seconds (1.6));

  NS_LOG_INFO ("Applications created.");

/*--------------Application creation ends----------------*/

  //Configuring Analysis tools
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  ThroughputMonitor(total_bytes_data, &flowHelper, flowMonitor); //Call ThroughputMonitor Function
  flowMonitor->SerializeToXmlFile("FlowMonitor-Throughput.xml", true, true);



  Simulator::Stop (Seconds (simulation_time));
  Simulator::Run ();

  //Analysis of simulation
  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
  std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter;
  flowMonitor->SerializeToXmlFile("lab-4.xml", true, true);

  Simulator::Destroy ();

  return 0;
}
