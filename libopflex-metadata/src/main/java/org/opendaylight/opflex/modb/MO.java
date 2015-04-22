/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package org.opendaylight.opflex.modb;

import java.util.*;

/*
 * Instances of this class should not be accessed from multiple threads concurrently
 */
public class MO {

    private final Map<Long, Object> properties = new HashMap<>();
    private final List<URI> children = new ArrayList<>();

    protected Object getProperty(long propId) {
        return properties.get(propId);
    }

    protected void setProperty(long propId, Object value) {
        properties.put(propId, value);
    }

    protected boolean isPropertySet(long propId) {
        return properties.containsKey(propId);
    }

    protected MO addChild(long parentPropId, long childClassId, URI childUri) {
        MO child = new MO(childClassId, childUri, getClassId(), getURI(), parentPropId);
        addChild(childUri);
        return child;
    }

    protected void addChild(URI childUri) {
        children.add(childUri);
    }

    public List<URI> getChildren() {
        return Collections.unmodifiableList(children);
    }

    public MO(long classId, URI uri, long parentClassId, URI parentUri, long parentPropId) {
        this.classId = classId;
        this.uri = uri;
        this.parentClassId = parentClassId;
        this.parentUri = parentUri;
        this.parentPropId = parentPropId;
    }

    private final long classId;
    private final URI uri;
    private final long parentClassId;
    private final URI parentUri;
    private final long parentPropId;

    public long getClassId() {
        return classId;
    }

    public URI getURI() {
        return uri;
    }

    public long getParentClassId() {
        return parentClassId;
    }

    public URI getParentUri() {
        return parentUri;
    }

    public long getParentPropId() {
        return parentPropId;
    }

    public static final class Reference {

        public final long classId;
        public final URI uri;

        public Reference(long classId, URI uri) {
            this.classId = classId;
            this.uri = uri;
        }
    }
}
