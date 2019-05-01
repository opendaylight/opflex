/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * class5 from testmodel
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef TESTMODEL_CLASS5_H
#define TESTMODEL_CLASS5_H

#include <boost/optional.hpp>

#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

namespace testmodel {

/**
 * @brief This is a sample class used in a test model
 *
 * This is the extended documentation for class5
 */
class class5 : public opflex::modb::mointernal::MO {
public:
    /**
     * The unique class ID for class5
     */
    static const opflex::modb::class_id_t CLASS_ID = 4;

    /**
     * Check whether prop10 has been set.  Prop10 is a string scalar
     * property.
     *
     * Prop10 documentation would go here.
     *
     * @return true if prop10 has been set
     */
    bool isProp10Set() {
        return getObjectInstance().isSet(10, opflex::modb::PropertyInfo::STRING);
    }

    /**
     * Get the value of prop10 if it has been set.  Prop10 is a string
     * scalar property.
     *
     * Prop10 documentation would go here.
     *
     * @return the value of prop10, or boost::none if it is not set.
     */
    boost::optional<const std::string&> getProp10() {
        if (isProp10Set())
            return getObjectInstance().getString(10);
        return boost::none;
    }

    /**
     * Get the value of prop10 if it has been set, or the specified
     * default value of prop10 has not been set.  Prop10 is a string
     * scalar property.
     *
     * Prop10 documentation would go here.
     *
     * @param defaultValue the default value to return of prop10 is not set.
     * @return the value of prop10 or defaultValue
     */
    const std::string& getProp10(const std::string& defaultValue) {
        return getProp10().get_value_or(defaultValue);
    }

    /**
     * Set prop10 to the specified value in the currently-active
     * mutator.
     *
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class5& setProp10(const std::string&  newValue) {
        getTLMutator().modify(CLASS_ID, getURI())->setString(10, newValue);
        return *this;
    }

    /**
     * Unset prop10 in the currently-active mutator
     *
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class5& unsetProp10() {
        getTLMutator().modify(CLASS_ID, getURI())
            ->unset(10,
                    opflex::modb::PropertyInfo::STRING,
                    opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Get the number of values set for class4Ref.  Class4Ref is a
     * vector-valued reference to class4.
     *
     * Class4Ref documentation would go here.
     *
     * @return the number of value set for class4Ref
     */
    size_t getClass4RefSize() {
        return getObjectInstance().getReferenceSize((opflex::modb::prop_id_t)11);
    }

    /**
     * Get the value of class4Ref at the specified index. Class4Ref is
     * a vector-valued reference to class4.
     *
     * Class4Ref documentation would go here.
     *
     * @param index the index of the property to retrieve
     * @return the value of class4Ref at the index specified.
     * @throws std::out_of_range if if there is no value for the
     * property at the specified index.
     */
    opflex::modb::reference_t getClass4Ref(size_t index) {
        return getObjectInstance().getReference((opflex::modb::prop_id_t)11,
                                                index);
    }

    /**
     * Set class4Ref to the specified value in the currently-active
     * mutator.
     *
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class5& setClass4Ref(const std::vector<opflex::modb::reference_t>& newValue) {
        getTLMutator().modify(CLASS_ID, getURI())->
            setReference((opflex::modb::prop_id_t)11, newValue);
        return *this;
    }

    /**
     * Add a value to the class4Ref reference vector
      *
     * @param newValue the new value to add
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class5& addClass4Ref(opflex::modb::URI& newValue) {
        getTLMutator().modify(CLASS_ID, getURI())->
            addReference((opflex::modb::prop_id_t)11, 4, newValue);
        return *this;
    }

    /**
     * Add a value to the class4Ref reference vector by constructing
     * its URI from the path elements that lead to it.
     *
     * The object URI generated by this function will take the form:
     * /class4/[prop9Value]
     *
     * @param newValue the new value to add
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class5& addClass4Ref(const std::string& prop9Value) {
        getTLMutator().modify(CLASS_ID, getURI())->
            addReference((opflex::modb::prop_id_t)11,
                         (opflex::modb::class_id_t)4,
                         opflex::modb::URIBuilder()
                             .addElement("class4")
                             .addElement(prop9Value)
                             .build());
        return *this;
    }

    /**
     * Unset class4Ref in the currently-active mutator
     *
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class5& unsetClass4Ref() {
        getTLMutator().modify(CLASS_ID, getURI())
            ->unset((opflex::modb::prop_id_t)11,
                    opflex::modb::PropertyInfo::REFERENCE,
                    opflex::modb::PropertyInfo::VECTOR);
        return *this;
    }

    /**
     * Retrieve an instance of class5 from the managed object store.
     * If the object does not exist in the local store, returns
     * boost::none.  Note that even though it may not exist locally,
     * it may still exist remotely.
     *
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<OF_SHARED_PTR<class5> >
    resolve(opflex::ofcore::OFFramework& framework,
            const opflex::modb::URI& uri) {
        return opflex::modb::mointernal
            ::MO::resolve<class5>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of class5 from the managed object store by
     * constructing its URI from the path elements that lead to it.
     * If the object does not exist in the local store, returns
     * boost::none.  Note that even though it may not exist locally,
     * it may still exist remotely.
     *
     * The object URI generated by this function will take the form:
     * /class5/[prop10Value]
     *
     * @param framework the framework instance to use
     * @param prop10Value The value of prop10, a naming property for class5
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<OF_SHARED_PTR<class5> >
    resolve(opflex::ofcore::OFFramework& framework,
            const std::string& prop10Value) {
        return resolve(framework,
                       opflex::modb::URIBuilder()
                           .addElement("class5")
                           .addElement(prop10Value)
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
     * Construct a class5 wrapper class.  This should typically not be
     * called from user code.
     */
    class5(opflex::ofcore::OFFramework& framework,
           const opflex::modb::URI& uri,
           const OF_SHARED_PTR<const opflex::modb
              ::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
};

} /* namespace testmodel */

#endif /* TESTMODEL_CLASS5_H */
