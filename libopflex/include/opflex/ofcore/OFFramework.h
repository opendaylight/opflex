/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*!
 * @file OFFramework.h
 * @brief Interface definition file for OFFramework
 */
/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/*!
 * @mainpage
 * @tableofcontents
 * @section intro Introduction
 *
 * The OpFlex framework allows you to develop agents that can
 * communicate using the OpFlex protocol and act as a policy element
 * in an OpFlex-based distributed control system.  The OpFlex
 * architecture provides a distributed control system based on a
 * declarative policy information model.  The policies are defined at
 * a logically centralized policy repository and enforced within a set
 * of distributed policy elements.  The policy repository communicates
 * with the subordinate policy elements using the OpFlex control
 * protocol.  This protocol allows for bidirectional communication of
 * policy, events, statistics, and faults.
 *
 * Rather than simply providing access to the OpFlex protocol, this
 * framework allows you to directly manipulate a management
 * information tree containing a hierarchy of managed objects.  This
 * tree is kept in sync as needed with other policy elements in the
 * system, and you are automatically notified when important changes
 * to the model occur.  Additionally, we can ensure that only those
 * managed objects that are important to the local policy element are
 * synchronized locally.
 *
 * @subsection model Object Model
 *
 * Interactions with the OpFlex framework happen through the
 * management information tree.  This is a tree of managed objects
 * defined by an object model specific to your application.  There are
 * a few important major category of objects that can appear in the
 * model.
 *
 * - First, there is the @em policy object.  A policy object
 *   represents some data related to a policy that describes a user
 *   intent for how the system should behave.  A policy object is
 *   stored in the <i>policy repository</i> which is the source of
 *   "truth" for this object.
 *
 * - Second, there is an @em endpoint object.  A endpoint represents
 *   an entity in the system to which we want to apply policy, which
 *   could be a network interface, a storage array, or other relevent
 *   policy endpoint.  Endpoints are discovered and reported by policy
 *   elements locally, and are synchronized into the <i>endpoint
 *   repository</i>.  The originating policy element is the source of
 *   truth for the endpoints it discovers.  Policy elements can
 *   retrieve information about endpoints discovered by other policy
 *   elements by resolving endpoints from the endpoint repository.
 *
 * - Third, there is the @em observable object.  An observable object
 *   represents some state related to the operational status or health
 *   of the policy element.  Observable objects will be reported to
 *   the \em observer.
 *
 * - Finally, there is the @em local-only object.  This is the
 *   simplest object because it exists only local to a particular
 *   policy element.  These objects can be used to store state
 *   specific to that policy element, or as helpers to resolve other
 *   objects.  Read on to learn more.
 *
 * You can use the genie tool that is included with the framework to
 * produce your application model along with a set of generated
 * accessor classes that can work with this framework library.  You
 * should refer to the documentation that accompanies the genie tool
 * for information on how to use to to generate your object model.
 * Later in this guide, we'll go through examples of how to use the
 * generated managed object accessor classes.
 *
 * @subsection sideeffect Programming by Side Effect
 *
 * When developing software on the OpFlex framework, you'll need to
 * think in a slightly different way.  Rather than calling an API
 * function that would perform some specific action, you'll need to
 * write a managed object to the managed object database.  Writing
 * that object to the store will trigger the side effect of performing
 * the action that you want.
 *
 * For example, a policy element will need to have a component
 * responsible for discovering policy endpoints.  When it discovers a
 * policy endpoint, it would write an endpoint object into the managed
 * object database.  That endpoint object will contain a reference to
 * policy that is relevant to the endpoint object.  This will trigger
 * a whole cascade of events.  First, the framework will notice that
 * an endpoint object has been created and it will write it to the
 * endpoint repository.  Second, the framework to will attempt to
 * resolve the unresolved reference to the relevent policy object.
 * There might be a whole chain of policy resolutions that will be
 * automatically performed to download all the relevent policy until
 * there are no longer any dangling references.
 *
 * As long as there is a locally-created object in the system with a
 * reference to that policy, the framework will continually ensure
 * that the policy and any transitive policies are kept up to date.
 * The policy element can subscribe to updates to these policy classes
 * that will be invoked either the first time the policy is resolved
 * or any time the policy changes.
 *
 * A consequence of this design is that the managed object database
 * can be temporarily in an inconsistent state with unresolved
 * dangling references.  Eventually, however, the inconsistency will
 * be fully resolved.  The policy element must be able to cleanly
 * handle partially-resolved or inconsistent state and eventually
 * reach the correct state as it receives these update notifications.
 * Note that, in the OpFlex architecture, when there is no policy that
 * specifically allows a particular action, that action must be
 * prevented.
 *
 * Let's cover one slightly more complex example.  If a policy element
 * needs to discover information about an endpoint that is @em not
 * local to that policy element, it will need to retrieve that
 * information from the endpoint repository.  However, just as there
 * is no API call to retrieve a policy object from the policy
 * repository, there is no API call to retrieve an endpoint from the
 * endpoint repository.
 *
 * To get this information, the policy element needs to create a
 * local-only object that references the endpoint.  Once it creates
 * this local-only object, if the endpoint is not already resolved,
 * the framework will notice the dangling reference and automatically
 * resolve the endpoint from the endpoint respository.  When the
 * endpoint resolution completes, the framework deliver an update
 * notification to the policy element.  The policy element will
 * continue to receive any updates related to that endpoint until the
 * policy element remove the local-only reference to the object.  Once
 * this occurs, the framework can garbage-collect any unreferenced
 * objects.
 *
 * @subsection threading Threading and Ownership
 *
 * The OpFlex framework uses a somewhat unique threading model.  Each
 * managed object in the system belongs to a particular owner.  An \em
 * owner would typically be a single thread that is reponsible for all
 * updates to objects with that owner.  Though anything can read the
 * state of a managed object, only the owner of a managed object is
 * permitted to write to it.  Though this is not strictly required for
 * correctness, the performance of the system wil be best if you
 * ensure that only one thread at a time is writing to objects with a
 * particular owner.
 *
 * Change notifications are delivered in a serialized fashion by a
 * single thread.  Blocking this thread from a notification callback
 * will stall delivery of all notifications.  It is therefore best
 * practice to ensure that you do not block or perform long-running
 * operations from a notification callback..
 *
 * @section init Basic Usage and Initialization
 *
 * There are two ways to use the OpFlex Framework.  The primary way is
 * through the C++ API, but you can also do the same thing using the C
 * wrapper API.  In the remaining sections, there will be some
 * introductory text that applies to both APIs, followed by the
 * specifics for each of the individual APIs.  If you only care about
 * either the C++ or the C wrapper API, you can simply skip the
 * sections that do not apply.
 *
 * @subsection cppinit C++
 * The primary interface point into the framework is @ref
 * opflex::ofcore::OFFramework.  You must instantiate your
 * own copy of the framework.
 *
 * Before you can use the framework, you must initialize it by
 * installing your model metadata.  The model metadata is accessible
 * through the generated model library.  In this case, it assumes your
 * model is called "mymodel":
 *
 * @code
 * #include <opflex/ofcore/OFFramework.h>
 * #include <mymodel/metadata/metadata.hpp>
 *
 * // ...
 *
 * using opflex::ofcore::OFFramework;
 * OFFramework framework;
 * framework.setModel(mymodel::getMetadata());
 * @endcode
 *
 * The other critical piece of information required for initialization
 * is the OpFlex identity information.  The identity information is
 * required in order to successfully connect to OpFlex peers.  In
 * OpFlex, each component has a unique name within its policy domain,
 * and each policy domain is identified by a globally unique domain
 * name.  You can set this identity information by calling:
 * @code
 * framework
 *     .setOpflexIdentity("[component name]", "[unique domain]");
 * @endcode
 *
 * You can then start the framework simply by calling:
 * @code
 * framework.start();
 * @endcode
 *
 * Finally, you can add peers after the framework is started by
 * calling the @ref opflex::ofcore::OFFramework::addPeer method:
 * @code
 * framework.addPeer("192.168.1.5", 1234);
 * @endcode
 *
 * When connecting to the peer, that peer may provide an additional
 * list of peers to connect to, which will be automatically added as
 * peers.  If the peer does not include itself in the list, then the
 * framework will disconnect from that peer and add the peers in the
 * list.  In this way, it is possible to automatically bootstrap the
 * correct set of peers using a known hostname or IP address or a
 * known, fixed anycast IP address.
 *
 * To cleanly shut down, you can call:
 * @code
 * framework.stop();
 * @endcode
 *
 * @subsection cinit C Wrapper
 * The C wrapper interface provides a clean way to use the framework
 * from a C-based policy element.  In this framework, you will
 * interact with opaque pointer objects that you can pass to
 * appropriate functions.
 *
 * All API calls will take the form:
 * @code
 * ofstatus ofobjname_func(in arguments, out arguments);
 * @endcode
 *
 * The ofstatus is a one of the status codes defined in ofcore_c.h.
 * There are macros defined @ref OF_IS_SUCCESS and @ref OF_IS_FAILURE
 * that make checking the status easy, so you can use code such as:
 * @code
 * ofstatus dostuff() {
 *     ofstatus result = OF_ESUCCESS;
 *     ofobjname_p obj = NULL;
 *     if (OF_IS_FAILED(result = ofobjname_create(&obj)) ||
 *         OF_IS_FAILED(result = ofobjname_func(obj))) {
 *         LOG("Could not do that thing I wanted to do!");
 *         goto done;
 *     }
 *
 *     // ...
 *
 * done:
 *     ofobjname_destroy(&obj);
 *     return result;
 * }
 * @endcode
 *
 * To manage your memory correctly, you'll need to ensure that you always call
 * the appropriate _destroy function for any object that you created
 * with a corresponding _create function, as well as for any functions
 * that are documented as requiring you to destroy the returned
 * object.
 *
 * The primary interface point into the framework is the offramework
 * object.  Before you can use the framework, you'll need to
 * instantiate a framework instance:
 * @code
 * #include <opflex/c/offramework_c.h>
 *
 * // ...
 *
 * offramework_p framework = NULL;
 * ofstatus result = OF_ESUCCESS;
 * if (OF_IS_FAILED(result = offramework_create(&framework))) {
 *     // handle error
 * }
 * @endcode
 *
 * Next, you'll need to intialize it with your model:
 * @code
 * #include <mymodel/metadata/metadata_c.h>
 *
 * // ...
 *
 * if (OF_IS_FAILED(result = offramework_set_model(framework,
 *                                                 getMetadata()))) {
 *     // handle error
 * }
 * @endcode
 *
 * Finally, you need to start up the framework:
 * @code
 * if (OF_IS_FAILED(result = offramework_start(framework))) {
 *     // handle error
 * }
 * @endcode
 *
 * To cleanly shut down and free the framework memory, you can call:
 * @code
 * if (OF_IS_FAILED(result = offramework_destroy(&framework)) {
 *     // handle error
 * }
 * @endcode
  *
 * @section data Working with Data in the Tree
 *
 * @subsection read Reading from the Tree
 *
 * You can access data in the managed tree using the generated
 * accessor classes.  The details of exactly which classes you'll use
 * will depend on the model you're using, but let's assume that we
 * have a simple model called "simple" with the following classes:
 *
 * - @b root - The root node.  The URI for the root node is "/"
 *
 * - @b foo - A policy object, and a child of root, with a scalar
 *   string property called "bar", and a unsigned 64-bit integer
 *   property called baz.  The bar property is the naming property for
 *   foo.  The URI for a foo object would be "/foo/[value of bar]/"
 *
 * - @b fooref - A local-only child of root, with a reference to a
 *   foo, and a scalar string property called "bar".  The bar property
 *   is the naming property for foo.  The URI for a fooref object
 *   would be "/fooref/[value of bar]/"
 *
 * In this example, we'll have a generated class for each of the
 * objects.  There are two main ways to get access to an object in the
 * tree.
 *
 * First, we can get instantiate an accessor class to any node in the
 * tree by calling one of its static resolve functions.  The resolve
 * functions can take either an already-built URI that represents the
 * object, or you can call the version that will locate the object by
 * its naming properties.
 *
 * Second, we can access the object also from its parent object using
 * the appropriate child resolver member functions.
 *
 * However we read it, the object we get back is an immutable view
 * into the object it references.  The properties set locally on that
 * object will not change even though the underlying object may have
 * been updated in the store.  Note, however, that its children @em
 * can change between when you first retrieve the object and when you
 * resolve any children.
 *
 * Another thing that is critical to note again is that when you
 * attempt to resolve an object, you can get back nothing, even if the
 * object actually does exist on another OpFlex node.  You must ensure
 * that some object in the managed object database references the
 * remote managed object you want before it will be visible to you.
 *
 * @subsubsection cppread C++
 *
 * To get access to the root node using the default framework
 * instance, we can simply call:
 *
 * @code
 * using std::shared_ptr;
 * using boost::optional;
 *
 * optional<shared_ptr<simple::root> > r(simple::root::resolve());
 * @endcode

 * Note that whenever we can a resolve function, we get back our data
 * in the form of an optional shared pointer to the object instance.
 * If the node does not exist, then the optional will be set to
 * boost::none.  Note that if you dereference an optional that has not
 * been set, you'll trigger an assert, so you must check the return as
 * follows:
 * @code
 * if (!r) {
 *    // handle missing object
 * }
 * @endcode
 *
 * Now let's get a child node of the root in three different ways:
 * @code
 * // Get foo1 by constructing its URI from the root
 * optional<shared_ptr<simple::foo> > foo1(simple::foo::resolve("test"));
 *
 * // get foo1 by constructing its URI relative to its parent
 * foo1 = r.get()->resolveFoo("test");
 *
 * // get foo1 by manually building its URI
 * foo1 = simple::foo::resolve(opflex::modb::URIBuilder()
 *                                .addElement("foo")
 *                                .addElement("test")
 *                                .build());
 * @endcode
 *
 * All three of these calls will give us the same object, which is the
 * "foo" object located at "/foo/test/".
 *
 * The foo class has a single string property called "bar".  We can
 * easily access it as follows:
 * @code
 * const std::string& barv = foo1.getBar();
 * @endcode
 *
 * @subsubsection cread C Wrapper
 *
 * The C wrapper interface is not yet available.
 *
 * @subsection write Writing to the Tree
 *
 * Writing to the tree is nearly as easy as reading from it.  The key
 * concept to understand is the mutator object.  If you want to make
 * changes to the tree, you must allocate a mutator object.  The
 * mutator will register itself in some thread-local storage in the
 * framework instance you're using.  The mutator is specific to a
 * single "owner" for the data, so you can only make changes to data
 * associated with that owner.
 *
 * Whenever you modify one of the accessor classes, the change is
 * actually forwarded to the currently-active mutator.  You won't see
 * any of the changes you make until you call the commit member
 * function on the mutator.  When you do that, all the changes you
 * made are written into the store.
 *
 * Once the changes are written into the store, you will need to call
 * the appropriate resolve function again to see the changes.
 *
 * @subsubsection cppwrite C++
 *
 * Allocating a mutator is simple.  To create a mutator for the
 * default framework instance associated with the owner "owner1", just
 * allocate the mutator on the stack.  Be sure to call commit() before
 * it goes out of scope or you'll lose your changes.
 *
 * @code
 * {
 *     opflex::modb::Mutator mutator("owner1");
 *
 *     // make changes here
 *
 *     mutator.commit();
 * }
 * @endcode

 * Note that if an exception is thrown while making changes but before
 * committing, the mutator will go out of scope and the changes will
 * be discarded.
 *
 * To create a new node, you must call the appropriate add[Child]
 * member function on its parent.  This function takes parameters for
 * each of the naming properties for the object:
 * @code
 * shared_ptr<simple::foo> newfoo(root->addFoo("test"));
 * @endcode
 *
 * This will return a shared pointer to a new foo object that has been
 * registered in the active mutator but not yet committed.  The "bar"
 * naming property will be set automatically, but if you want to set
 * the "baz" property now, you can do so by calling:
 * @code
 * newfoo->setBaz(42);
 * @endcode
 *
 * Note that creating the root node requires a call to the special
 * static class method createRootElement:
 * @code
 * shared_ptr<simple::root> newroot(simple::root::createRootElement());
 * @endcode
 *
 * Here's a complete example that ties this all together:
 * @code
 * {
 *     opflex::modb::Mutator mutator("owner1");
 *
 *     shared_ptr<simple::root> newroot(simple::root::createRootElement());
 *     shared_ptr<simple::root> newfoo(newroot->addFoo("test"));
 *     newfoo->setBaz(42);
 *
 *     mutator.commit();
 * }
 * @endcode
 *
 * @subsubsection cwrite C Wrapper
 *
 * The C wrapper interface is not yet available.
 *
 * @section notifs Update Notifications
 *
 * When using the OpFlex framework, you're likely to find that most of
 * your time is spend responding to changes in the managed object
 * database.  To get these notifications, you're going to need to
 * register some number of listeners.
 *
 * You can register an object listener to see all changes related to a
 * particular class by calling a static function for that class.
 * You'll then get notifications whenever any object in that class is
 * added, updated, or deleted.  The listener should queue a task to
 * read the new state and perform appropriate processing.  If this
 * function blocks or peforms a long-running operation, then the
 * dispatching of update notifications will be stalled, but there will
 * not be any other deleterious effects.
 *
 * If multiple changes happen to the same URI, then at least one
 * notification will be delivered but some events may be consolidated.
 *
 * The update you get will tell you the URI and the Class ID of the
 * changed object.  The class ID is a unique ID for each class.  When
 * you get the update, you'll need to call the appropriate resolve
 * function to retrieve the new value.
 *
 * @subsection cppnotifs C++
 *
 * You'll need to create your own object listener derived from
 * opflex::modb::ObjectListener:
 * @code
 * class MyListener : public ObjectListener {
 * public:
 *     MyListener() { }
 *
 *     virtual void objectUpdated(class_id_t class_id, const URI& uri) {
 *         // Your handler here
 *     }
 * };
 * @endcode
 *
 * To register your listener with the default framework instance, just
 * call the appropriate class static method:
 *
 * @code
 * MyListener listener;
 * simple::foo::registerListener(&listener);
 *
 * // main loop
 *
 * simple::foo::unregisterListener(&listener);
 * @endcode
 *
 * The listener will now recieve notifications whenever any foo or any
 * children of any foo object changes.
 *
 * Note that you must ensure that you unregister your listeners before
 * deallocating them.
 *
 * @subsection cnotifs C Wrapper
 *
 * You'll need to define one ore more callbacks that will get the
 * notifications, of type ofnotify_p:
 * @code
 * void notify(void* user_data, class_id_t class_id, ofuri_p uri) {
 *    // your handler here
 * }
 * @endcode
 *
 * Then, to register your handler, you must create an @ref cofobjectlistener
 * that references your handler:
 * @code
 * ofobjectlistener_p listener = NULL;
 * ofstatus result = OF_ESUCCESS;
 * if (OF_IS_FAILED(result = ofobjectlistener_create(NULL, notify,
                                                     &listener))) {
 *     // handle error
 * }
 * @endcode
 *
 * Then, you can register it to receive notifications.
 *
 * The C interface for registering a listener is not yet available.
 *
 * You'll need to unregister the object listener before you destroy
 * it:
 * @code
 * // XXX TODO
 * ofobjectlistener_destroy(&listener);
 * @endcode
 *
 */

