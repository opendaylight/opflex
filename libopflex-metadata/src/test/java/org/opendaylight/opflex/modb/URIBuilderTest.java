/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package org.opendaylight.opflex.modb;

import org.junit.Assert;
import org.junit.Test;

public class URIBuilderTest {

    @Test
    public void testRoot() {
        URIBuilder builder = new URIBuilder();
        Assert.assertEquals("/", builder.build().toString());
    }

    @Test
    public void testInts() {
        URIBuilder builder = new URIBuilder();
        builder.addElement("prop1").addElement(75L).addElement(-75L);
        Assert.assertEquals("/prop1/75/-75/", builder.build().toString());
    }

    @Test
    public void testString() {
        URIBuilder builder = new URIBuilder();
        builder.addElement("prop2").addElement("test");
        Assert.assertEquals("/prop2/test/", builder.build().toString());

        builder = new URIBuilder();
        builder.addElement("prop3")
               .addElement("t est")
               .addElement("prop4")
               .addElement("t\"est");
        Assert.assertEquals("/prop3/t%20est/prop4/t%22est/", builder.build().toString());

        builder = new URIBuilder();
        builder.addElement("prop3")
               .addElement(".-~_");
        Assert.assertEquals("/prop3/.-~_/", builder.build().toString());

        builder = new URIBuilder();
        builder.addElement("prop3")
               .addElement(",./<>?;':\"[]\\{}|~!@#$%^&*()_-+=/");
        Assert.assertEquals("/prop3/%2c.%2f%3c%3e%3f%3b%27%3a%22%5b%5d%5c%7b" +
                            "%7d%7c~%21%40%23%24%25%5e%26%2a%28%29_-%2b%3d%2f/",
                            builder.build().toString());
    }

    @Test
    public void testMac() {
        URIBuilder builder = new URIBuilder();
        builder.addElement("macprop")
               .addElement(new MAC("11:22:33:44:55:66"));
        Assert.assertEquals("/macprop/11%3a22%3a33%3a44%3a55%3a66/", builder.build().toString());
    }

    @Test
    public void testUri() {
        URIBuilder builder = new URIBuilder();
        builder.addElement("uriprop")
               .addElement(new URI("/macprop/11%3a22%3a33%3a44%3a55%3a66/"));
        Assert.assertEquals("/uriprop/%2fmacprop%2f11%253a22%253a33%253a44%253a55%253a66%2f/", builder.build().toString());

        builder = new URIBuilder(builder.build());
        Assert.assertEquals("/uriprop/%2fmacprop%2f11%253a22%253a33%253a44%253a55%253a66%2f/", builder.build().toString());
    }
}
