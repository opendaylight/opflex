/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package org.opendaylight.opflex.modb;

public class URIBuilder {

    private final StringBuilder uri;

    public URIBuilder() {
        this.uri = new StringBuilder("/");
    }

    public URIBuilder(URI uri) {
        this.uri = new StringBuilder(uri.toString());
    }

    public URIBuilder addElement(String path) {
        uri.append(path);
        uri.append('/');
        return this;
    }

    public URIBuilder addElement(Long path) {
        uri.append(path);
        uri.append('/');
        return this;
    }

    public URI build() {
        return new URI(uri.toString());
    }
}