#pragma once
#ifndef OPFLEX_CORE_OFFRAMEWORK_H
#define OPFLEX_CORE_OFFRAMEWORK_H

#include <vector>
#include <string>

#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>

#include "opflex/modb/Mutator.h"
#include "opflex/ofcore/PeerStatusListener.h"
#include "opflex/ofcore/MainLoopAdaptor.h"
#include "opflex/ofcore/OFConstants.h"
#include "boost/asio/ip/address_v4.hpp"
#include <opflex/modb/URI.h>
#include <opflex/modb/PropertyInfo.h>

/**
 * @defgroup cpp C++ Interface
 *
 * @brief The C++ interface is the native interface into the OpFlex
 * framework.
 *
 * The main interface to the framework is the @ref OFFramework object.
 * For more information, see the main page of the documentation.
 *
 * There is also a @ref cwrapper available if you need to use C.
 */

namespace opflex {
namespace modb {
class ObjectStore;
class ModelMetadata;
namespace mointernal {
class MO;
}
}

namespace ofcore {

/**
 * \addtogroup cpp
 * @{
 */

/**
 * @defgroup ofcore Core
 * Defines basic library types and definitions
 * \addtogroup ofcore
 * @{
 */

/**
 * @brief Main interface to the OpFlex framework.
 *
 * This class manages configuration and lifecycle for the framework,
 * and provides the primary interface point into the framework.  You
 * must create your own instance.
 *
 * You must first initialize the framework by calling setModel() with
 * your model and then calling start():
 * @code
 * using opflex::ofcore::OFFramework;
 * OFFramework framework;
 * framework.setModel(mymodel);
 * framework.start();
 * @endcode
 * You can shut down the framework by calling stop():
 * @code
 * framework.stop();
 * @endcode
 *
 * Once the framework is initialized, you can interact with the model
 * using the generated model classes, which include static methods for
 * resolving objects from the managed object store.
 */
class OFFramework : private boost::noncopyable {
public:
    /**
     * Create a new framework instance
     */
    OFFramework();

