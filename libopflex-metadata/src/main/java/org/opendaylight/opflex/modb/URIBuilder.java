/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package org.opendaylight.opflex.modb;

import java.nio.charset.Charset;

public class URIBuilder {

    private final StringBuilder uri;
    private static final Charset UTF8_CHARSET = Charset.forName("UTF-8");

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
        for (byte b : input.getBytes(UTF8_CHARSET)) {
            if (Character.isLetterOrDigit(b) || b == '-' || b == '_' || b == '.' || b == '~') {
                result.append((char)b);
            } else {
                result.append('%').append(String.format("%02x", (int) b));
            }
        }
        return result.toString();
    }
}
