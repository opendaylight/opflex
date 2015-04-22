/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package org.opendaylight.opflex.modb;

public class PropertyInfo {

    public enum Type {
        /** A composite property */
        COMPOSITE,
        /** A string-valued property containing a class ID and URI
         which is a reference to another location in the management
         information tree. */
        REFERENCE,
        /** A string-valued property */
        STRING,
        /** A signed 64-bit integer value */
        S64,
        /** An unsigned 64-bit integer value */
        U64,
        /** A MAC address */
        MAC,
        /** An 8-bit enum value */
        ENUM8,
        /** A 16-bit enum value */
        ENUM16,
        /** A 32-bit enum value */
        ENUM32,
        /** A 64-bit enum value */
        ENUM64
    }

    public enum Cardinality {
        /** A scalar-valued property */
        SCALAR,
        /** A vector-valued property */
        VECTOR
    }

    private final long id;
    private final String name;
    private final Type type;
    private final long classId;
    private final Cardinality cardinality;
    private final EnumInfo enumInfo;

    public PropertyInfo(long id, String name, Type type, Cardinality cardinality) {
        this.id = id;
        this.name = name;
        this.type = type;
        this.classId = 0;
        this.cardinality = cardinality;
        this.enumInfo = null;
    }

    public PropertyInfo(long id, String name, Type type, Cardinality cardinality, EnumInfo enumInfo) {
        this.id = id;
        this.name = name;
        this.type = type;
        this.classId = 0;
        this.cardinality = cardinality;
        this.enumInfo = enumInfo;
    }

    public PropertyInfo(long id, String name, Type type, long classId, Cardinality cardinality) {
        this.id = id;
        this.name = name;
        this.type = type;
        this.classId = classId;
        this.cardinality = cardinality;
        this.enumInfo = null;
    }

    public long getId() {
        return id;
    }

    public String getName() {
        return name;
    }

    public Type getType() {
        return type;
    }

    public long getClassId() {
        return classId;
    }

    public Cardinality getCardinality() {
        return cardinality;
    }
}
