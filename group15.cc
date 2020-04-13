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

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;

}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream() << Simulator::Now ().GetSeconds () << "\t" << newCwnd << "\n";
}

static void
RxDrop (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> p)
{
  static int i=1;
  *stream->GetStream() << Simulator::Now ().GetSeconds () << "\t" << i << "\n";
  i++;
}

void ThroughputMonitor (Ptr<OutputStreamWrapper> stream, FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon,Gnuplot2dDataset DataSet)
{
  static double time = 0;
  double localThrou=0;
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
  Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin (); stats != flowStats.end (); ++stats)
  {
    localThrou += stats->second.rxBytes;
    DataSet.Add((double)Simulator::Now().GetSeconds(),(double) localThrou);
  }
  *stream->GetStream() << time << "\t" << localThrou << '\n';
  time += 0.1;
  Simulator::Schedule(Seconds(0.1),&ThroughputMonitor, stream , fmhelper, flowMon,DataSet);
  {
    flowMon->SerializeToXmlFile ("ThroughputMonitor.xml", true, true);
  }
}

int
main (int argc, char *argv[])
{

  float simulation_time = 1.8; //seconds
   //change these parameters for different simulations
    std::string tcp_variant;
    std:: cout << "Enter Tcp Tcp variant\n";
    std::cin >> tcp_variant ;

    // if (tcp_variant.compare("TcpTahoe") == 0)
      // Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpTahoe::GetTypeId()));
    // else if (tcp_variant.compare("TcpReno") == 0)
    //   Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpReno::GetTypeId()));
    // else 
      if (tcp_variant.compare("TcpNewReno") == 0)
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId()));
      else if (tcp_variant.compare("TcpVegas") == 0)
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpVegas::GetTypeId ()));
      else if (tcp_variant.compare ("TcpWestwood") == 0){ 
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
        Config::SetDefault ("ns3::TcpWestwood::FilterType", EnumValue (TcpWestwood::TUSTIN));
      }
      else if(tcp_variant.compare("TcpScalable") ==0)
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpScalable::GetTypeId()));
      else if(tcp_variant.compare("TcpHybla") ==0)
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpHybla::GetTypeId()));
      else
      {
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

  NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));

  // Install internet stack

  InternetStackHelper stack;
  stack.Install (nodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetQueue ("ns3::DropTailQueue");
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (n0n1);

  Ptr<RateErrorModel> error_model = CreateObject<RateErrorModel> ();
  error_model->SetAttribute ("ErrorRate", DoubleValue (0.00001));
  devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (error_model));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.252");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

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

  std::string dataTitle= "Throughput Data";

  Gnuplot2dDataset dataset;
  dataset.SetTitle (dataTitle);
  dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  ThroughputMonitor(total_bytes_data, &flowHelper, flowMonitor, dataset); //Call ThroughputMonitor Function
  flowMonitor->SerializeToXmlFile("FlowMonitor-Throughput.xml", true, true);

  Simulator::Stop (Seconds (simulation_time));
  Simulator::Run ();
  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
  std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter;
  flowMonitor->SerializeToXmlFile("lab-4.xml", true, true);

  Simulator::Destroy ();

  return 0;
}