    /**
     * Destroy the framework instance
     */
    virtual ~OFFramework();

    /**
     * Get the library version as an vector of three version numbers:
     * major, minor, and release
     */
    static const std::vector<int>& getVersion();

    /**
     * Get the library version as a string of the format
     * [major].[minor].[release]-[build]
     */
    static const std::string& getVersionStr();

    /**
     * Add the given model metadata to the managed object database.
     * Must be called before start()
     *
     * @param model the model metadata to add to the object database
     */
    void setModel(const modb::ModelMetadata& model);

    /**
     * Set the opflex identity information for this framework
     * instance.
     *
     * @param name the unique name for this opflex component within
     * the policy domain
     * @param domain the globally unique name for this policy domain
     */
    void setOpflexIdentity(const std::string& name,
                           const std::string& domain);

    /**
     * Set the opflex identity information for this framework
     * instance.
     *
     * @param name the unique name for this opflex component within
     * the policy domain
     * @param domain the globally unique name for this policy domain
     * @param location the location string for this policy element.
     */
    void setOpflexIdentity(const std::string& name,
                           const std::string& domain,
                           const std::string& location);

    /**
     * Set the element mode for the opflex element. Element mode
     * can be set only before starting the framework
     *
     * @param mode_ stitched(default) or transport mode
     * @return whether setting was successful
     */
    bool setElementMode(
    opflex::ofcore::OFConstants::OpflexElementMode mode_);

