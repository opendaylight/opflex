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
                             (PropertyInfo(2147516425ul, "EpdrL2Discovered", PropertyInfo::COMPOSITE, 9, PropertyInfo::VECTOR)) // item:mclass(epdr/L2Discovered)
                             (PropertyInfo(2147516426ul, "EpdrL3Discovered", PropertyInfo::COMPOSITE, 10, PropertyInfo::VECTOR)) // item:mclass(epdr/L3Discovered)
                             (PropertyInfo(2147516429ul, "EprL2Universe", PropertyInfo::COMPOSITE, 13, PropertyInfo::VECTOR)) // item:mclass(epr/L2Universe)
                             (PropertyInfo(2147516430ul, "EprL3Universe", PropertyInfo::COMPOSITE, 14, PropertyInfo::VECTOR)) // item:mclass(epr/L3Universe)
                             (PropertyInfo(2147516490ul, "PolicyUniverse", PropertyInfo::COMPOSITE, 74, PropertyInfo::VECTOR)) // item:mclass(policy/Universe)
                             ,
                         std::vector<prop_id_t>() // no naming rule; assume cardinality of 1 in any containment rule
                         )
                )
                (
                    ClassInfo(7, ClassInfo::LOCAL_ONLY, "RelatorUniverse", "init",
                         list_of
                             (PropertyInfo(2147713051ul, "EpdrEndPointToGroupRRes", PropertyInfo::COMPOSITE, 27, PropertyInfo::VECTOR)) // item:mclass(epdr/EndPointToGroupRRes)
                             (PropertyInfo(2147713057ul, "GbpEpGroupToNetworkRRes", PropertyInfo::COMPOSITE, 33, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupToNetworkRRes)
                             (PropertyInfo(2147713060ul, "GbpRuleToClassifierRRes", PropertyInfo::COMPOSITE, 36, PropertyInfo::VECTOR)) // item:mclass(gbp/RuleToClassifierRRes)
                             (PropertyInfo(2147713065ul, "GbpEpGroupToProvContractRRes", PropertyInfo::COMPOSITE, 41, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupToProvContractRRes)
                             (PropertyInfo(2147713074ul, "GbpEpGroupToConsContractRRes", PropertyInfo::COMPOSITE, 50, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupToConsContractRRes)
                             (PropertyInfo(2147713076ul, "GbpBridgeDomainToNetworkRRes", PropertyInfo::COMPOSITE, 52, PropertyInfo::VECTOR)) // item:mclass(gbp/BridgeDomainToNetworkRRes)
                             (PropertyInfo(2147713086ul, "GbpFloodDomainToNetworkRRes", PropertyInfo::COMPOSITE, 62, PropertyInfo::VECTOR)) // item:mclass(gbp/FloodDomainToNetworkRRes)
                             (PropertyInfo(2147713094ul, "GbpSubnetsToNetworkRRes", PropertyInfo::COMPOSITE, 70, PropertyInfo::VECTOR)) // item:mclass(gbp/SubnetsToNetworkRRes)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(relator/Universe/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(9, ClassInfo::LOCAL_ONLY, "EpdrL2Discovered", "init",
                         list_of
                             (PropertyInfo(2147778597ul, "EpdrLocalL2Ep", PropertyInfo::COMPOSITE, 37, PropertyInfo::VECTOR)) // item:mclass(epdr/LocalL2Ep)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(epdr/L2Discovered/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(10, ClassInfo::LOCAL_ONLY, "EpdrL3Discovered", "init",
                         list_of
                             (PropertyInfo(2147811367ul, "EpdrLocalL3Ep", PropertyInfo::COMPOSITE, 39, PropertyInfo::VECTOR)) // item:mclass(epdr/LocalL3Ep)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(epdr/L3Discovered/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(13, ClassInfo::LOCAL_ENDPOINT, "EprL2Universe", "init",
                         list_of
                             (PropertyInfo(2147909650ul, "EprL2Ep", PropertyInfo::COMPOSITE, 18, PropertyInfo::VECTOR)) // item:mclass(epr/L2Ep)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(epr/L2Universe/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(14, ClassInfo::LOCAL_ENDPOINT, "EprL3Universe", "init",
                         list_of
                             (PropertyInfo(2147942420ul, "EprL3Ep", PropertyInfo::COMPOSITE, 20, PropertyInfo::VECTOR)) // item:mclass(epr/L3Ep)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(epr/L3Universe/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(18, ClassInfo::LOCAL_ENDPOINT, "EprL2Ep", "policyelement",
                         list_of
                             (PropertyInfo(589828ul, "context", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/L2Ep/context)
                             (PropertyInfo(589827ul, "group", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/ReportedEp/group)
                             (PropertyInfo(589826ul, "mac", PropertyInfo::MAC, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/mac)
                             (PropertyInfo(589825ul, "uuid", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/uuid)
                             (PropertyInfo(2148073491ul, "EprL3Net", PropertyInfo::COMPOSITE, 19, PropertyInfo::VECTOR)) // item:mclass(epr/L3Net)
                             ,
                         list_of // item:mnamer:rule(epr/L2Ep/*)
                             (589828) //item:mprop(epr/L2Ep/context) of name component item:mnamer:component(epr/L2Ep/*/1)
                             (589826) //item:mprop(epr/EndPoint/mac) of name component item:mnamer:component(epr/L2Ep/*/2)
                         )
                )
                (
                    ClassInfo(19, ClassInfo::LOCAL_ENDPOINT, "EprL3Net", "policyelement",
                         list_of
                             (PropertyInfo(622593ul, "ip", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/L3Net/ip)
                             ,
                         list_of // item:mnamer:rule(epr/L3Net/*)
                             (622593) //item:mprop(epr/L3Net/ip) of name component item:mnamer:component(epr/L3Net/*/1)
                         )
                )
                (
                    ClassInfo(20, ClassInfo::LOCAL_ENDPOINT, "EprL3Ep", "policyelement",
                         list_of
                             (PropertyInfo(655365ul, "context", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/L3Ep/context)
                             (PropertyInfo(655363ul, "group", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/ReportedEp/group)
                             (PropertyInfo(655364ul, "ip", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/L3Ep/ip)
                             (PropertyInfo(655362ul, "mac", PropertyInfo::MAC, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/mac)
                             (PropertyInfo(655361ul, "uuid", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/uuid)
                             ,
                         list_of // item:mnamer:rule(epr/L3Ep/*)
                             (655365) //item:mprop(epr/L3Ep/context) of name component item:mnamer:component(epr/L3Ep/*/1)
                             (655364) //item:mprop(epr/L3Ep/ip) of name component item:mnamer:component(epr/L3Ep/*/2)
                         )
                )
                (
                    ClassInfo(21, ClassInfo::POLICY, "GbpClassifier", "policyreg",
                         list_of
                             (PropertyInfo(688131ul, "connectionTracking", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("GbpConnTrackT",
                                     list_of
                                             (ConstInfo("normal", 0)) // item:mconst(gbp/ConnTrack/normal)
                                             (ConstInfo("reflexive", 1)) // item:mconst(gbp/ConnTrack/reflexive)
                                     ))) // item:mprop(gbp/Classifier/connectionTracking)
                             (PropertyInfo(688132ul, "direction", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("GbpDirectionT",
                                     list_of
                                             (ConstInfo("bidirectional", 0)) // item:mconst(gbp/Direction/bidirectional)
                                             (ConstInfo("in", 1)) // item:mconst(gbp/Direction/in)
                                             (ConstInfo("out", 2)) // item:mconst(gbp/Direction/out)
                                     ))) // item:mprop(gbp/Classifier/direction)
                             (PropertyInfo(688129ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(688130ul, "order", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbp/Classifier/order)
                             (PropertyInfo(2148171811ul, "GbpRuleFromClassifierRTgt", PropertyInfo::COMPOSITE, 35, PropertyInfo::VECTOR)) // item:mclass(gbp/RuleFromClassifierRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/Classifier/*)
                             (688129) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Classifier/*/1)
                         )
                )
                (
                    ClassInfo(22, ClassInfo::RELATIONSHIP, "EpdrEndPointToGroupRSrc", "policyelement",
                         list_of
                             (PropertyInfo(720899ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(epdr/EndPointToGroupRSrc/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(23, ClassInfo::LOCAL_ONLY, "GbpeInstContext", "policyreg",
                         list_of
                             (PropertyInfo(753666ul, "classid", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/InstContext/classid)
                             (PropertyInfo(753665ul, "vnid", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/InstContext/vnid)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbpe/InstContext/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(24, ClassInfo::REVERSE_RELATIONSHIP, "EpdrEndPointFromGroupRTgt", "policyelement",
                         list_of
                             (PropertyInfo(786434ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(786435ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(786433ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                             (786435) //item:mprop(relator/Target/source) of name component item:mnamer:component(epdr/EndPointFromGroupRTgt/*/1)
                         )
                )
                (
                    ClassInfo(25, ClassInfo::POLICY, "GbpEpGroup", "policyreg",
                         list_of
                             (PropertyInfo(819201ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2148302871ul, "GbpeInstContext", PropertyInfo::COMPOSITE, 23, PropertyInfo::VECTOR)) // item:mclass(gbpe/InstContext)
                             (PropertyInfo(2148302872ul, "EpdrEndPointFromGroupRTgt", PropertyInfo::COMPOSITE, 24, PropertyInfo::VECTOR)) // item:mclass(epdr/EndPointFromGroupRTgt)
                             (PropertyInfo(2148302877ul, "GbpEpGroupToNetworkRSrc", PropertyInfo::COMPOSITE, 29, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupToNetworkRSrc)
                             (PropertyInfo(2148302886ul, "GbpEpGroupToProvContractRSrc", PropertyInfo::COMPOSITE, 38, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupToProvContractRSrc)
                             (PropertyInfo(2148302891ul, "GbpEpGroupToConsContractRSrc", PropertyInfo::COMPOSITE, 43, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupToConsContractRSrc)
                             ,
                         list_of // item:mnamer:rule(gbp/EpGroup/*)
                             (819201) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/EpGroup/*/1)
                         )
                )
                (
                    ClassInfo(26, ClassInfo::POLICY, "GbpeL24Classifier", "policyreg",
                         list_of
                             (PropertyInfo(851973ul, "arpOpc", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("ArpOpcodeT",
                                     list_of
                                             (ConstInfo("reply", 2)) // item:mconst(arp/Opcode/reply)
                                             (ConstInfo("request", 1)) // item:mconst(arp/Opcode/request)
                                             (ConstInfo("unspecified", 0)) // item:mconst(arp/Opcode/unspecified)
                                     ))) // item:mprop(gbpe/L24Classifier/arpOpc)
                             (PropertyInfo(851971ul, "connectionTracking", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("GbpConnTrackT",
                                     list_of
                                             (ConstInfo("normal", 0)) // item:mconst(gbp/ConnTrack/normal)
                                             (ConstInfo("reflexive", 1)) // item:mconst(gbp/ConnTrack/reflexive)
                                     ))) // item:mprop(gbp/Classifier/connectionTracking)
                             (PropertyInfo(851978ul, "dFromPort", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/L24Classifier/dFromPort)
                             (PropertyInfo(851979ul, "dToPort", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/L24Classifier/dToPort)
                             (PropertyInfo(851972ul, "direction", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("GbpDirectionT",
                                     list_of
                                             (ConstInfo("bidirectional", 0)) // item:mconst(gbp/Direction/bidirectional)
                                             (ConstInfo("in", 1)) // item:mconst(gbp/Direction/in)
                                             (ConstInfo("out", 2)) // item:mconst(gbp/Direction/out)
                                     ))) // item:mprop(gbp/Classifier/direction)
                             (PropertyInfo(851974ul, "etherT", PropertyInfo::ENUM16, PropertyInfo::SCALAR,
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
                             (PropertyInfo(851969ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(851970ul, "order", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbp/Classifier/order)
                             (PropertyInfo(851975ul, "prot", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/L24Classifier/prot)
                             (PropertyInfo(851976ul, "sFromPort", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/L24Classifier/sFromPort)
                             (PropertyInfo(851977ul, "sToPort", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbpe/L24Classifier/sToPort)
                             (PropertyInfo(2148335651ul, "GbpRuleFromClassifierRTgt", PropertyInfo::COMPOSITE, 35, PropertyInfo::VECTOR)) // item:mclass(gbp/RuleFromClassifierRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/Classifier/*)
                             (851969) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Classifier/*/1)
                         )
                )
                (
                    ClassInfo(27, ClassInfo::RESOLVER, "EpdrEndPointToGroupRRes", "framework",
                         list_of
                             (PropertyInfo(884738ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(884737ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                    ClassInfo(28, ClassInfo::POLICY, "GbpContract", "policyreg",
                         list_of
                             (PropertyInfo(917505ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2148401182ul, "GbpSubject", PropertyInfo::COMPOSITE, 30, PropertyInfo::VECTOR)) // item:mclass(gbp/Subject)
                             (PropertyInfo(2148401192ul, "GbpEpGroupFromProvContractRTgt", PropertyInfo::COMPOSITE, 40, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromProvContractRTgt)
                             (PropertyInfo(2148401198ul, "GbpEpGroupFromConsContractRTgt", PropertyInfo::COMPOSITE, 46, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromConsContractRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/Contract/*)
                             (917505) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Contract/*/1)
                         )
                )
                (
                    ClassInfo(29, ClassInfo::RELATIONSHIP, "GbpEpGroupToNetworkRSrc", "policyreg",
                         list_of
                             (PropertyInfo(950275ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/EpGroupToNetworkRSrc/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(30, ClassInfo::POLICY, "GbpSubject", "policyreg",
                         list_of
                             (PropertyInfo(983041ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2148466720ul, "GbpRule", PropertyInfo::COMPOSITE, 32, PropertyInfo::VECTOR)) // item:mclass(gbp/Rule)
                             ,
                         list_of // item:mnamer:rule(gbp/Subject/*)
                             (983041) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Subject/*/1)
                         )
                )
                (
                    ClassInfo(31, ClassInfo::REVERSE_RELATIONSHIP, "GbpEpGroupFromNetworkRTgt", "policyreg",
                         list_of
                             (PropertyInfo(1015810ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1015811ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(1015809ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                             (1015811) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/EpGroupFromNetworkRTgt/*/1)
                         )
                )
                (
                    ClassInfo(32, ClassInfo::POLICY, "GbpRule", "policyreg",
                         list_of
                             (PropertyInfo(1048577ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(1048578ul, "order", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(gbp/Rule/order)
                             (PropertyInfo(2148532258ul, "GbpRuleToClassifierRSrc", PropertyInfo::COMPOSITE, 34, PropertyInfo::VECTOR)) // item:mclass(gbp/RuleToClassifierRSrc)
                             ,
                         list_of // item:mnamer:rule(gbp/Rule/*)
                             (1048577) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Rule/*/1)
                         )
                )
                (
                    ClassInfo(33, ClassInfo::RESOLVER, "GbpEpGroupToNetworkRRes", "framework",
                         list_of
                             (PropertyInfo(1081346ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1081345ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                    ClassInfo(34, ClassInfo::RELATIONSHIP, "GbpRuleToClassifierRSrc", "policyreg",
                         list_of
                             (PropertyInfo(1114115ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         list_of // item:mnamer:rule(gbp/RuleToClassifierRSrc/*)
                             (1114116) //item:mprop(relator/NameResolvedRelSource/targetClass) of name component item:mnamer:component(gbp/RuleToClassifierRSrc/*/1)
                             (1114115) //item:mprop(relator/NameResolvedRelSource/targetName) of name component item:mnamer:component(gbp/RuleToClassifierRSrc/*/2)
                         )
                )
                (
                    ClassInfo(35, ClassInfo::REVERSE_RELATIONSHIP, "GbpRuleFromClassifierRTgt", "policyreg",
                         list_of
                             (PropertyInfo(1146882ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1146883ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(1146881ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                             (1146883) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/RuleFromClassifierRTgt/*/1)
                         )
                )
                (
                    ClassInfo(36, ClassInfo::RESOLVER, "GbpRuleToClassifierRRes", "framework",
                         list_of
                             (PropertyInfo(1179650ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1179649ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                    ClassInfo(37, ClassInfo::LOCAL_ONLY, "EpdrLocalL2Ep", "policyelement",
                         list_of
                             (PropertyInfo(1212418ul, "mac", PropertyInfo::MAC, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/mac)
                             (PropertyInfo(1212417ul, "uuid", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/uuid)
                             (PropertyInfo(2148696086ul, "EpdrEndPointToGroupRSrc", PropertyInfo::COMPOSITE, 22, PropertyInfo::VECTOR)) // item:mclass(epdr/EndPointToGroupRSrc)
                             ,
                         list_of // item:mnamer:rule(epdr/EndPoint/*)
                             (1212417) //item:mprop(epr/EndPoint/uuid) of name component item:mnamer:component(epdr/EndPoint/*/1)
                         )
                )
                (
                    ClassInfo(38, ClassInfo::RELATIONSHIP, "GbpEpGroupToProvContractRSrc", "policyreg",
                         list_of
                             (PropertyInfo(1245187ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         list_of // item:mnamer:rule(gbp/EpGroupToProvContractRSrc/*)
                             (1245188) //item:mprop(relator/NameResolvedRelSource/targetClass) of name component item:mnamer:component(gbp/EpGroupToProvContractRSrc/*/1)
                             (1245187) //item:mprop(relator/NameResolvedRelSource/targetName) of name component item:mnamer:component(gbp/EpGroupToProvContractRSrc/*/2)
                         )
                )
                (
                    ClassInfo(39, ClassInfo::LOCAL_ONLY, "EpdrLocalL3Ep", "policyelement",
                         list_of
                             (PropertyInfo(1277955ul, "ip", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epdr/LocalL3Ep/ip)
                             (PropertyInfo(1277954ul, "mac", PropertyInfo::MAC, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/mac)
                             (PropertyInfo(1277953ul, "uuid", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(epr/EndPoint/uuid)
                             (PropertyInfo(2148761622ul, "EpdrEndPointToGroupRSrc", PropertyInfo::COMPOSITE, 22, PropertyInfo::VECTOR)) // item:mclass(epdr/EndPointToGroupRSrc)
                             ,
                         list_of // item:mnamer:rule(epdr/EndPoint/*)
                             (1277953) //item:mprop(epr/EndPoint/uuid) of name component item:mnamer:component(epdr/EndPoint/*/1)
                         )
                )
                (
                    ClassInfo(40, ClassInfo::REVERSE_RELATIONSHIP, "GbpEpGroupFromProvContractRTgt", "policyreg",
                         list_of
                             (PropertyInfo(1310722ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1310723ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(1310721ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                             (1310723) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/EpGroupFromProvContractRTgt/*/1)
                         )
                )
                (
                    ClassInfo(41, ClassInfo::RESOLVER, "GbpEpGroupToProvContractRRes", "framework",
                         list_of
                             (PropertyInfo(1343490ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
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
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/EpGroupToProvContractRRes/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(43, ClassInfo::RELATIONSHIP, "GbpEpGroupToConsContractRSrc", "policyreg",
                         list_of
                             (PropertyInfo(1409027ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         list_of // item:mnamer:rule(gbp/EpGroupToConsContractRSrc/*)
                             (1409028) //item:mprop(relator/NameResolvedRelSource/targetClass) of name component item:mnamer:component(gbp/EpGroupToConsContractRSrc/*/1)
                             (1409027) //item:mprop(relator/NameResolvedRelSource/targetName) of name component item:mnamer:component(gbp/EpGroupToConsContractRSrc/*/2)
                         )
                )
                (
                    ClassInfo(44, ClassInfo::POLICY, "GbpBridgeDomain", "policyreg",
                         list_of
                             (PropertyInfo(1441793ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2148925463ul, "GbpeInstContext", PropertyInfo::COMPOSITE, 23, PropertyInfo::VECTOR)) // item:mclass(gbpe/InstContext)
                             (PropertyInfo(2148925471ul, "GbpEpGroupFromNetworkRTgt", PropertyInfo::COMPOSITE, 31, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromNetworkRTgt)
                             (PropertyInfo(2148925488ul, "GbpBridgeDomainToNetworkRSrc", PropertyInfo::COMPOSITE, 48, PropertyInfo::VECTOR)) // item:mclass(gbp/BridgeDomainToNetworkRSrc)
                             (PropertyInfo(2148925498ul, "GbpFloodDomainFromNetworkRTgt", PropertyInfo::COMPOSITE, 58, PropertyInfo::VECTOR)) // item:mclass(gbp/FloodDomainFromNetworkRTgt)
                             (PropertyInfo(2148925509ul, "GbpSubnetsFromNetworkRTgt", PropertyInfo::COMPOSITE, 69, PropertyInfo::VECTOR)) // item:mclass(gbp/SubnetsFromNetworkRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/BridgeDomain/*)
                             (1441793) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/BridgeDomain/*/1)
                         )
                )
                (
                    ClassInfo(46, ClassInfo::REVERSE_RELATIONSHIP, "GbpEpGroupFromConsContractRTgt", "policyreg",
                         list_of
                             (PropertyInfo(1507330ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1507331ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
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
                         list_of // item:mnamer:rule(gbp/EpGroupFromConsContractRTgt/*)
                             (1507331) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/EpGroupFromConsContractRTgt/*/1)
                         )
                )
                (
                    ClassInfo(48, ClassInfo::RELATIONSHIP, "GbpBridgeDomainToNetworkRSrc", "policyreg",
                         list_of
                             (PropertyInfo(1572867ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/BridgeDomainToNetworkRSrc/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(50, ClassInfo::RESOLVER, "GbpEpGroupToConsContractRRes", "framework",
                         list_of
                             (PropertyInfo(1638402ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1638401ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                    ClassInfo(51, ClassInfo::REVERSE_RELATIONSHIP, "GbpBridgeDomainFromNetworkRTgt", "policyreg",
                         list_of
                             (PropertyInfo(1671170ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1671171ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(1671169ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                             (1671171) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/BridgeDomainFromNetworkRTgt/*/1)
                         )
                )
                (
                    ClassInfo(52, ClassInfo::RESOLVER, "GbpBridgeDomainToNetworkRRes", "framework",
                         list_of
                             (PropertyInfo(1703938ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(1703937ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                    ClassInfo(53, ClassInfo::POLICY, "GbpFloodDomain", "policyreg",
                         list_of
                             (PropertyInfo(1736705ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2149220383ul, "GbpEpGroupFromNetworkRTgt", PropertyInfo::COMPOSITE, 31, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromNetworkRTgt)
                             (PropertyInfo(2149220406ul, "GbpFloodDomainToNetworkRSrc", PropertyInfo::COMPOSITE, 54, PropertyInfo::VECTOR)) // item:mclass(gbp/FloodDomainToNetworkRSrc)
                             (PropertyInfo(2149220421ul, "GbpSubnetsFromNetworkRTgt", PropertyInfo::COMPOSITE, 69, PropertyInfo::VECTOR)) // item:mclass(gbp/SubnetsFromNetworkRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/FloodDomain/*)
                             (1736705) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/FloodDomain/*/1)
                         )
                )
                (
                    ClassInfo(54, ClassInfo::RELATIONSHIP, "GbpFloodDomainToNetworkRSrc", "policyreg",
                         list_of
                             (PropertyInfo(1769475ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/FloodDomainToNetworkRSrc/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(58, ClassInfo::REVERSE_RELATIONSHIP, "GbpFloodDomainFromNetworkRTgt", "policyreg",
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
                         list_of // item:mnamer:rule(gbp/FloodDomainFromNetworkRTgt/*)
                             (1900547) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/FloodDomainFromNetworkRTgt/*/1)
                         )
                )
                (
                    ClassInfo(62, ClassInfo::RESOLVER, "GbpFloodDomainToNetworkRRes", "framework",
                         list_of
                             (PropertyInfo(2031618ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(2031617ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                    ClassInfo(64, ClassInfo::POLICY, "GbpRoutingDomain", "policyreg",
                         list_of
                             (PropertyInfo(2097153ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2149580823ul, "GbpeInstContext", PropertyInfo::COMPOSITE, 23, PropertyInfo::VECTOR)) // item:mclass(gbpe/InstContext)
                             (PropertyInfo(2149580831ul, "GbpEpGroupFromNetworkRTgt", PropertyInfo::COMPOSITE, 31, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromNetworkRTgt)
                             (PropertyInfo(2149580851ul, "GbpBridgeDomainFromNetworkRTgt", PropertyInfo::COMPOSITE, 51, PropertyInfo::VECTOR)) // item:mclass(gbp/BridgeDomainFromNetworkRTgt)
                             (PropertyInfo(2149580869ul, "GbpSubnetsFromNetworkRTgt", PropertyInfo::COMPOSITE, 69, PropertyInfo::VECTOR)) // item:mclass(gbp/SubnetsFromNetworkRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/RoutingDomain/*)
                             (2097153) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/RoutingDomain/*/1)
                         )
                )
                (
                    ClassInfo(65, ClassInfo::POLICY, "GbpSubnet", "policyreg",
                         list_of
                             (PropertyInfo(2129921ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2149613599ul, "GbpEpGroupFromNetworkRTgt", PropertyInfo::COMPOSITE, 31, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromNetworkRTgt)
                             ,
                         list_of // item:mnamer:rule(gbp/Subnet/*)
                             (2129921) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Subnet/*/1)
                         )
                )
                (
                    ClassInfo(66, ClassInfo::POLICY, "GbpSubnets", "policyreg",
                         list_of
                             (PropertyInfo(2162689ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2149646367ul, "GbpEpGroupFromNetworkRTgt", PropertyInfo::COMPOSITE, 31, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroupFromNetworkRTgt)
                             (PropertyInfo(2149646401ul, "GbpSubnet", PropertyInfo::COMPOSITE, 65, PropertyInfo::VECTOR)) // item:mclass(gbp/Subnet)
                             (PropertyInfo(2149646403ul, "GbpSubnetsToNetworkRSrc", PropertyInfo::COMPOSITE, 67, PropertyInfo::VECTOR)) // item:mclass(gbp/SubnetsToNetworkRSrc)
                             ,
                         list_of // item:mnamer:rule(gbp/Subnets/*)
                             (2162689) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(gbp/Subnets/*/1)
                         )
                )
                (
                    ClassInfo(67, ClassInfo::RELATIONSHIP, "GbpSubnetsToNetworkRSrc", "policyreg",
                         list_of
                             (PropertyInfo(2195459ul, "target", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/NameResolvedRelSource/targetName)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(gbp/SubnetsToNetworkRSrc/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(68, ClassInfo::LOCAL_ONLY, "CdpConfig", "policyreg",
                         list_of
                             (PropertyInfo(2228225ul, "state", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                    ClassInfo(69, ClassInfo::REVERSE_RELATIONSHIP, "GbpSubnetsFromNetworkRTgt", "policyreg",
                         list_of
                             (PropertyInfo(2260994ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(2260995ul, "source", PropertyInfo::REFERENCE, PropertyInfo::SCALAR)) // item:mprop(relator/Target/source)
                             (PropertyInfo(2260993ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                             (2260995) //item:mprop(relator/Target/source) of name component item:mnamer:component(gbp/SubnetsFromNetworkRTgt/*/1)
                         )
                )
                (
                    ClassInfo(70, ClassInfo::RESOLVER, "GbpSubnetsToNetworkRRes", "framework",
                         list_of
                             (PropertyInfo(2293762ul, "role", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("RelatorRoleT",
                                     list_of
                                             (ConstInfo("resolver", 4)) // item:mconst(relator/Role/resolver)
                                             (ConstInfo("source", 1)) // item:mconst(relator/Role/source)
                                             (ConstInfo("target", 2)) // item:mconst(relator/Role/target)
                                     ))) // item:mprop(relator/Item/role)
                             (PropertyInfo(2293761ul, "type", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                (
                    ClassInfo(74, ClassInfo::LOCAL_ONLY, "PolicyUniverse", "init",
                         list_of
                             (PropertyInfo(2149908555ul, "PlatformConfig", PropertyInfo::COMPOSITE, 75, PropertyInfo::VECTOR)) // item:mclass(platform/Config)
                             (PropertyInfo(2149908556ul, "PolicySpace", PropertyInfo::COMPOSITE, 76, PropertyInfo::VECTOR)) // item:mclass(policy/Space)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(policy/Universe/*); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(75, ClassInfo::POLICY, "PlatformConfig", "policyreg",
                         list_of
                             (PropertyInfo(2457601ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedDefinition/name)
                             (PropertyInfo(2149941316ul, "CdpConfig", PropertyInfo::COMPOSITE, 68, PropertyInfo::VECTOR)) // item:mclass(cdp/Config)
                             (PropertyInfo(2149941326ul, "L2Config", PropertyInfo::COMPOSITE, 78, PropertyInfo::VECTOR)) // item:mclass(l2/Config)
                             (PropertyInfo(2149941327ul, "LldpConfig", PropertyInfo::COMPOSITE, 79, PropertyInfo::VECTOR)) // item:mclass(lldp/Config)
                             (PropertyInfo(2149941328ul, "LacpConfig", PropertyInfo::COMPOSITE, 80, PropertyInfo::VECTOR)) // item:mclass(lacp/Config)
                             (PropertyInfo(2149941329ul, "StpConfig", PropertyInfo::COMPOSITE, 81, PropertyInfo::VECTOR)) // item:mclass(stp/Config)
                             ,
                         list_of // item:mnamer:rule(platform/Config/policy/Universe)
                             (2457601) //item:mprop(policy/NamedDefinition/name) of name component item:mnamer:component(platform/Config/policy/Universe/1)
                         )
                )
                (
                    ClassInfo(76, ClassInfo::LOCAL_ONLY, "PolicySpace", "policyreg",
                         list_of
                             (PropertyInfo(2490369ul, "name", PropertyInfo::STRING, PropertyInfo::SCALAR)) // item:mprop(policy/NamedContainer/name)
                             (PropertyInfo(2149974037ul, "GbpClassifier", PropertyInfo::COMPOSITE, 21, PropertyInfo::VECTOR)) // item:mclass(gbp/Classifier)
                             (PropertyInfo(2149974041ul, "GbpEpGroup", PropertyInfo::COMPOSITE, 25, PropertyInfo::VECTOR)) // item:mclass(gbp/EpGroup)
                             (PropertyInfo(2149974042ul, "GbpeL24Classifier", PropertyInfo::COMPOSITE, 26, PropertyInfo::VECTOR)) // item:mclass(gbpe/L24Classifier)
                             (PropertyInfo(2149974044ul, "GbpContract", PropertyInfo::COMPOSITE, 28, PropertyInfo::VECTOR)) // item:mclass(gbp/Contract)
                             (PropertyInfo(2149974060ul, "GbpBridgeDomain", PropertyInfo::COMPOSITE, 44, PropertyInfo::VECTOR)) // item:mclass(gbp/BridgeDomain)
                             (PropertyInfo(2149974069ul, "GbpFloodDomain", PropertyInfo::COMPOSITE, 53, PropertyInfo::VECTOR)) // item:mclass(gbp/FloodDomain)
                             (PropertyInfo(2149974080ul, "GbpRoutingDomain", PropertyInfo::COMPOSITE, 64, PropertyInfo::VECTOR)) // item:mclass(gbp/RoutingDomain)
                             (PropertyInfo(2149974082ul, "GbpSubnets", PropertyInfo::COMPOSITE, 66, PropertyInfo::VECTOR)) // item:mclass(gbp/Subnets)
                             ,
                         list_of // item:mnamer:rule(policy/Space/policy/Universe)
                             (2490369) //item:mprop(policy/NamedContainer/name) of name component item:mnamer:component(policy/Space/policy/Universe/1)
                         )
                )
                (
                    ClassInfo(78, ClassInfo::LOCAL_ONLY, "L2Config", "policyreg",
                         list_of
                             (PropertyInfo(2555905ul, "state", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(l2/Config/state)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(platform/ConfigComponent/platform/Config); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(79, ClassInfo::LOCAL_ONLY, "LldpConfig", "policyreg",
                         list_of
                             (PropertyInfo(2588673ul, "rx", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("PlatformAdminStateT",
                                     list_of
                                             (ConstInfo("off", 0)) // item:mconst(platform/AdminState/off)
                                             (ConstInfo("on", 1)) // item:mconst(platform/AdminState/on)
                                     ))) // item:mprop(lldp/Config/rx)
                             (PropertyInfo(2588674ul, "tx", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
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
                    ClassInfo(80, ClassInfo::LOCAL_ONLY, "LacpConfig", "policyreg",
                         list_of
                             (PropertyInfo(2621444ul, "controlBits", PropertyInfo::U64, PropertyInfo::SCALAR,
                                 EnumInfo("LacpControlBitsT",
                                 std::vector<ConstInfo>()
                                     ))) // item:mprop(lacp/Config/controlBits)
                             (PropertyInfo(2621442ul, "maxLinks", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(lacp/Config/maxLinks)
                             (PropertyInfo(2621441ul, "minLinks", PropertyInfo::U64, PropertyInfo::SCALAR)) // item:mprop(lacp/Config/minLinks)
                             (PropertyInfo(2621443ul, "mode", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("LacpModeT",
                                 std::vector<ConstInfo>()
                                     ))) // item:mprop(lacp/Config/mode)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(platform/ConfigComponent/platform/Config); assume cardinality of 1
                         )
                )
                (
                    ClassInfo(81, ClassInfo::LOCAL_ONLY, "StpConfig", "policyreg",
                         list_of
                             (PropertyInfo(2654210ul, "bpduFilter", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("PlatformAdminStateT",
                                     list_of
                                             (ConstInfo("off", 0)) // item:mconst(platform/AdminState/off)
                                             (ConstInfo("on", 1)) // item:mconst(platform/AdminState/on)
                                     ))) // item:mprop(stp/Config/bpduFilter)
                             (PropertyInfo(2654209ul, "bpduGuard", PropertyInfo::ENUM8, PropertyInfo::SCALAR,
                                 EnumInfo("PlatformAdminStateT",
                                     list_of
                                             (ConstInfo("off", 0)) // item:mconst(platform/AdminState/off)
                                             (ConstInfo("on", 1)) // item:mconst(platform/AdminState/on)
                                     ))) // item:mprop(stp/Config/bpduGuard)
                             ,
                         std::vector<prop_id_t>() // no naming props in rule item:mnamer:rule(platform/ConfigComponent/platform/Config); assume cardinality of 1
                         )
                )
        );
    return metadata;
}
} // namespace opmodelgbp
