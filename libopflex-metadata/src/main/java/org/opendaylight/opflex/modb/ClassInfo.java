/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package org.opendaylight.opflex.modb;

import java.util.*;

public class ClassInfo {

    /**
     * The type of an MO in the Opflex protocol.  Updates to these MOs
     * will trigger different operations depending on their types.
     */
    public enum ClassType {
        /**
         * An MO describing a configured policy that describes some
         * user intent.  These objects are owned by the policy
         * repository.
         */
        POLICY,
        /**
         * An MO describing a policy enforcement endpoint that
         * resolved from the endpoint registry.  These objects are
         * owned by the endpoint registry and ultimately the remote
         * policy element.
         */
        REMOTE_ENDPOINT,
        /**
         * An MO describing a policy enforcement endpoint that must be
         * registered in the endpoint registry.  These objects are
         * owned and maintained by the local policy element.
         */
        LOCAL_ENDPOINT,
        /**
         * An MO containing information that is reported to the
         * observer.  This could include health information, faults
         * and other status, and statistics related to its parent MO.
         */
        OBSERVABLE,
        /**
         * An MO that exists only locally and will not be transmitted
         * over the OpFlex protocol.
         */
        LOCAL_ONLY,
        /**
         * A special managed object that will be created by the
         * framework and will contain state about pending managed
         * object resolutions
         */
        RESOLVER,
        /**
         * A type that represents a relationship between two managed
         * objects.  This type would contain a target reference
         * property that will allow the user to resolve related
         * managed objects.
         */
        RELATIONSHIP,
        /**
         * A reverse relationship works just like a relationship
         * except that it represents the reverse direction.  Though a
         * reverse relationship contains a reference to the referring
         * object, this reference will not be automatically resolved.
         */
        REVERSE_RELATIONSHIP
    }

    private final long classId;
    private final ClassType type;
    private final String name;
    private final String owner;
    private final List<PropertyInfo> properties;
    private final List<Long> namingProperties;

    public ClassInfo(long classId, ClassType type, String name, String owner,
                     List<PropertyInfo> properties, List<Long> namingProperties) {
        this.classId = classId;
        this.type = type;
        this.name = name;
        this.owner = owner;
        this.properties = new ArrayList<>(properties);
        this.namingProperties = new ArrayList<>(namingProperties);
    }

    public long getClassId() {
        return classId;
    }

    public ClassType getType() {
        return type;
    }

    public String getName() {
        return name;
    }

    public String getOwner() {
        return owner;
    }

    public List<PropertyInfo> getProperties() {
        return Collections.unmodifiableList(properties);
    }

    public List<Long> getNamingProperties() {
        return Collections.unmodifiableList(namingProperties);
    }
}
