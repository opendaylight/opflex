/**
 * 
 * SOME COPYRIGHT
 * 
 * Rule.hpp
 * 
 * generated Rule.hpp file genie code generation framework free of license.
 *  
 */
#pragma once
#ifndef GI_GBP_RULE_HPP
#define GI_GBP_RULE_HPP

#include <boost/optional.hpp>
#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"
/*
 * contains: item:mclass(gbp/RuleToClassifierRSrc)
 */
#include "opmodelgbp/gbp/RuleToClassifierRSrc.hpp"

namespace opmodelgbp {
namespace gbp {

class Rule
    : public opflex::modb::mointernal::MO
{
public:

    /**
     * The unique class ID for Rule
     */
    static const opflex::modb::class_id_t CLASS_ID = 32;

    /**
     * Check whether name has been set
     * @return true if name has been set
     */
    bool isNameSet()
    {
        return getObjectInstance().isSet(1048577ul, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of name if it has been set.
     * @return the value of name or boost::none if not set
     */
    boost::optional<const std::string&> getName()
    {
        if (isNameSet())
            return getObjectInstance().getString(1048577ul);
        return boost::none;
    }

    /**
     * Get the value of name if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of name if set, otherwise the value of default passed in
     */
    const std::string& getName(const std::string& defaultValue)
    {
        return getName().get_value_or(defaultValue);
    }

    /**
     * Set name to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::Rule& setName(const std::string& newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setString(1048577ul, newValue);
        return *this;
    }

    /**
     * Unset name in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::Rule& unsetName()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(1048577ul, opflex::modb::PropertyInfo::STRING, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether order has been set
     * @return true if order has been set
     */
    bool isOrderSet()
    {
        return getObjectInstance().isSet(1048578ul, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of order if it has been set.
     * @return the value of order or boost::none if not set
     */
    boost::optional<uint32_t> getOrder()
    {
        if (isOrderSet())
            return (uint32_t)getObjectInstance().getUInt64(1048578ul);
        return boost::none;
    }

    /**
     * Get the value of order if set, otherwise the value of default passed in.
     * @param defaultValue default value returned if the property is not set
     * @return the value of order if set, otherwise the value of default passed in
     */
    uint32_t getOrder(uint32_t defaultValue)
    {
        return getOrder().get_value_or(defaultValue);
    }

    /**
     * Set order to the specified value in the currently-active mutator.
     * 
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::Rule& setOrder(uint32_t newValue)
    {
        getTLMutator().modify(getClassId(), getURI())->setUInt64(1048578ul, newValue);
        return *this;
    }

    /**
     * Unset order in the currently-active mutator.
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    opmodelgbp::gbp::Rule& unsetOrder()
    {
        getTLMutator().modify(getClassId(), getURI())->unset(1048578ul, opflex::modb::PropertyInfo::U64, opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Retrieve an instance of Rule from the managed
     * object store.  If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::Rule> > resolve(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::gbp::Rule>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of Rule from the managed
     * object store using the default framework instance.  If the 
     * object does not exist in the local store, returns boost::none. 
     * Note that even though it may not exist locally, it may still 
     * exist remotely.
     * 
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::Rule> > resolve(
        const opflex::modb::URI& uri)
    {
        return opflex::modb::mointernal::MO::resolve<opmodelgbp::gbp::Rule>(opflex::ofcore::OFFramework::defaultInstance(), CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of Rule from the managed
     * object store by constructing its URI from the path elements
     * that lead to it.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpContract/[gbpContractName]/GbpSubject/[gbpSubjectName]/GbpRule/[gbpRuleName]
     * 
     * @param framework the framework instance to use 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpContractName the value of gbpContractName,
     * a naming property for Contract
     * @param gbpSubjectName the value of gbpSubjectName,
     * a naming property for Subject
     * @param gbpRuleName the value of gbpRuleName,
     * a naming property for Rule
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::Rule> > resolve(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpContractName,
        const std::string& gbpSubjectName,
        const std::string& gbpRuleName)
    {
        return resolve(framework,opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpContract").addElement(gbpContractName).addElement("GbpSubject").addElement(gbpSubjectName).addElement("GbpRule").addElement(gbpRuleName).build());
    }

    /**
     * Retrieve an instance of Rule from the 
     * default managed object store by constructing its URI from the
     * path elements that lead to it.  If the object does not exist in
     * the local store, returns boost::none.  Note that even though it
     * may not exist locally, it may still exist remotely.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpContract/[gbpContractName]/GbpSubject/[gbpSubjectName]/GbpRule/[gbpRuleName]
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpContractName the value of gbpContractName,
     * a naming property for Contract
     * @param gbpSubjectName the value of gbpSubjectName,
     * a naming property for Subject
     * @param gbpRuleName the value of gbpRuleName,
     * a naming property for Rule
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<boost::shared_ptr<opmodelgbp::gbp::Rule> > resolve(
        const std::string& policySpaceName,
        const std::string& gbpContractName,
        const std::string& gbpSubjectName,
        const std::string& gbpRuleName)
    {
        return resolve(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpContractName,gbpSubjectName,gbpRuleName);
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     * 
     * @param gbpRuleToClassifierRSrcTargetClass the value of gbpRuleToClassifierRSrcTargetClass,
     * a naming property for RuleToClassifierRSrc
     * @param gbpRuleToClassifierRSrcTargetName the value of gbpRuleToClassifierRSrcTargetName,
     * a naming property for RuleToClassifierRSrc
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    boost::optional<boost::shared_ptr<opmodelgbp::gbp::RuleToClassifierRSrc> > resolveGbpRuleToClassifierRSrc(
        const std::string& gbpRuleToClassifierRSrcTargetName)
    {
        opflex::modb::class_id_t gbpRuleToClassifierRSrcTargetClass = 21;
        return opmodelgbp::gbp::RuleToClassifierRSrc::resolve(getFramework(), opflex::modb::URIBuilder(getURI()).addElement("GbpRuleToClassifierRSrc").addElement(gbpRuleToClassifierRSrcTargetClass).addElement(gbpRuleToClassifierRSrcTargetName).build());
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     * 
     * @param gbpRuleToClassifierRSrcTargetClass the value of gbpRuleToClassifierRSrcTargetClass,
     * a naming property for RuleToClassifierRSrc
     * @param gbpRuleToClassifierRSrcTargetName the value of gbpRuleToClassifierRSrcTargetName,
     * a naming property for RuleToClassifierRSrc
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    boost::shared_ptr<opmodelgbp::gbp::RuleToClassifierRSrc> addGbpRuleToClassifierRSrc(
        const std::string& gbpRuleToClassifierRSrcTargetName)
    {
        opflex::modb::class_id_t gbpRuleToClassifierRSrcTargetClass = 21;
        boost::shared_ptr<opmodelgbp::gbp::RuleToClassifierRSrc> result = addChild<opmodelgbp::gbp::RuleToClassifierRSrc>(
            CLASS_ID, getURI(), 2148532258ul, 34,
            opflex::modb::URIBuilder(getURI()).addElement("GbpRuleToClassifierRSrc").addElement(gbpRuleToClassifierRSrcTargetClass).addElement(gbpRuleToClassifierRSrcTargetName).build()
            );
        result->setTargetClassifier(opflex::modb::URI(gbpRuleToClassifierRSrcTargetName));
        return result;
    }

    /**
     * Resolve and retrieve all of the immediate children of type
     * opmodelgbp::gbp::RuleToClassifierRSrc
     * 
     * Note that this retrieves only those children that exist in the
     * local store.  It is possible that there are other children that
     * exist remotely.
     * 
     * The resulting managed objects will be added to the result
     * vector provided.
     * 
     * @param out a reference to a vector that will receive the child
     * objects.
     */
    void resolveGbpRuleToClassifierRSrc(/* out */ std::vector<boost::shared_ptr<opmodelgbp::gbp::RuleToClassifierRSrc> >& out)
    {
        opflex::modb::mointernal::MO::resolveChildren<opmodelgbp::gbp::RuleToClassifierRSrc>(
            getFramework(), CLASS_ID, getURI(), 2148532258ul, 34, out);
    }

    /**
     * Remove this instance using the currently-active mutator.  If
     * the object does not exist, then this will be a no-op.  If this
     * object has any children, they will be garbage-collected at some
     * future time.
     * 
     * @throws std::logic_error if no mutator is active
     */
    void remove()
    {
        getTLMutator().remove(CLASS_ID, getURI());
    }

    /**
     * Remove the Rule object with the specified URI
     * using the currently-active mutator.  If the object does not exist,
     * then this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * @param framework the framework instance to use
     * @param uri the URI of the object to remove
     * @throws std::logic_error if no mutator is active
     */
    static void remove(opflex::ofcore::OFFramework& framework,
                       const opflex::modb::URI& uri)
    {
        MO::remove(framework, CLASS_ID, uri);
    }

    /**
     * Remove the Rule object with the specified URI 
     * using the currently-active mutator and the default framework 
     * instance.  If the object does not exist, then this will be a
     * no-op.  If this object has any children, they will be 
     * garbage-collected at some future time.
     * 
     * @param uri the URI of the object to remove
     * @throws std::logic_error if no mutator is active
     */
    static void remove(const opflex::modb::URI& uri)
    {
        remove(opflex::ofcore::OFFramework::defaultInstance(), uri);
    }

    /**
     * Remove the Rule object with the specified path
     * elements from the managed object store.  If the object does
     * not exist, then this will be a no-op.  If this object has any
     * children, they will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpContract/[gbpContractName]/GbpSubject/[gbpSubjectName]/GbpRule/[gbpRuleName]
     * 
     * @param framework the framework instance to use
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpContractName the value of gbpContractName,
     * a naming property for Contract
     * @param gbpSubjectName the value of gbpSubjectName,
     * a naming property for Subject
     * @param gbpRuleName the value of gbpRuleName,
     * a naming property for Rule
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        opflex::ofcore::OFFramework& framework,
        const std::string& policySpaceName,
        const std::string& gbpContractName,
        const std::string& gbpSubjectName,
        const std::string& gbpRuleName)
    {
        MO::remove(framework, CLASS_ID, opflex::modb::URIBuilder().addElement("PolicyUniverse").addElement("PolicySpace").addElement(policySpaceName).addElement("GbpContract").addElement(gbpContractName).addElement("GbpSubject").addElement(gbpSubjectName).addElement("GbpRule").addElement(gbpRuleName).build());
    }

    /**
     * Remove the Rule object with the specified path
     * elements from the managed object store using the default
     * framework instance.  If the object does not exist, then
     * this will be a no-op.  If this object has any children, they
     * will be garbage-collected at some future time.
     * 
     * The object URI generated by this function will take the form:
     * /PolicyUniverse/PolicySpace/[policySpaceName]/GbpContract/[gbpContractName]/GbpSubject/[gbpSubjectName]/GbpRule/[gbpRuleName]
     * 
     * @param policySpaceName the value of policySpaceName,
     * a naming property for Space
     * @param gbpContractName the value of gbpContractName,
     * a naming property for Contract
     * @param gbpSubjectName the value of gbpSubjectName,
     * a naming property for Subject
     * @param gbpRuleName the value of gbpRuleName,
     * a naming property for Rule
     * @throws std::logic_error if no mutator is active
     */
    static void remove(
        const std::string& policySpaceName,
        const std::string& gbpContractName,
        const std::string& gbpSubjectName,
        const std::string& gbpRuleName)
    {
        remove(opflex::ofcore::OFFramework::defaultInstance(),policySpaceName,gbpContractName,gbpSubjectName,gbpRuleName);
    }

    /**
     * Register a listener that will get called for changes related to
     * this class.  This listener will be called for any modifications
     * of this class or any transitive children of this class.
     * 
     * @param framework the framework instance 
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     */
    static void registerListener(
        opflex::ofcore::OFFramework& framework,
        opflex::modb::ObjectListener* listener)
    {
        opflex::modb::mointernal
            ::MO::registerListener(framework, listener, CLASS_ID);
    }

    /**
     * Register a listener that will get called for changes related to
     * this class with the default framework instance.  This listener
     * will be called for any modifications of this class or any
     * transitive children of this class.
     * 
     * @param listener the listener functional object that should be
     * called when changes occur related to the class.  This memory is
     * owned by the caller and should be freed only after it has been
     * unregistered.
     */
    static void registerListener(
        opflex::modb::ObjectListener* listener)
    {
        registerListener(opflex::ofcore::OFFramework::defaultInstance(), listener);
    }

    /**
     * Unregister a listener from updates to this class.
     * 
     * @param framework the framework instance 
     * @param listener The listener to unregister.
     */
    static void unregisterListener(
        opflex::ofcore::OFFramework& framework,
        opflex::modb::ObjectListener* listener)
    {
        opflex::modb::mointernal
            ::MO::unregisterListener(framework, listener, CLASS_ID);
    }

    /**
     * Unregister a listener from updates to this class from the
     * default framework instance
     * 
     * @param listener The listener to unregister.
     */
    static void unregisterListener(
        opflex::modb::ObjectListener* listener)
    {
        unregisterListener(opflex::ofcore::OFFramework::defaultInstance(), listener);
    }

    /**
     * Construct an instance of Rule.
     * This should not typically be called from user code.
     */
    Rule(
        opflex::ofcore::OFFramework& framework,
        const opflex::modb::URI& uri,
        const boost::shared_ptr<const opflex::modb::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
}; // class Rule

} // namespace gbp
} // namespace opmodelgbp
#endif // GI_GBP_RULE_HPP
