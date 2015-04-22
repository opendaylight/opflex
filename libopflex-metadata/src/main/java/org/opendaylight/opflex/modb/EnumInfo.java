/*
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package org.opendaylight.opflex.modb;

import java.util.Arrays;
import java.util.List;

public class EnumInfo {

    private final String name;
    private final List<ConstInfo> consts;

    public EnumInfo(String name, ConstInfo ... consts) {
        this.name = name;
        this.consts = Arrays.asList(consts);
    }

    public String getName() {
        return name;
    }

    public List<ConstInfo> getConsts() {
        return consts;
    }
}
