/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package org.opendaylight.opflex.modb;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

public class ModelMetadata {

    private final String name;
    private final List<ClassInfo> classes;
    private static final Map<String, ModelMetadata> MODELS = new ConcurrentHashMap<>();

    private ModelMetadata(String name, ClassInfo ... classes) {
        this.name = name;
        this.classes  = Arrays.asList(classes);
    }

    public String getName() {
        return name;
    }

    public List<ClassInfo> getClasses() {
        return Collections.unmodifiableList(classes);
    }

    public static void addMetadata(String name, ClassInfo ... classes) {
        ModelMetadata metadata = new ModelMetadata(name, classes);
        MODELS.put(name, metadata);
    }

    public static ModelMetadata getMetadata(String name) {
        return MODELS.get(name);
    }
}
