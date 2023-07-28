enum ip_protocol {
    IP_PROTO_HOPOPT = 0,            ///< IPv6 Hop-by-Hop Option
    IP_PROTO_ICMP = 1,              ///< Internet Control Message
    IP_PROTO_IGMP = 2,              ///< Internet Group Management
    IP_PROTO_GGP = 3,               ///< Gateway-to-Gateway
    IP_PROTO_IPv4 = 4,              ///< IPv4 encapsulation
    IP_PROTO_ST = 5,                ///< Stream
    IP_PROTO_TCP = 6,               ///< Transmission Control
    IP_PROTO_CBT = 7,               ///< CBT
    IP_PROTO_EGP = 8,               ///< Exterior Gateway Protocol
    IP_PROTO_IGP = 9,               ///< any private interior gateway(used by Cisco for their IGRP)
    IP_PROTO_BBN_RCC_MON = 10,      ///< BBN RCC Monitoring
    IP_PROTO_NVP_II = 11,           ///< Network Voice Protocol
    IP_PROTO_PUP = 12,              ///< PUP
    IP_PROTO_EMCON = 14,            ///< EMCON
    IP_PROTO_XNET = 15,             ///< Cross Net Debugger
    IP_PROTO_CHAOS = 16,            ///< Chaos
    IP_PROTO_UDP = 17,              ///< User Datagram
    IP_PROTO_MUX = 18,              ///< Multiplexing
    IP_PROTO_DCN_MEAS = 19,         ///< DCN Measurement Subsystems
    IP_PROTO_HMP = 20,              ///< Host Monitoring
    IP_PROTO_PRM = 21,              ///< Packet Radio Measurement
    IP_PROTO_XNS_IDP = 22,          ///< XEROX NS IDP
    IP_PROTO_TRUNK_1 = 23,          ///< Trunk-1
    IP_PROTO_TRUNK_2 = 24,          ///< Trunk-2
    IP_PROTO_LEAF_1 = 25,           ///< Leaf-1
    IP_PROTO_LEAF_2 = 26,           ///< Leaf-2
    IP_PROTO_RDP = 27,              ///< Reliable Data Protocol
    IP_PROTO_IRTP = 28,             ///< Internet Reliable Transaction
    IP_PROTO_ISO_TP4 = 29,          ///< ISO Transport Protocol Class 4
    IP_PROTO_NETBLT = 30,           ///< Bulk Data Transfer Protocol
    IP_PROTO_MFE_NSP = 31,          ///< MFE Network Services Protocol
    IP_PROTO_MERIT_INP = 32,        ///< MERIT Internodal Protocol
    IP_PROTO_DCCP = 33,             ///< Datagram Congestion Control Protocol
    IP_PROTO_3PC = 34,              ///< Third Party Connect Protocol
    IP_PROTO_IDPR = 35,             ///< Inter-Domain Policy Routing Protocol
    IP_PROTO_XTP = 36,              ///< XTP
    IP_PROTO_DDP = 37,              ///< Datagram Delivery Protocol
    IP_PROTO_IDPR_CMTP = 38,        ///< IDPR Control Message Transport Proto
    IP_PROTO_TPpp = 39,             ///< TP++ Transport Protocol
    IP_PROTO_IL = 40,               ///< IL Transport Protocol
    IP_PROTO_IPv6 = 41,             ///< IPv6 encapsulation
    IP_PROTO_SDRP = 42,             ///< Source Demand Routing Protocol
    IP_PROTO_IPv6_Route = 43,       ///< Routing Header for IPv6
    IP_PROTO_IPv6_Frag = 44,        ///< Fragment Header for IPv6
    IP_PROTO_IDRP = 45,             ///< Inter-Domain Routing Protocol
    IP_PROTO_RSVP = 46,             ///< Reservation Protocol
    IP_PROTO_GRE = 47,              ///< Generic Routing Encapsulation
    IP_PROTO_DSR = 48,              ///< Dynamic Source Routing Protocol
    IP_PROTO_BNA = 49,              ///< BNA
    IP_PROTO_ESP = 50,              ///< Encap Security Payload
    IP_PROTO_AH = 51,               ///< Authentication Header
    IP_PROTO_I_NLSP = 52,           ///< Integrated Net Layer Security  TUBA
    IP_PROTO_NARP = 54,             ///< NBMA Address Resolution Protocol
    IP_PROTO_MOBILE = 55,           ///< IP Mobility
    IP_PROTO_TLSP = 56,             ///< Transport Layer Security Protocol using Kryptonet key management
    IP_PROTO_SKIP = 57,             ///< SKIP
    IP_PROTO_IPv6_ICMP = 58,        ///< ICMP for IPv6
    IP_PROTO_IPv6_NoNxt = 59,       ///< No Next Header for IPv6
    IP_PROTO_IPv6_Opts = 60,        ///< Destination Options for IPv6
    IP_PROTO_CFTP = 62,             ///< CFTP
    IP_PROTO_SAT_EXPAK = 64,        ///< SATNET and Backroom EXPAK
    IP_PROTO_KRYPTOLAN = 65,        ///< Kryptolan
    IP_PROTO_RVD = 66,              ///< MIT Remote Virtual Disk Protocol
    IP_PROTO_IPPC = 67,             ///< Internet Pluribus Packet Core
    IP_PROTO_SAT_MON = 69,          ///< SATNET Monitoring
    IP_PROTO_VISA = 70,             ///< VISA Protocol
    IP_PROTO_IPCV = 71,             ///< Internet Packet Core Utility
    IP_PROTO_CPNX = 72,             ///< Computer Protocol Network Executive
    IP_PROTO_CPHB = 73,             ///< Computer Protocol Heart Beat
    IP_PROTO_WSN = 74,              ///< Wang Span Network
    IP_PROTO_PVP = 75,              ///< Packet Video Protocol
    IP_PROTO_BR_SAT_MON = 76,       ///< Backroom SATNET Monitoring
    IP_PROTO_SUN_ND = 77,           ///< SUN ND PROTOCOL-Temporary
    IP_PROTO_WB_MON = 78,           ///< WIDEBAND Monitoring
    IP_PROTO_WB_EXPAK = 79,         ///< WIDEBAND EXPAK
    IP_PROTO_ISO_IP = 80,           ///< ISO Internet Protocol
    IP_PROTO_VMTP = 81,             ///< VMTP
    IP_PROTO_SECURE_VMTP = 82,      ///< SECURE-VMTP
    IP_PROTO_VINES = 83,            ///< VINES
    IP_PROTO_TTP = 84,              ///< Transaction Transport Protocol
    IP_PROTO_IPTM = 84,             ///< Internet Protocol Traffic Manager
    IP_PROTO_NSFNET_IGP = 85,       ///< NSFNET-IGP
    IP_PROTO_DGP = 86,              ///< Dissimilar Gateway Protocol
    IP_PROTO_TCF = 87,              ///< TCF
    IP_PROTO_EIGRP = 88,            ///< EIGRP
    IP_PROTO_OSPFIGP = 89,          ///< OSPFIGP
    IP_PROTO_Sprite_RPC = 90,       ///< Sprite RPC Protocol
    IP_PROTO_LARP = 91,             ///< Locus Address Resolution Protocol
    IP_PROTO_MTP = 92,              ///< Multicast Transport Protocol
    IP_PROTO_AX_25 = 93,            ///< AX.25 Frames
    IP_PROTO_IPIP = 94,             ///< IP-within-IP Encapsulation Protocol
    IP_PROTO_SCC_SP = 96,           ///< Semaphore Communications Sec. Pro.
    IP_PROTO_ETHERIP = 97,          ///< Ethernet-within-IP Encapsulation
    IP_PROTO_ENCAP = 98,            ///< Encapsulation Header
    IP_PROTO_GMTP = 100,            ///< GMTP
    IP_PROTO_IFMP = 101,            ///< Ipsilon Flow Management Protocol
    IP_PROTO_PNNI = 102,            ///< PNNI over IP
    IP_PROTO_PIM = 103,             ///< Protocol Independent Multicast
    IP_PROTO_ARIS = 104,            ///< ARIS
    IP_PROTO_SCPS = 105,            ///< SCPS
    IP_PROTO_QNX = 106,             ///< QNX
    IP_PROTO_A_N = 107,             ///< Active Networks
    IP_PROTO_IPComp = 108,          ///< IP Payload Compression Protocol
    IP_PROTO_SNP = 109,             ///< Sitara Networks Protocol
    IP_PROTO_Compaq_Peer = 110,     ///< Compaq Peer Protocol
    IP_PROTO_IPX_in_IP = 111,       ///< IPX in IP
    IP_PROTO_VRRP = 112,            ///< Virtual Router Redundancy Protocol
    IP_PROTO_PGM = 113,             ///< PGM Reliable Transport Protocol
    IP_PROTO_L2TP = 115,            ///< Layer Two Tunneling Protocol
    IP_PROTO_DDX = 116,             ///< D-II Data Exchange (DDX)
    IP_PROTO_IATP = 117,            ///< Interactive Agent Transfer Protocol
    IP_PROTO_STP = 118,             ///< Schedule Transfer Protocol
    IP_PROTO_SRP = 119,             ///< SpectraLink Radio Protocol
    IP_PROTO_UTI = 120,             ///< UTI
    IP_PROTO_SMP = 121,             ///< Simple Message Protocol
    IP_PROTO_PTP = 123,             ///< Performance Transparency Protocol
    IP_PROTO_ISIS = 124,            ///<
    IP_PROTO_FIRE = 125,            ///<
    IP_PROTO_CRTP = 126,            ///< Combat Radio Transport Protocol
    IP_PROTO_CRUDP = 127,           ///< Combat Radio User Datagram
    IP_PROTO_SSCOPMCE = 128,        ///<
    IP_PROTO_IPLT = 129,            ///<
    IP_PROTO_SPS = 130,             ///< Secure Packet Shield
    IP_PROTO_PIPE = 131,            ///< Private IP Encapsulation within IP
    IP_PROTO_SCTP = 132,            ///< Stream Control Transmission Protocol
    IP_PROTO_FC = 133,              ///< Fibre Channel
    IP_PROTO_RSVP_E2E_IGNORE = 134, ///<
    IP_PROTO_MH = 135,              ///<
    IP_PROTO_UDPLite = 136,         ///<
    IP_PROTO_MPLS_in_IP = 137,      ///<
    IP_PROTO_manet = 138,           ///< MANET Protocols
    IP_PROTO_HIP = 139,             ///< Host Identity Protocol
    IP_PROTO_Shim6 = 140,           ///< Shim6 Protocol
    IP_PROTO_WESP = 141,            ///< Wrapped Encapsulating Security Payload
    IP_PROTO_ROHC = 142,            ///< Robust Header Compression
};
