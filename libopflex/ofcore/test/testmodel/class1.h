/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * class1 from testmodel
 *
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef TESTMODEL_CLASS1_H
#define TESTMODEL_CLASS1_H

#include <boost/optional.hpp>

#include "opflex/modb/URIBuilder.h"
#include "opflex/modb/mo-internal/MO.h"

#include "testmodel/class2.h"
#include "testmodel/class4.h"
#include "testmodel/class5.h"

/**
 * @brief Namespace containing the generated classes for testmodel
 */
namespace testmodel {

/**
 * @brief This is a sample class used in a test model
 *
 * This is the extended documentation for class1
 */
class class1 : public opflex::modb::mointernal::MO {
public:
    /**
     * The unique class ID for class1
     */
    static const opflex::modb::class_id_t CLASS_ID = 1;

    /**
     * Check whether prop1 has been set.  Prop1 is an unsigned 64-bit
     * integer scalar property.
     *
     * Prop1 documentation would go here.
     *
     * @return true if prop1 has been set
     */
    bool isProp1Set() {
        return getObjectInstance().isSet(1, opflex::modb::PropertyInfo::U64);
    }

    /**
     * Get the value of prop1 if it has been set.  Prop1 is an
     * unsigned 64-bit integer scalar property.
     *
     * Prop1 documentation would go here.
     *
     * @return the value of prop4, or boost::none if it is not set.
     */
    boost::optional<uint64_t> getProp1() {
        if (isProp1Set())
            return getObjectInstance().getUInt64(1);
        return boost::none;
    }

    /**
     * Get the value of prop1 if it has been set, or the specified
     * default value of prop1 has not been set.  Prop1 is an unsigned
     * 64-bit integer scalar property.
     *
     * Prop1 documentation would go here.
     *
     * @param defaultValue the default value to return of prop1 is not set.
     * @return the value of prop1 or defaultValue
     */
    uint64_t getProp1(uint64_t defaultValue) {
        return getProp1().get_value_or(defaultValue);
    }

    /**
     * Set prop1 to the specified value in the currently-active
     * mutator.
     *
     * @return a reference to the current object
     * @param newValue the new value to set.
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class1& setProp1(uint64_t newValue) {
        getTLMutator().modify(CLASS_ID, getURI())->
            setUInt64((opflex::modb::prop_id_t)1, newValue);
        return *this;
    }

    /**
     * Unset prop1 in the currently-active mutator
     *
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class1& unsetProp1() {
        getTLMutator().modify(CLASS_ID, getURI())
            ->unset((opflex::modb::prop_id_t)1,
                    opflex::modb::PropertyInfo::U64,
                    opflex::modb::PropertyInfo::SCALAR);
        return *this;
    }

    /**
     * Get the number of values set for prop2.  Prop2 is a string
     * vector property.
     *
     * Prop2 documentation would go here.
     *
     * @return the number of value set for prop2
     */
    size_t getProp2Size() {
        return getObjectInstance().getStringSize((opflex::modb::prop_id_t)2);
    }

    /**
     * Get the value of prop2 at the specified index. Prop2 is a
     * string vector property.
     *
     * Prop2 documentation would go here.
     *
     * @param index the index of the property to retrieve
     * @return the value of prop2 at the index specified.
     * @throws std::out_of_range if if there is no value for the
     * property at the specified index.
     */
    const std::string& getProp2(size_t index) {
        return getObjectInstance().getString((opflex::modb::prop_id_t)2,
                                             index);
    }

