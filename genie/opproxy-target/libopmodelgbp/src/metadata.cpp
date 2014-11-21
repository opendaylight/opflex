/**
 * 
 * SOME COPYRIGHT
 * 
 * metadata.cpp
 * 
 * generated metadata.cpp file genie code generation framework free of license.
 *  
 */
#include <boost/assign/list_of.hpp>
#include <opmodelgbp/metadata/metadata.hpp>
#include <opmodelgbp/dmtree/Root.hpp>
namespace opmodelgbp
{
const opflex::modb::ModelMetadata& getMetadata()
{
    using namespace opflex::modb;
    using namespace boost::assign;
    static const opflex::modb::ModelMetadata metadata("opmodelgbp", 
        list_of
                (
                    ClassInfo(1, ClassInfo::LOCAL_ONLY, "DmtreeRoot", "init",
                         list_of
                             (PropertyInfo(2147516423ul, "RelatorUniverse", PropertyInfo::COMPOSITE, 7, PropertyInfo::VECTOR)) // item:mclass(relator/Universe)
                             (PropertyInfo(2147516434ul, "EpdrL2Discovered", PropertyInfo::COMPOSITE, 18, PropertyInfo::VECTOR)) // item:mclass(epdr/L2Discovered)
                             (PropertyInfo(2147516436ul, "EpdrL3Discovered", PropertyInfo::COMPOSITE, 20, PropertyInfo::VECTOR)) // item:mclass(epdr/L3Discovered)
                             (PropertyInfo(2147516442ul, "EprL2Universe", PropertyInfo::COMPOSITE, 26, PropertyInfo::VECTOR)) // item:mclass(epr/L2Universe)
                             (PropertyInfo(2147516443ul, "EprL3Universe", PropertyInfo::COMPOSITE, 27, PropertyInfo::VECTOR)) // item:mclass(epr/L3Universe)
                             (PropertyInfo(2147516494ul, "PolicyUniverse", PropertyInfo::COMPOSITE, 78, PropertyInfo::VECTOR)) // item:mclass(policy/Universe)
                             ,
                         std::vector<prop_id_t>() // no naming rule; assume cardinality of 1 in any containment rule
                         )
                )
                (
                    ClassInfo(7, ClassInfo::LOCAL_ONLY, "RelatorUniverse", "init",
                         list_of
                             (PropertyInfo(2147713069ul, "EpdrEndPointToGroupRRes", PropertyInfo::COMPOSITE, 45, PropertyInfo::VECTOR)) // item:mclass(epdr/EndPointToGroupRRes)
                             (PropertyInfo(2147713070ul, "GbpEpGroupToNetworkRRes", PropertyInfo::COMPOSITE, 46, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupToNetworkRRes)
                             (PropertyInfo(2147713071ul, "GbpRuleToClassifierRRes", PropertyInfo::COMPOSITE, 47, PropertyInfo::VECTOR)) // item:mclass(gbp/RuleToClassifierRRes)
                             (PropertyInfo(2147713072ul, "GbpBridgeDomainToNetworkRRes", PropertyInfo::COMPOSITE, 48, PropertyInfo::VECTOR)) // item:mclass(gbp/BridgeDomainToNetworkRRes)
                             (PropertyInfo(2147713079ul, "GbpEpGroupToProvContractRRes", PropertyInfo::COMPOSITE, 55, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupToProvContractRRes)
                             (PropertyInfo(2147713080ul, "GbpFloodDomainToNetworkRRes", PropertyInfo::COMPOSITE, 56, PropertyInfo::VECTOR)) // item:mclass(gbp/FloodDomainToNetworkRRes)
                             (PropertyInfo(2147713083ul, "GbpEpGroupToConsContractRRes", PropertyInfo::COMPOSITE, 59, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupToConsContractRRes)
                             (PropertyInfo(2147713105ul, "GbpSubnetsToNetworkRRes", PropertyInfo::COMPOSITE, 81, PropertyInfo::VECTOR)) // item:mclass(gbp/SubnetsToNetworkRRes)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(relator/Universe/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(9, ClassInfo::LOCAL_ONLY, "CdpConfig", "policyreg",
                         list_of
                             (PropertyInfo(294913ul, "state", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("PlatformAdminStateT",
                                     list_of
                                             (ConstInfo("off", 0)) // item:mconst(platform/AdminState/off)
                                             (ConstInfo("on", 1)) // item:mconst(platform/AdminState/on)
                                     ))) // item:mprop(cdp/Config/state)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(platform/ConfigComponent/platform/Config); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(10, ClassInfo::POLICY, "PlatformConfig", "policyreg",
                         list_of
                             (PropertyInfo(327681ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2147811337ul, "CdpConfig", PropertyInfo::COMPOSITE, 9, PropertyInfo::VECTOR)) // item:mclass(cdp/Config)
                             (PropertyInfo(2147811341ul, "L2Config", PropertyInfo::COMPOSITE, 13, PropertyInfo::VECTOR)) // item:mclass(l2/Config)
                             (PropertyInfo(2147811343ul, "LldpConfig", PropertyInfo::COMPOSITE, 15, PropertyInfo::VECTOR)) // item:mclass(lldp/Config)
                             (PropertyInfo(2147811344ul, "StpConfig", PropertyInfo::COMPOSITE, 16, PropertyInfo::VECTOR)) // item:mclass(stp/Config)
                             (PropertyInfo(2147811345ul, "LacpConfig", PropertyInfo::COMPOSITE, 17, PropertyInfo::VECTOR)) // item:mclass(lacp/Config)
                             ,
                         list_of // item:mnamer:rule(platform/Config/policy/Universe)
                             (327681) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(platform/Config/policy/Universe/1)
                         )
                )
                (
                    ClassInfo(12, ClassInfo::LOCAL_ONLY, "GbpeInstContext", "policyreg",
                         list_of
                             (PropertyInfo(393218ul, "classid", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/InstContext/classid)
                             (PropertyInfo(393217ul, "vnid", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/InstContext/vnid)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbpe/InstContext/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(13, ClassInfo::LOCAL_ONLY, "L2Config", "policyreg",
                         list_of
                             (PropertyInfo(425985ul, "state", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(l2/Config/state)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(platform/ConfigComponent/platform/Config); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(14, ClassInfo::POLICY, "GbpeL24Classifier", "policyreg",
                         list_of
                             (PropertyInfo(458757ul, "arpOpc", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("ArpOpcodeT",
                                     list_of
                                             (ConstInfo("reply", 2)) // item:mconst(arp/Opcode/reply)
                                             (ConstInfo("request", 1)) // item:mconst(arp/Opcode/request)
                                             (ConstInfo("unspecified", 0)) // item:mconst(arp/Opcode/unspecified)
                                     ))) // item:mprop(gbpe/L24Classifier/arpOpc)
                             (PropertyInfo(458755ul, "connectionTracking", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("GbpConnTrackT",
                                     list_of
                                             (ConstInfo("normal", 0)) // item:mconst(gbp/ConnTrack/normal)
                                             (ConstInfo("reflexive", 1)) // item:mconst(gbp/ConnTrack/reflexive)
                                     ))) // item:mprop(gbp/Classifier/connectionTracking)
                             (PropertyInfo(458762ul, "dFromPort", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/L24Classifier/dFromPort)
                             (PropertyInfo(458763ul, "dToPort", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/L24Classifier/dToPort)
                             (PropertyInfo(458756ul, "direction", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("GbpDirectionT",
                                     list_of
                                             (ConstInfo("bidirectional", 0)) // item:mconst(gbp/Direction/bidirectional)
                                             (ConstInfo("in", 1)) // item:mconst(gbp/Direction/in)
                                             (ConstInfo("out", 2)) // item:mconst(gbp/Direction/out)
                                     ))) // item:mprop(gbp/Classifier/direction)
                             (PropertyInfo(458758ul, "etherT", PropertyInfo::ENUM16, PropertyInfo::SCALAR,
                                 EnumInfo("L2EtherTypeT",
                                     list_of
                                             (ConstInfo("arp", 0x0806)) // item:mconst(l2/EtherType/arp)
                                             (ConstInfo("fcoe", 0x8906)) // item:mconst(l2/EtherType/fcoe)
                                             (ConstInfo("ipv4", 0x0800)) // item:mconst(l2/EtherType/ipv4)
                                             (ConstInfo("ipv6", 0x86DD)) // item:mconst(l2/EtherType/ipv6)
                                             (ConstInfo("mac_security", 0x88E5)) // item:mconst(l2/EtherType/mac_security)
                                             (ConstInfo("mpls_ucast", 0x8847)) // item:mconst(l2/EtherType/mpls_ucast)
                                             (ConstInfo("trill", 0x22F3)) // item:mconst(l2/EtherType/trill)
                                             (ConstInfo("unspecified", 0)) // item:mconst(l2/EtherType/unspecified)
                                     ))) // item:mprop(gbpe/L24Classifier/etherT)
                             (PropertyInfo(458753ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(458754ul, "order", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbp/Classifier/order)
                             (PropertyInfo(458759ul, "prot", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/L24Classifier/prot)
                             (PropertyInfo(458760ul, "sFromPort", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/L24Classifier/sFromPort)
                             (PropertyInfo(458761ul, "sToPort", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/L24Classifier/sToPort)
                             (PropertyInfo(2147942442ul, "GbpRuleFromClassifierRTgt", PropertyInfo::COMPOSITE, 42, PropertyInfo::VECTOR)) // item:mclass(gbp/RuleFromClassifierRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/Classifier/*)
                             (458753) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Classifier/*/1)
                         )
                )
                (
                    ClassInfo(15, ClassInfo::LOCAL_ONLY, "LldpConfig", "policyreg",
                         list_of
                             (PropertyInfo(491521ul, "rx", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("PlatformAdminStateT",
                                     list_of
                                             (ConstInfo("off", 0)) // item:mconst(platform/AdminState/off)
                                             (ConstInfo("on", 1)) // item:mconst(platform/AdminState/on)
                                     ))) // item:mprop(lldp/Config/rx)
                             (PropertyInfo(491522ul, "tx", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("PlatformAdminStateT",
                                     list_of
                                             (ConstInfo("off", 0)) // item:mconst(platform/AdminState/off)
                                             (ConstInfo("on", 1)) // item:mconst(platform/AdminState/on)
                                     ))) // item:mprop(lldp/Config/tx)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(platform/ConfigComponent/platform/Config); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(16, ClassInfo::LOCAL_ONLY, "StpConfig", "policyreg",
                         list_of
                             (PropertyInfo(524290ul, "bpduFilter", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("PlatformAdminStateT",
                                     list_of
                                             (ConstInfo("off", 0)) // item:mconst(platform/AdminState/off)
                                             (ConstInfo("on", 1)) // item:mconst(platform/AdminState/on)
                                     ))) // item:mprop(stp/Config/bpduFilter)
                             (PropertyInfo(524289ul, "bpduGuard", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("PlatformAdminStateT",
                                     list_of
                                             (ConstInfo("off", 0)) // item:mconst(platform/AdminState/off)
                                             (ConstInfo("on", 1)) // item:mconst(platform/AdminState/on)
                                     ))) // item:mprop(stp/Config/bpduGuard)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(platform/ConfigComponent/platform/Config); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(17, ClassInfo::LOCAL_ONLY, "LacpConfig", "policyreg",
                         list_of
                             (PropertyInfo(557060ul, "controlBits", PropertyInfo::U64, PropertyInfo::SCALAR,
                                 EnumInfo("LacpControlBitsT",
                                 std::vector<ConstInfo>()
                                     ))) // item:mprop(lacp/Config/controlBits)
                             (PropertyInfo(557058ul, "maxLinks", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(lacp/Config/maxLinks)
                             (PropertyInfo(557057ul, "minLinks", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(lacp/Config/minLinks)
                             (PropertyInfo(557059ul, "mode", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("LacpModeT",
                                 std::vector<ConstInfo>()
                                     ))) // item:mprop(lacp/Config/mode)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(platform/ConfigComponent/platform/Config); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(18, ClassInfo::LOCAL_ONLY, "EpdrL2Discovered", "init",
                         list_of
                             (PropertyInfo(2148073521ul, "EpdrLocalL2Ep", PropertyInfo::COMPOSITE, 49, PropertyInfo::VECTOR)) // item:mclass(epdr/LocalL2Ep)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(epdr/L2Discovered/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(20, ClassInfo::LOCAL_ONLY, "EpdrL3Discovered", "init",
                         list_of
                             (PropertyInfo(2148139068ul, "EpdrLocalL3Ep", PropertyInfo::COMPOSITE, 60, PropertyInfo::VECTOR)) // item:mclass(epdr/LocalL3Ep)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(epdr/L3Discovered/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(25, ClassInfo::LOCAL_ENDPOINT, "EprL2Ep", "policyelement",
                         list_of
                             (PropertyInfo(819204ul, "context", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/L2Ep/context)
                             (PropertyInfo(819203ul, "group", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/ReportedEp/group)
                             (PropertyInfo(819202ul, "mac", PropertyInfo::MAC, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/mac)
                             (PropertyInfo(819201ul, "uuid", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/uuid)
                             (PropertyInfo(2148302876ul, "EprL3Net", PropertyInfo::COMPOSITE, 28, PropertyInfo::VECTOR)) // item:mclass(epr/L3Net)
                             ,
                         list_of // item:mnamer:rule(epr/L2Ep/*)
                             (819204) //item:mprop(epr/L2Ep/context) of name component item:mnamer:component(epr/L2Ep/*/1)
                             (819202) //item:mprop(epr/EndPoint/mac) of name component item:mnamer:component(epr/L2Ep/*/2)
                         )
                )
                (
                    ClassInfo(26, ClassInfo::LOCAL_ENDPOINT, "EprL2Universe", "init",
                         list_of
                             (PropertyInfo(2148335641ul, "EprL2Ep", PropertyInfo::COMPOSITE, 25, PropertyInfo::VECTOR)) // item:mclass(epr/L2Ep)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(epr/L2Universe/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(27, ClassInfo::LOCAL_ENDPOINT, "EprL3Universe", "init",
                         list_of
                             (PropertyInfo(2148368413ul, "EprL3Ep", PropertyInfo::COMPOSITE, 29, PropertyInfo::VECTOR)) // item:mclass(epr/L3Ep)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(epr/L3Universe/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(28, ClassInfo::LOCAL_ENDPOINT, "EprL3Net", "policyelement",
                         list_of
                             (PropertyInfo(917505ul, "ip", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/L3Net/ip)
                             ,
                         list_of // item:mnamer:rule(epr/L3Net/*)
                             (917505) //item:mprop(epr/L3Net/ip) of name component item:mnamer:component(epr/L3Net/*/1)
                         )
                )
                (
                    ClassInfo(29, ClassInfo::LOCAL_ENDPOINT, "EprL3Ep", "policyelement",
                         list_of
                             (PropertyInfo(950277ul, "context", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/L3Ep/context)
                             (PropertyInfo(950275ul, "group", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/ReportedEp/group)
                             (PropertyInfo(950276ul, "ip", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/L3Ep/ip)
                             (PropertyInfo(950274ul, "mac", PropertyInfo::MAC, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/mac)
                             (PropertyInfo(950273ul, "uuid", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/uuid)
                             ,
                         list_of // item:mnamer:rule(epr/L3Ep/*)
                             (950277) //item:mprop(epr/L3Ep/context) of name component item:mnamer:component(epr/L3Ep/*/1)
                             (950276) //item:mprop(epr/L3Ep/ip) of name component item:mnamer:component(epr/L3Ep/*/2)
                         )
                )
                (
                    ClassInfo(30, ClassInfo::POLICY, "GbpClassifier", "policyreg",
                         list_of
                             (PropertyInfo(983043ul, "connectionTracking", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("GbpConnTrackT",
                                     list_of
                                             (ConstInfo("normal", 0)) // item:mconst(gbp/ConnTrack/normal)
                                             (ConstInfo("reflexive", 1)) // item:mconst(gbp/ConnTrack/reflexive)
                                     ))) // item:mprop(gbp/Classifier/connectionTracking)
                             (PropertyInfo(983044ul, "direction", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("GbpDirectionT",
                                     list_of
                                             (ConstInfo("bidirectional", 0)) // item:mconst(gbp/Direction/bidirectional)
                                             (ConstInfo("in", 1)) // item:mconst(gbp/Direction/in)
                                             (ConstInfo("out", 2)) // item:mconst(gbp/Direction/out)
                                     ))) // item:mprop(gbp/Classifier/direction)
                             (PropertyInfo(983041ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(983042ul, "order", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbp/Classifier/order)
                             (PropertyInfo(2148466730ul, "GbpRuleFromClassifierRTgt", PropertyInfo::COMPOSITE, 42, PropertyInfo::VECTOR)) // item:mclass(gbp/RuleFromClassifierRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/Classifier/*)
                             (983041) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Classifier/*/1)
                         )
                )
                (
                    ClassInfo(31, ClassInfo::POLICY, "GbpContract", "policyreg",
                         list_of
                             (PropertyInfo(1015809ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2148499489ul, "GbpSubject", PropertyInfo::COMPOSITE, 33, PropertyInfo::VECTOR)) // item:mclass(gbp/Subject)
                             (PropertyInfo(2148499509ul, "GbpEpGroupFromProvContractRTgt", PropertyInfo::COMPOSITE, 53, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromProvContractRTgt)
                             (PropertyInfo(2148499514ul, "GbpEpGroupFromConsContractRTgt", PropertyInfo::COMPOSITE, 58, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromConsContractRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/Contract/*)
                             (1015809) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Contract/*/1)
                         )
                )
                (
                    ClassInfo(32, ClassInfo::POLICY, "GbpEpGroup", "policyreg",
                         list_of
                             (PropertyInfo(1048577ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2148532236ul, "GbpeInstContext", PropertyInfo::COMPOSITE, 12, PropertyInfo::VECTOR)) // item:mclass(gbpe/InstContext)
                             (PropertyInfo(2148532264ul, "GbpEpGroupToNetworkRSrc", PropertyInfo::COMPOSITE, 40, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupToNetworkRSrc)
                             (PropertyInfo(2148532265ul, "EpdrEndPointFromGroupRTgt", PropertyInfo::COMPOSITE, 41, PropertyInfo::VECTOR)) // item:mclass(epdr/EndPointFromGroupRTgt)
                             (PropertyInfo(2148532275ul, "GbpEpGroupToProvContractRSrc", PropertyInfo::COMPOSITE, 51, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupToProvContractRSrc)
                             (PropertyInfo(2148532281ul, "GbpEpGroupToConsContractRSrc", PropertyInfo::COMPOSITE, 57, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupToConsContractRSrc)
                             ,
                         list_of // item:mnamer:rule(gbp/EpGroup/*)
                             (1048577) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/EpGroup/*/1)
                         )
                )
                (
                    ClassInfo(33, ClassInfo::POLICY, "GbpSubject", "policyreg",
                         list_of
                             (PropertyInfo(1081345ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2148565026ul, "GbpRule", PropertyInfo::COMPOSITE, 34, PropertyInfo::VECTOR)) // item:mclass(gbp/Rule)
                             ,
                         list_of // item:mnamer:rule(gbp/Subject/*)
                             (1081345) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Subject/*/1)
                         )
                )
                (
                    ClassInfo(34, ClassInfo::POLICY, "GbpRule", "policyreg",
                         list_of
                             (PropertyInfo(1114113ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(1114114ul, "order", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbp/Rule/order)
                             (PropertyInfo(2148597798ul, "GbpRuleToClassifierRSrc", PropertyInfo::COMPOSITE, 38, PropertyInfo::VECTOR)) // item:mclass(gbp/RuleToClassifierRSrc)
                             ,
                         list_of // item:mnamer:rule(gbp/Rule/*)
                             (1114113) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Rule/*/1)
                         )
                )
                (
                    ClassInfo(36, ClassInfo::POLICY, "GbpBridgeDomain", "policyreg",
                         list_of
                             (PropertyInfo(1179649ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2148663308ul, "GbpeInstContext", PropertyInfo::COMPOSITE, 12, PropertyInfo::VECTOR)) // item:mclass(gbpe/InstContext)
                             (PropertyInfo(2148663335ul, "GbpBridgeDomainToNetworkRSrc", PropertyInfo::COMPOSITE, 39, PropertyInfo::VECTOR)) // item:mclass(gbp/BridgeDomainToNetworkRSrc)
                             (PropertyInfo(2148663340ul, "GbpEpGroupFromNetworkRTgt", PropertyInfo::COMPOSITE, 44, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromNetworkRTgt)
                             (PropertyInfo(2148663350ul, "GbpFloodDomainFromNetworkRTgt", PropertyInfo::COMPOSITE, 54, PropertyInfo::VECTOR)) // item:mclass(gbp/FloodDomainFromNetworkRTgt)
                             (PropertyInfo(2148663375ul, "GbpSubnetsFromNetworkRTgt", PropertyInfo::COMPOSITE, 79, PropertyInfo::VECTOR)) // item:mclass(gbp/SubnetsFromNetworkRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/BridgeDomain/*)
                             (1179649) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/BridgeDomain/*/1)
                         )
                )
                (
                    ClassInfo(37, ClassInfo::RELATIONSHIP, "EpdrEndPointToGroupRSrc", "policyelement",
                         list_of
                             (PropertyInfo(1212419ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(epdr/EndPointToGroupRSrc/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(38, ClassInfo::RELATIONSHIP, "GbpRuleToClassifierRSrc", "policyreg",
                         list_of
                             (PropertyInfo(1245187ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         list_of // item:mnamer:rule(gbp/RuleToClassifierRSrc/*)
                             (1245188) //item:mprop(relator/NameResolvedRelSource/targetClass) of name component item:mnamer:component(gbp/RuleToClassifierRSrc/*/1)
                             (1245187) //item:mprop(relator/NameResolvedRelSource/targetName) of name component item:mnamer:component(gbp/RuleToClassifierRSrc/*/2)
                         )
                )
                (
                    ClassInfo(39, ClassInfo::RELATIONSHIP, "GbpBridgeDomainToNetworkRSrc", "policyreg",
                         list_of
                             (PropertyInfo(1277955ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/BridgeDomainToNetworkRSrc/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(40, ClassInfo::RELATIONSHIP, "GbpEpGroupToNetworkRSrc", "policyreg",
                         list_of
                             (PropertyInfo(1310723ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/EpGroupToNetworkRSrc/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(41, ClassInfo::REVERSE_RELATIONSHIP, "EpdrEndPointFromGroupRTgt", "policyelement",
                         list_of
                             (PropertyInfo(1343490ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1343491ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(1343489ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         list_of // item:mnamer:rule(epdr/EndPointFromGroupRTgt/*)
                             (1343491) //item:mprop(relator/Target/source) of name component item:mnamer:component(epdr/EndPointFromGroupRTgt/*/1)
                         )
                )
                (
                    ClassInfo(42, ClassInfo::REVERSE_RELATIONSHIP, "GbpRuleFromClassifierRTgt", "policyreg",
                         list_of
                             (PropertyInfo(1376258ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1376259ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(1376257ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         list_of // item:mnamer:rule(gbp/RuleFromClassifierRTgt/*)
                             (1376259) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/RuleFromClassifierRTgt/*/1)
                         )
                )
                (
                    ClassInfo(43, ClassInfo::REVERSE_RELATIONSHIP, "GbpBridgeDomainFromNetworkRTgt", "policyreg",
                         list_of
                             (PropertyInfo(1409026ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1409027ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(1409025ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         list_of // item:mnamer:rule(gbp/BridgeDomainFromNetworkRTgt/*)
                             (1409027) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/BridgeDomainFromNetworkRTgt/*/1)
                         )
                )
                (
                    ClassInfo(44, ClassInfo::REVERSE_RELATIONSHIP, "GbpEpGroupFromNetworkRTgt", "policyreg",
                         list_of
                             (PropertyInfo(1441794ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1441795ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(1441793ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         list_of // item:mnamer:rule(gbp/EpGroupFromNetworkRTgt/*)
                             (1441795) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/EpGroupFromNetworkRTgt/*/1)
                         )
                )
                (
                    ClassInfo(45, ClassInfo::RESOLVER, "EpdrEndPointToGroupRRes", "framework",
                         list_of
                             (PropertyInfo(1474562ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1474561ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(epdr/EndPointToGroupRRes/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(46, ClassInfo::RESOLVER, "GbpEpGroupToNetworkRRes", "framework",
                         list_of
                             (PropertyInfo(1507330ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1507329ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/EpGroupToNetworkRRes/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(47, ClassInfo::RESOLVER, "GbpRuleToClassifierRRes", "framework",
                         list_of
                             (PropertyInfo(1540098ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1540097ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/RuleToClassifierRRes/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(48, ClassInfo::RESOLVER, "GbpBridgeDomainToNetworkRRes", "framework",
                         list_of
                             (PropertyInfo(1572866ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1572865ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/BridgeDomainToNetworkRRes/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(49, ClassInfo::LOCAL_ONLY, "EpdrLocalL2Ep", "policyelement",
                         list_of
                             (PropertyInfo(1605634ul, "mac", PropertyInfo::MAC, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/mac)
                             (PropertyInfo(1605633ul, "uuid", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/uuid)
                             (PropertyInfo(2149089317ul, "EpdrEndPointToGroupRSrc", PropertyInfo::COMPOSITE, 37, PropertyInfo::VECTOR)) // item:mclass(epdr/EndPointToGroupRSrc)
                             ,
                         list_of // item:mnamer:rule(epdr/EndPoint/*)
                             (1605633) //item:mprop(epr/EndPoint/uuid) of name component item:mnamer:component(epdr/EndPoint/*/1)
                         )
                )
                (
                    ClassInfo(50, ClassInfo::POLICY, "GbpFloodDomain", "policyreg",
                         list_of
                             (PropertyInfo(1638401ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2149122092ul, "GbpEpGroupFromNetworkRTgt", PropertyInfo::COMPOSITE, 44, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromNetworkRTgt)
                             (PropertyInfo(2149122100ul, "GbpFloodDomainToNetworkRSrc", PropertyInfo::COMPOSITE, 52, PropertyInfo::VECTOR)) // item:mclass(gbp/FloodDomainToNetworkRSrc)
                             (PropertyInfo(2149122127ul, "GbpSubnetsFromNetworkRTgt", PropertyInfo::COMPOSITE, 79, PropertyInfo::VECTOR)) // item:mclass(gbp/SubnetsFromNetworkRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/FloodDomain/*)
                             (1638401) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/FloodDomain/*/1)
                         )
                )
                (
                    ClassInfo(51, ClassInfo::RELATIONSHIP, "GbpEpGroupToProvContractRSrc", "policyreg",
                         list_of
                             (PropertyInfo(1671171ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         list_of // item:mnamer:rule(gbp/EpGroupToProvContractRSrc/*)
                             (1671172) //item:mprop(relator/NameResolvedRelSource/targetClass) of name component item:mnamer:component(gbp/EpGroupToProvContractRSrc/*/1)
                             (1671171) //item:mprop(relator/NameResolvedRelSource/targetName) of name component item:mnamer:component(gbp/EpGroupToProvContractRSrc/*/2)
                         )
                )
                (
                    ClassInfo(52, ClassInfo::RELATIONSHIP, "GbpFloodDomainToNetworkRSrc", "policyreg",
                         list_of
                             (PropertyInfo(1703939ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/FloodDomainToNetworkRSrc/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(53, ClassInfo::REVERSE_RELATIONSHIP, "GbpEpGroupFromProvContractRTgt", "policyreg",
                         list_of
                             (PropertyInfo(1736706ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1736707ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(1736705ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         list_of // item:mnamer:rule(gbp/EpGroupFromProvContractRTgt/*)
                             (1736707) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/EpGroupFromProvContractRTgt/*/1)
                         )
                )
                (
                    ClassInfo(54, ClassInfo::REVERSE_RELATIONSHIP, "GbpFloodDomainFromNetworkRTgt", "policyreg",
                         list_of
                             (PropertyInfo(1769474ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1769475ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(1769473ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         list_of // item:mnamer:rule(gbp/FloodDomainFromNetworkRTgt/*)
                             (1769475) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/FloodDomainFromNetworkRTgt/*/1)
                         )
                )
                (
                    ClassInfo(55, ClassInfo::RESOLVER, "GbpEpGroupToProvContractRRes", "framework",
                         list_of
                             (PropertyInfo(1802242ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1802241ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/EpGroupToProvContractRRes/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(56, ClassInfo::RESOLVER, "GbpFloodDomainToNetworkRRes", "framework",
                         list_of
                             (PropertyInfo(1835010ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1835009ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/FloodDomainToNetworkRRes/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(57, ClassInfo::RELATIONSHIP, "GbpEpGroupToConsContractRSrc", "policyreg",
                         list_of
                             (PropertyInfo(1867779ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         list_of // item:mnamer:rule(gbp/EpGroupToConsContractRSrc/*)
                             (1867780) //item:mprop(relator/NameResolvedRelSource/targetClass) of name component item:mnamer:component(gbp/EpGroupToConsContractRSrc/*/1)
                             (1867779) //item:mprop(relator/NameResolvedRelSource/targetName) of name component item:mnamer:component(gbp/EpGroupToConsContractRSrc/*/2)
                         )
                )
                (
                    ClassInfo(58, ClassInfo::REVERSE_RELATIONSHIP, "GbpEpGroupFromConsContractRTgt", "policyreg",
                         list_of
                             (PropertyInfo(1900546ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1900547ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(1900545ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         list_of // item:mnamer:rule(gbp/EpGroupFromConsContractRTgt/*)
                             (1900547) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/EpGroupFromConsContractRTgt/*/1)
                         )
                )
                (
                    ClassInfo(59, ClassInfo::RESOLVER, "GbpEpGroupToConsContractRRes", "framework",
                         list_of
                             (PropertyInfo(1933314ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1933313ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/EpGroupToConsContractRRes/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(60, ClassInfo::LOCAL_ONLY, "EpdrLocalL3Ep", "policyelement",
                         list_of
                             (PropertyInfo(1966083ul, "ip", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epdr/LocalL3Ep/ip)
                             (PropertyInfo(1966082ul, "mac", PropertyInfo::MAC, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/mac)
                             (PropertyInfo(1966081ul, "uuid", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/uuid)
                             (PropertyInfo(2149449765ul, "EpdrEndPointToGroupRSrc", PropertyInfo::COMPOSITE, 37, PropertyInfo::VECTOR)) // item:mclass(epdr/EndPointToGroupRSrc)
                             ,
                         list_of // item:mnamer:rule(epdr/EndPoint/*)
                             (1966081) //item:mprop(epr/EndPoint/uuid) of name component item:mnamer:component(epdr/EndPoint/*/1)
                         )
                )
                (
                    ClassInfo(71, ClassInfo::POLICY, "GbpRoutingDomain", "policyreg",
                         list_of
                             (PropertyInfo(2326529ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2149810188ul, "GbpeInstContext", PropertyInfo::COMPOSITE, 12, PropertyInfo::VECTOR)) // item:mclass(gbpe/InstContext)
                             (PropertyInfo(2149810219ul, "GbpBridgeDomainFromNetworkRTgt", PropertyInfo::COMPOSITE, 43, PropertyInfo::VECTOR)) // item:mclass(gbp/BridgeDomainFromNetworkRTgt)
                             (PropertyInfo(2149810220ul, "GbpEpGroupFromNetworkRTgt", PropertyInfo::COMPOSITE, 44, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromNetworkRTgt)
                             (PropertyInfo(2149810255ul, "GbpSubnetsFromNetworkRTgt", PropertyInfo::COMPOSITE, 79, PropertyInfo::VECTOR)) // item:mclass(gbp/SubnetsFromNetworkRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/RoutingDomain/*)
                             (2326529) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/RoutingDomain/*/1)
                         )
                )
                (
                    ClassInfo(72, ClassInfo::POLICY, "GbpSubnet", "policyreg",
                         list_of
                             (PropertyInfo(2359297ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2149842988ul, "GbpEpGroupFromNetworkRTgt", PropertyInfo::COMPOSITE, 44, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromNetworkRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/Subnet/*)
                             (2359297) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Subnet/*/1)
                         )
                )
                (
                    ClassInfo(73, ClassInfo::POLICY, "GbpSubnets", "policyreg",
                         list_of
                             (PropertyInfo(2392065ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2149875756ul, "GbpEpGroupFromNetworkRTgt", PropertyInfo::COMPOSITE, 44, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromNetworkRTgt)
                             (PropertyInfo(2149875784ul, "GbpSubnet", PropertyInfo::COMPOSITE, 72, PropertyInfo::VECTOR)) // item:mclass(gbp/Subnet)
                             (PropertyInfo(2149875787ul, "GbpSubnetsToNetworkRSrc", PropertyInfo::COMPOSITE, 75, PropertyInfo::VECTOR)) // item:mclass(gbp/SubnetsToNetworkRSrc)
                             ,
                         list_of // item:mnamer:rule(gbp/Subnets/*)
                             (2392065) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Subnets/*/1)
                         )
                )
                (
                    ClassInfo(75, ClassInfo::RELATIONSHIP, "GbpSubnetsToNetworkRSrc", "policyreg",
                         list_of
                             (PropertyInfo(2457603ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/SubnetsToNetworkRSrc/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(78, ClassInfo::LOCAL_ONLY, "PolicyUniverse", "init",
                         list_of
                             (PropertyInfo(2150039562ul, "PlatformConfig", PropertyInfo::COMPOSITE, 10, PropertyInfo::VECTOR)) // item:mclass(platform/Config)
                             (PropertyInfo(2150039632ul, "PolicySpace", PropertyInfo::COMPOSITE, 80, PropertyInfo::VECTOR)) // item:mclass(policy/Space)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(policy/Universe/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(79, ClassInfo::REVERSE_RELATIONSHIP, "GbpSubnetsFromNetworkRTgt", "policyreg",
                         list_of
                             (PropertyInfo(2588674ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(2588675ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(2588673ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         list_of // item:mnamer:rule(gbp/SubnetsFromNetworkRTgt/*)
                             (2588675) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/SubnetsFromNetworkRTgt/*/1)
                         )
                )
                (
                    ClassInfo(80, ClassInfo::LOCAL_ONLY, "PolicySpace", "policyreg",
                         list_of
                             (PropertyInfo(2621441ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedContainer/name)
                             (PropertyInfo(2150105102ul, "GbpeL24Classifier", PropertyInfo::COMPOSITE, 14, PropertyInfo::VECTOR)) // item:mclass(gbpe/L24Classifier)
                             (PropertyInfo(2150105118ul, "GbpClassifier", PropertyInfo::COMPOSITE, 30, PropertyInfo::VECTOR)) // item:mclass(gbp/Classifier)
                             (PropertyInfo(2150105119ul, "GbpContract", PropertyInfo::COMPOSITE, 31, PropertyInfo::VECTOR)) // item:mclass(gbp/Contract)
                             (PropertyInfo(2150105120ul, "GbpEpGroup", PropertyInfo::COMPOSITE, 32, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroup)
                             (PropertyInfo(2150105124ul, "GbpBridgeDomain", PropertyInfo::COMPOSITE, 36, PropertyInfo::VECTOR)) // item:mclass(gbp/BridgeDomain)
                             (PropertyInfo(2150105138ul, "GbpFloodDomain", PropertyInfo::COMPOSITE, 50, PropertyInfo::VECTOR)) // item:mclass(gbp/FloodDomain)
                             (PropertyInfo(2150105159ul, "GbpRoutingDomain", PropertyInfo::COMPOSITE, 71, PropertyInfo::VECTOR)) // item:mclass(gbp/RoutingDomain)
                             (PropertyInfo(2150105161ul, "GbpSubnets", PropertyInfo::COMPOSITE, 73, PropertyInfo::VECTOR)) // item:mclass(gbp/Subnets)
                             ,
                         list_of // item:mnamer:rule(policy/Space/policy/Universe)
                             (2621441) //item:mprop(policy/NamedContainer/name) of name component item:mnamer:component(policy/Space/policy/Universe/1)
                         )
                )
                (
                    ClassInfo(81, ClassInfo::RESOLVER, "GbpSubnetsToNetworkRRes", "framework",
                         list_of
                             (PropertyInfo(2654210ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(2654209ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorTypeT",
                                     list_of
                                             (ConstInfo("direct-association", 1)) // item:mconst(relator/Type/direct-association)
                                             (ConstInfo("direct-dependency", 3)) // item:mconst(relator/Type/direct-dependency)
                                             (ConstInfo("named-association", 2)) // item:mconst(relator/Type/named-association)
                                             (ConstInfo("named-dependency", 4)) // item:mconst(relator/Type/named-dependency)
                                             (ConstInfo("reference", 8)) // item:mconst(relator/Type/reference)
                                     ))) // item:mprop(relator/Item/type)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/SubnetsToNetworkRRes/*); assume cardinality of 1
                         )
                )
        );
    return metadata;
}
} // namespace opmodelgbp
