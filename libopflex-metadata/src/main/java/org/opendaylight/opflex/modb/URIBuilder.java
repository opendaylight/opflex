/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package org.opendaylight.opflex.modb;

import java.nio.charset.StandardCharsets;

public class URIBuilder {

    private final StringBuilder uri;

    public URIBuilder() {
        this.uri = new StringBuilder("/");
    }

    public URIBuilder(URI uri) {
        this.uri = new StringBuilder(uri.toString());
    }

    public URIBuilder addElement(String path) {
        uri.append(escape(path));
        uri.append('/');
        return this;
    }

    public URIBuilder addElement(Long path) {
        uri.append(path);
        uri.append('/');
        return this;
    }

    public URIBuilder addElement(MAC mac) {
        uri.append(escape(mac.toString()));
        uri.append('/');
        return this;
    }

    public URIBuilder addElement(URI uri) {
        this.uri.append(escape(uri.toString()));
        this.uri.append('/');
        return this;
    }

    public URI build() {
        return new URI(uri.toString());
    }

    private String escape(String input) {
        StringBuilder result = new StringBuilder();
        for (byte b1 : input.getBytes(StandardCharsets.UTF_8)) {
            if (Character.isLetterOrDigit(b1) || b1 == '-' || b1 == '_' || b1 == '.' || b1 == '~') {
                result.append((char)b1);
            } else {
                result.append('%').append(String.format("%02x", (int) b1));
            }
        }
        return result.toString();
    }
}