    /**
     * set the prr (policy resolve request) timer durarion.
     * @param duration timer duration in milliseconds
     */
    void setPrrTimerDuration(const uint64_t duration);

    /**
     * Start the framework.  This will start all the framework threads
     * and attempt to connect to configured OpFlex peers.
     */
    virtual void start();

    /**
     * Cleanly stop the framework
     */
    virtual void stop();

    /**
     * Start the framework in synchronous mode using a main loop
     * adaptor.  You will be responsible for calling
     * MainLoopAdaptor::runOnce in each iteration of your main loop so
     * libopflex can make progress.
     *
     * @return the MainLoopAdaptor you will need to call.  The memory
     * is owned by the framework and the pointer will become invalid
     * when stop() is called.
     */
    virtual MainLoopAdaptor* startSync();

    /**
     * Dump the managed object database to the file specified as a
     * JSON blob.
     *
     * @param file the file to write to.
     */
    virtual void dumpMODB(const std::string& file);

    /**
     * Dump the managed object database to the file specified as a
     * JSON blob.
     *
     * @param file the file to write to.
     */
    virtual void dumpMODB(FILE* file);

    /**
     * Pretty print the current MODB to the provided output stream.
     *
     * @param output the output stream to write to
     * @param tree print in a tree format
     * @param includeProps include the object properties
     * @param utf8 output tree using UTF-8 box drawing
     * @param truncate truncate lines to the specified number of
     * characters.  0 means do not truncate.
     */
    virtual void prettyPrintMODB(std::ostream& output,
                                 bool tree = true,
                                 bool includeProps = true,
                                 bool utf8 = true,
                                 size_t truncate = 0);