    /**
     * Set prop2 to the specified value in the currently-active
     * mutator.
     *
     * @param newValue the new value to set.
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class1& setProp2(const std::vector<std::string>& newValue) {
        getTLMutator().modify(CLASS_ID, getURI())->
            setString((opflex::modb::prop_id_t)2, newValue);
        return *this;
    }

    /**
     * Add a value to the prop2 string vector
      *
     * @param newValue the new value to add
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class1& addProp2(const std::string& newValue) {
        getTLMutator().modify(CLASS_ID, getURI())->
            addString((opflex::modb::prop_id_t)2, newValue);
        return *this;
    }

    /**
     * Unset prop2 in the currently-active mutator
     *
     * @return a reference to the current object
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    class1& unsetProp2() {
        getTLMutator().modify(CLASS_ID, getURI())
            ->unset((opflex::modb::prop_id_t)2,
                    opflex::modb::PropertyInfo::STRING,
                    opflex::modb::PropertyInfo::VECTOR);
        return *this;
    }

    /**
     * Retrieve an instance of class1 from the managed object store
     * using its URI.  If the object does not exist in the local
     * store, returns boost::none.  Note that even though it may not
     * exist locally, it may still exist remotely.
     *
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<OF_SHARED_PTR<class1> >
    resolve(opflex::ofcore::OFFramework& framework,
            const opflex::modb::URI& uri) {
        return opflex::modb::mointernal
            ::MO::resolve<class1>(framework, CLASS_ID, uri);
    }

    /**
     * Retrieve an instance of class1 from the managed object store by
     * constructing its URI from the path elements that lead to it.
     * If the object does not exist in the local store, returns
     * boost::none.  Note that even though it may not exist locally,
     * it may still exist remotely.
     *
     * @param framework the framework instance to use
     * @param uri the URI of the object to retrieve
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    static boost::optional<OF_SHARED_PTR<class1> >
    resolve(opflex::ofcore::OFFramework& framework) {
        return resolve(framework,
                       opflex::modb::URI::ROOT);
    }

    /**
     * Create an instance of class1, the root element in the
     * management information tree for the given framework instance in
     * the currently-active mutator.
     *
     * @param framework the framework instance to use
     * @throws std::logic_error if no mutator is active
     * @see opflex::modb::Mutator
     */
    static OF_SHARED_PTR<class1>
    createRootElement(opflex::ofcore::OFFramework& framework) {
        return opflex::modb::mointernal
            ::MO::createRootElement<class1>(framework, CLASS_ID);
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     *
     * @param prop4Value The value of prop4, a naming property for
     * class2
     * @return a shared pointer to the object or boost::none if it
     * does not exist.
     */
    boost::optional<OF_SHARED_PTR<class2> >
    resolveClass2(int64_t prop4Value) {
        return class2::resolve(getFramework(),
                               opflex::modb::URIBuilder(getURI())
                                   .addElement("class2")
                                   .addElement(prop4Value)
                                   .build());
    }

    /**
     * Resolve and retrieve all of the immediate children of type
     * class2. Note that this retrieves only those children that exist
     * in the local store.  It is possible that are other children
     * exist remotely.
     *
     * The resulting managed objects will be added to the result
     * vector provided.
     *
     * @param out a reference to a vector that will receive the child
     * objects.
     */
    void resolveClass2(/* out */ std::vector<OF_SHARED_PTR<class2> >& out) {
        return opflex::modb::mointernal
            ::MO::resolveChildren<class2>(getFramework(),
                                          CLASS_ID, getURI(),
                                          (opflex::modb::prop_id_t)3,
                                          (opflex::modb::class_id_t)2,
                                          out);
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     *
     * @param prop4Value The value of prop4, a naming property for
     * class2
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    OF_SHARED_PTR<class2> addClass2(int64_t prop4Value) {
        OF_SHARED_PTR<class2> result =
            addChild<class2>(CLASS_ID, getURI(),
                             (opflex::modb::prop_id_t)3,
                             (opflex::modb::class_id_t)2,
                             opflex::modb::URIBuilder(getURI())
                                 .addElement("class2")
                                 .addElement(prop4Value)
                                 .build());
        result->setProp4(prop4Value);
        return result;
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     *
     * @param prop9Value The value of prop9, a naming property for
     * class3

     */
    boost::optional<OF_SHARED_PTR<class4> >
    resolveClass4(const std::string& prop9Value) {
        return class4::resolve(getFramework(),
                               opflex::modb::URIBuilder(getURI())
                                   .addElement("class4")
                                   .addElement(prop9Value)
                                   .build());
    }

    /**
     * Resolve and retrieve all of the immediate children of type
     * class4. Note that this retrieves only those children that exist
     * in the local store.  It is possible that are other children
     * exist remotely.
     *
     * The resulting managed objects will be added to the result
     * vector provided.
     *
     * @param out a reference to a vector that will receive the child
     * objects.
     */
    void resolveClass4(/* out */ std::vector<OF_SHARED_PTR<class4> >& out) {
        return opflex::modb::mointernal
            ::MO::resolveChildren<class4>(getFramework(),
                                          CLASS_ID, getURI(),
                                          (opflex::modb::prop_id_t)8,
                                          (opflex::modb::class_id_t)4,
                                          out);
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     *
     * @param prop9Value The value of prop9, a naming property for
     * class4
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    OF_SHARED_PTR<class4> addClass4(const std::string& prop9Value) {
        OF_SHARED_PTR<class4> result =
            addChild<class4>(CLASS_ID, getURI(),
                             (opflex::modb::prop_id_t)8,
                             (opflex::modb::class_id_t)4,
                             opflex::modb::URIBuilder(getURI())
                                 .addElement("class4")
                                 .addElement(prop9Value)
                                 .build());
        result->setProp9(prop9Value);
        return result;
    }

    /**
     * Retrieve the child object with the specified naming
     * properties. If the object does not exist in the local store,
     * returns boost::none.  Note that even though it may not exist
     * locally, it may still exist remotely.
     *
     * @param prop10Value The value of prop10, a naming property for
     * class3

     */
    boost::optional<OF_SHARED_PTR<class5> >
    resolveClass5(const std::string& prop10Value) {
        return class5::resolve(getFramework(),
                               opflex::modb::URIBuilder(getURI())
                                   .addElement("class5")
                                   .addElement(prop10Value)
                                   .build());
    }

    /**
     * Resolve and retrieve all of the immediate children of type
     * class5. Note that this retrieves only those children that exist
     * in the local store.  It is possible that are other children
     * exist remotely.
     *
     * The resulting managed objects will be added to the result
     * vector provided.
     *
     * @param out a reference to a vector that will receive the child
     * objects.
     */
    void resolveClass5(/* out */ std::vector<OF_SHARED_PTR<class5> >& out) {
        return opflex::modb::mointernal
            ::MO::resolveChildren<class5>(getFramework(),
                                          CLASS_ID, getURI(),
                                          (opflex::modb::prop_id_t)8,
                                          (opflex::modb::class_id_t)4,
                                          out);
    }

    /**
     * Create a new child object with the specified naming properties
     * and make it a child of this object in the currently-active
     * mutator.  If the object already exists in the store, get a
     * mutable copy of that object.  If the object already exists in
     * the mutator, get a reference to the object.
     *
     * @param prop10Value The value of prop10, a naming property for
     * class5
     * @throws std::logic_error if no mutator is active
     * @return a shared pointer to the (possibly new) object
     */
    OF_SHARED_PTR<class5> addClass5(const std::string& prop10Value) {
        OF_SHARED_PTR<class5> result =
            addChild<class5>(CLASS_ID, getURI(),
                             (opflex::modb::prop_id_t)8,
                             (opflex::modb::class_id_t)4,
                             opflex::modb::URIBuilder(getURI())
                                 .addElement("class5")
                                 .addElement(prop10Value)
                                 .build());
        result->setProp10(prop10Value);
        return result;
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
     * Construct a class1 wrapper class.  This should typically not be
     * called from user code.
     */
    class1(opflex::ofcore::OFFramework& framework,
           const opflex::modb::URI& uri,
           const OF_SHARED_PTR<const opflex::modb
              ::mointernal::ObjectInstance>& oi)
        : MO(framework, CLASS_ID, uri, oi) { }
};

} /* namespace testmodel */

#endif /* TESTMODEL_CLASS1_H */
