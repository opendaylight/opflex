/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * class3 from testmodel
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef TESTMODEL_CLASS3_H
#define TESTMODEL_CLASS3_H

#include <boost/optional.hpp>

#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace testmodel {

/**
 * @brief This is a sample class used in a test model
 *
 * This is the extended documentation for class3
 */
class class3 : public opflex::modb::mointernal::MO {
public:
    /**
     * The unique class ID for class3
     */
    static const opflex::modb::class_id_t CLASS_ID = 3;

    /**
     * Check whether prop6 has been set.  Prop6 is a signed 64-bit
     * integer scalar property.
     *
     * Prop6 documentation would go here.
     *
     * @return true if prop6 has been set
     */
    bool isProp6Set() {
        return getObjectInstance().isSet(6, opflex::modb::PropertyInfo::S64);
    }

    /**
     * Get the value of prop6 if it has been set.  Prop6 is an
     * unsigned 64-bit integer scalar property.
     *
     * Prop6 documentation would go here.
     *
     * @return the value of prop6, or boost::none if it is not set.
     */
    boost::optional<int64_t> getProp6() {
        if (isProp6Set())
            return getObjectInstance().getInt64(6);
        return boost::none;
    }

    /**
     * Get the value of prop6 if it has been set, or the specified
     * default value of prop6 has not been set.  Prop6 is a signed
     * 64-bit integer scalar property.
     *
     * Prop6 documentation would go here.
     *
     * @param defaultValue the default value to return of prop6 is not set.
     * @return the value of prop6 or defaultValue
     */
    int64_t getProp6(int64_t defaultValue) {
        return getProp6().get_value_or(defaultValue);
    }

    /**
     * Set prop6 to the specified value in the currently-active
     * mutator.
     *
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class3& setProp6(int64_t newValue) {
        getTLMutator().modify(CLASS_ID, getURI())->setInt64(6, newValue);
        return *this;
    }

    /**
     * Unset prop6 in the currently-active mutator
     *
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class3& unsetProp6() {
        getTLMutator().modify(CLASS_ID, getURI())
            ->unset(6,
                    opflex::modb::PropertyInfo::S64,
                    opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Check whether prop7 has been set.  Prop7 is a scalar string
     * property.
     *
     * Prop7 documentation would go here.
     *
     * @return true if prop7 has been set
     */
    bool isProp7Set() {
        return getObjectInstance().isSet(7, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of prop7 if it has been set.  Prop7 is an scalar
     * string property.
     *
     * Prop7 documentation would go here.
     *
     * @return the value of prop7, or boost::none if it is not set.
     */
    boost::optional<const std::string&> getProp7() {
        if (isProp7Set())
            return getObjectInstance().getString(7);
        return boost::none;
    }

    /**
     * Get the value of prop7 if it has been set, or the specified
     * default value of prop7 has not been set.  Prop7 is a scalar
     * string property.
     *
     * Prop7 documentation would go here.
     *
     * @param defaultValue the default value to return of prop7 is not set.
     * @return the value of prop7 or defaultValue
     */
    const std::string& getProp7(const std::string& defaultValue) {
        return getProp7().get_value_or(defaultValue);
    }

    /**
     * Set prop7 to the specified value in the currently-active
     * mutator.
     *
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class3& setProp7(const std::string& newValue) {
        getTLMutator().modify(CLASS_ID, getURI())->setString(7, newValue);
        return *this;
    }

    /**
     * Unset prop7 in the currently-active mutator
     *
     * @throws std::logic_error if no mutator is active
     * @return a reference to the current object
     * @see opflex::modb::Mutator
     */
    class3& unsetProp7() {
        getTLMutator().modify(CLASS_ID, getURI())
            ->unset(7,
                    opflex::modb::PropertyInfo::STRING,
                    opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Retrieve an instance of class3 from the managed object store.
     * If the object does not exist in the local store, returns
     * boost::none.  Note that even though it may not exist locally,
     * it may still exist remotely.
     *
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<OF_SHARED_PTR<class3> >
    resolve(opflex::ofcore::OFFramework& framework,
            const opflex::modb::URI& uri) {
        return opflex::modb::mointernal
            ::MO::resolve<class3>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of class3 from the managed object store by
     * constructing its URI from the path elements that lead to it.
     * If the object does not exist in the local store, returns
     * boost::none.  Note that even though it may not exist locally,
     * it may still exist remotely.
     *
     * The object URI generated by this function will take the form:
     * /class2/[prop4Value]/class3/[prop6Value]/[prop7Value]
     *
     * @param framework the framework instance to use
     * @param prop4Value The value of prop4, a naming property for class2
     * @param prop6Value The value of prop6, a naming property for class3
     * @param prop7Value The value of prop7, a naming property for class3
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<OF_SHARED_PTR<class3> >
    resolve(opflex::ofcore::OFFramework& framework,
            int64_t prop4Value,
            int64_t prop6Value,
            const std::string& prop7Value) {
        return resolve(framework,
                       opflex::modb::URIBuilder()
                           .addElement("class2")
                           .addElement(prop4Value)
                           .addElement("class3")
                           .addElement(prop6Value)
                           .addElement(prop7Value)
                           .build());
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
    static void registerListener(opflex::ofcore::OFFramework& framework,
                                 opflex::modb::ObjectListener* listener) {
        return opflex::modb::mointernal
            ::MO::registerListener(framework, listener, CLASS_ID);
    }

    /**
     * Unregister a listener from updates to this class.
     *
     * @param framework the framework instance
     * @param listener The listener to unregister.
     */
    static void unregisterListener(opflex::ofcore::OFFramework& framework,
                                   opflex::modb::ObjectListener* listener) {
        return opflex::modb::mointernal
            ::MO::unregisterListener(framework, listener, CLASS_ID);
    }

    /**
     * Construct a class3 wrapper class.  This should typically not be
     * called from user code.
     */
    class3(opflex::ofcore::OFFramework& framework,
           const opflex::modb::URI& uri,
           const OF_SHARED_PTR<const opflex::modb
              ::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
};

} /* namespace testmodel */

#endif /* TESTMODEL_CLASS3_H */