    /**
     * Enable SSL for connections to opflex peers
     *
     * @param caStorePath the filesystem path to a directory
     * containing CA certificates, or to a file containing a specific
     * CA certificate.
     * @param verifyPeers set to true to verify that peer certificates
     * properly chain to a trusted root
     */
    virtual void enableSSL(const std::string& caStorePath,
                   bool verifyPeers = true);

    /**
     * Enable SSL for connections to opflex peers
     *
     * @param caStorePath the filesystem path to a directory
     * containing CA certificates, or to a file containing a specific
     * CA certificate.
     * @param keyAndCertFilePath the path to the PEM file for this peer,
     * containing its certificate and its private key, possibly encrypted.
     * @param passphrase the passphrase to be used to decrypt the private
     * key within this peer's PEM file
     * @param verifyPeers set to true to verify that peer certificates
     * properly chain to a trusted root
     */
    virtual void enableSSL(const std::string& caStorePath,
                           const std::string& keyAndCertFilePath,
                           const std::string& passphrase,
                           bool verifyPeers = true);

    /**
     * Enable the MODB inspector service.  The service will listen on
     * the specified UNIX domain socket for connections from the
     * inspector client.
     *
     * @param socketName A path to the UNIX domain socket
     */
    virtual void enableInspector(const std::string& socketName);

    /**
     * Add an OpFlex peer.  If the framework is started, this will
     * immediately initiate a new connection asynchronously.
     *
     * When connecting to the peer, that peer may provide an
     * additional list of peers to connect to, which will be
     * automatically added as peers.  If the peer does not include
     * itself in the list, then the framework will disconnect from
     * that peer and add the peers in the list.  In this way, it is
     * possible to automatically bootstrap the correct set of peers
     * using a known hostname or IP address or a known, fixed anycast
     * IP address.
     *
     * @param hostname the hostname or IP address to connect to
     * @param port the TCP port to connect on
     */
    virtual void addPeer(const std::string& hostname, int port);

    /**
     * Register the given peer status listener to get updates on the
     * health of the connection pool and on individual connections.
     * Must be called before calling start() on the framework.
     *
     * @param listener the listener to register
     */
    virtual void registerPeerStatusListener(PeerStatusListener* listener);

    /**
     * Get the Ipv4 proxy address for the vswitch in transport mode.
     * In transport mode, proxy addresses are obtained
     * from the first peer's identity response whereas in stitched mode,
     * proxy address is configured/implicit.
     *
     * @param v4ProxyAddress
     */
    virtual void getV4Proxy(boost::asio::ip::address_v4 &v4ProxyAddress );

    /**
     * Get the Ipv6 proxy address for the vswitch in transport mode.
     *
     * @param v6ProxyAddress
     */
    virtual void getV6Proxy(boost::asio::ip::address_v4 &v6ProxyAddress );

    /**
     * Get the Mac proxy address for the vswitch in transport mode.
     *
     * @param macProxyAddress
     */
    virtual void getMacProxy(boost::asio::ip::address_v4 &macProxyAddress );

    /**
     * Get the parent URI for the passed in child object URI
     * @param[in] child_class the class of the child object
     * @param[in] child the URI of the child object
     * @return a reference to the URI of the parent
     */
    boost::optional<opflex::modb::URI>
         getParent(opflex::modb::class_id_t child_class,
                   const opflex::modb::URI& child);


private:
    /**
     * Get the object store that provides access to the managed object
     * database.
     *
     * @return a reference to the object store associated with this
     * framework instance
     */
    modb::ObjectStore& getStore();

    /**
     * Register a mutator into the thread-local storage for this
     * store.
     * @param mutator the mutator to set
     */
    void registerTLMutator(modb::Mutator& mutator);
    /**
     * Get the mutator from thread-local storage
     * @return the mutator, or NULL if none is set
     */
    modb::Mutator& getTLMutator();
    /**
     * Reset the thread-local mutator to NULL
     */
    void clearTLMutator();

    class OFFrameworkImpl;
    OFFrameworkImpl* pimpl;

    friend class OFFrameworkImpl;
    friend class modb::Mutator;
    friend class modb::mointernal::MO;
    friend class MockOFFramework;
};

/**
 * A mock framework object that will not attempt to create remote
 * connections or resolve references.  This is useful for unit tests
 * for code that uses the framework, since you can easily write into
 * the store the objects that would have been resolved remotely
 * without any interference.
 */
class MockOFFramework : public OFFramework {
public:
    MockOFFramework() : OFFramework() { }
    virtual ~MockOFFramework() {};
    /**
     * Set the Ipv4 proxy address for the vswitch in transport mode.
     * In transport mode, proxy addresses are obtained
     * from the first peer's identity response whereas in stitched mode,
     * proxy address is configured/implicit.
     *
     * @param v4ProxyAddress
     */
    virtual void setV4Proxy(const boost::asio::ip::address_v4& v4ProxyAddress);

    /**
     * Set the Ipv6 proxy address for the vswitch in transport mode.
     *
     * @param v6ProxyAddress
     */
    virtual void setV6Proxy(const boost::asio::ip::address_v4& v6ProxyAddress);

    /**
     * Set the Mac proxy address for the vswitch in transport mode.
     *
     * @param macProxyAddress
     */
    virtual void setMacProxy(const boost::asio::ip::address_v4& macProxyAddress);

    virtual void start();
    virtual void stop();
};

/** @} ofcore */

/**
 * \defgroup cmodb Managed Objects
 * Types for accessing and working with managed objects.
 */

/** @} cpp */

} /* namespace ofcore */
} /* namespace opflex */

#endif /* OPFLEX_CORE_OFFRAMEWORK_H */
